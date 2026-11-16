#pragma once
// Learned Online broad-phase method for heterogeneous material AABBs.
//
// This is a *safe policy selector* integrated with the existing concrete
// algorithms. The MLP only ranks costs, while SAP/Global Guard/Selective
// Guard/RegionalCandidates still construct exact current collision pairs.
// Switching invalidates the destination cache, and its measured rebuild cost
// remains in the reported wall time. No oracle predictions may enter features.
#include "ccp/generic_online.hpp"
#include "ccp/learned_policy.hpp"
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <string>
#include <vector>
namespace ccp::bench {
class LearnedOnline {
 learned::Network model_;
 learned::Extractor features_;
 ccp::live::BroadPhaseService global_;
 ccp::research::SelectiveGuard selective_;
 RegionalCandidates regional_;
 double margin_{},penaltyMs_{},hysteresis_{};
 int cooldown_{},frame_{},lastSwitch_{-10000},active_{-1};
 bool startWithGuard_{};
 size_t switches_{};
 static bool overlap(const Bounds& a,const Bounds& b){
  return a.min.x<=b.max.x&&b.min.x<=a.max.x&&
         a.min.y<=b.max.y&&b.min.y<=a.max.y&&
         a.min.z<=b.max.z&&b.min.z<=a.max.z;
 }
 static bool eligible(const Bounds& a,const Bounds& b){
  return (a.mask&b.category)!=0&&(b.mask&a.category)!=0;
 }
 static std::vector<Bounds> padded(const std::vector<Bounds>& raw,double margin){
  std::vector<Bounds> output=raw;
  for(auto& b:output){
   const double m=margin*.5;
   b.min.x-=m;b.min.y-=m;b.min.z-=m;
   b.max.x+=m;b.max.y+=m;b.max.z+=m;
  }
  return output;
 }
public:
 LearnedOnline(const std::string& path,double margin=0.,
               double switchPenaltyMs=.03,double hysteresis=.12,
               int cooldownFrames=12,bool startWithGuard=false):
  model_(learned::Network::load(path)),
  selective_(margin,.045),
  margin_(margin),penaltyMs_(switchPenaltyMs),
  hysteresis_(hysteresis),cooldown_(cooldownFrames),startWithGuard_(startWithGuard){
  if(margin<0||!std::isfinite(margin)||!std::isfinite(penaltyMs_)||
     penaltyMs_<0||!std::isfinite(hysteresis_)||hysteresis_<0||
     hysteresis_>=1||cooldown_<0)
   throw std::invalid_argument("invalid learned policy control parameters");
 }
 // Snapshot-replay already owns validated Bounds. Do not allocate a second
 // Primitive vector, copy it back to Bounds, or pad a zero-margin scene.
 Result query(const std::vector<Bounds>& raw){return queryBounds(raw,nullptr);}
 // Preserve the original API and region/sheet metadata for coupled callers.
 Result query(const std::vector<Primitive>& input){
  return queryBounds(rawBounds(input),&input);
 }
private:
 Result queryBounds(const std::vector<Bounds>& raw,
                    const std::vector<Primitive>* original){
  const auto begin=Clock::now();
  ++frame_;
  std::vector<Bounds> expanded;
  if(margin_>0.)expanded=padded(raw,margin_);
  const std::vector<Bounds>& tight=margin_==0.?raw:expanded;
  // At tiny N the measured decision overhead can dominate the entire
  // guarded query. Deterministically use the safe guarded method rather
  // than paying for neural inference. This gate was developed AFTER
  // examining the first negative seed-101 test; all gains MUST be checked
  // on previously unseen seeds, never retroactively on that test set.
  constexpr size_t kSmallSceneCutoff=384;
  const bool bypass=tight.size()<kSmallSceneCutoff;
  const auto features=bypass?learned::Features{}:features_.describe(tight);
  const auto featureEnd=Clock::now();
  // While a cooldown forbids changing backend, still update causal
  // geometry history but do NOT pay for ignored neural inference.
  const bool decisionDue=!bypass && !(active_<0&&startWithGuard_) &&
                         (active_<0||frame_-lastSwitch_>=cooldown_);
  const auto predicted=decisionDue?model_.predict(features):
                                    std::array<double,learned::kActions>{};
  const auto inferenceEnd=Clock::now();
  int selected=bypass?1:active_;
  if(!bypass){
   if(selected<0){
    selected=startWithGuard_?1:int(std::min_element(predicted.begin(),predicted.end())-predicted.begin());
   }else if(frame_-lastSwitch_>=cooldown_){
    const int fastest=int(std::min_element(predicted.begin(),predicted.end())-predicted.begin());
    if(fastest!=active_ && predicted[size_t(fastest)]+penaltyMs_<
        predicted[size_t(active_)]*(1.-hysteresis_))
     selected=fastest;
   }
  }
  Result result;result.span=64;result.usedGlobal=selected==1||selected==2;
  result.learnedBypassed=bypass;
  if(selected!=active_){
   // Any method resumed after a gap MUST reject stale cached certificates.
   // This reset is included in end-to-end online controller timing.
   if(selected==1)global_.invalidate();
   if(selected==2)selective_.reset();
   if(selected==3)regional_.reset();
   if(active_>=0)++switches_;
   result.switched=active_>=0;active_=selected;lastSwitch_=frame_;
  }
  const auto decisionEnd=Clock::now();
  if(selected==0){
   result.pairs=ccp::live::candidates(tight,true);
  }else if(selected==1){
   ccp::live::QueryMetrics m;
   const auto& cached=global_.query(raw,{margin_,.045,ccp::live::UpdatePolicy::GuardedReuse},true,m);
   result.pairs.reserve(cached.size());
   for(const auto p:cached){
    const auto& a=tight.at(size_t(p.a)),&b=tight.at(size_t(p.b));
    if(eligible(a,b)&&overlap(a,b))result.pairs.push_back(p);
   }
   result.guardBuilds=m.builds;result.guardReuses=m.reuses;
   result.aabbTests=m.pairTests;
  }else if(selected==2){
   ccp::research::SelectiveGuard::Metrics m;
   const auto& output=selective_.query(raw,m);
   result.pairs.assign(output.begin(),output.end());
   result.guardBuilds=m.fullBuilds;result.guardReuses=m.cacheReuses;
   result.selectiveRepairs=m.partialRepairs;result.aabbTests=m.pairChecks;
   result.workers=m.parallelWorkers;
  }else if(selected==3){
   Metrics m;
   std::vector<Primitive> actual;
   if(original)actual=*original;
   else{
    actual.reserve(raw.size());
    for(size_t i=0;i<raw.size();++i)
     actual.push_back({raw[i],int(i/64),-1});
   }
   for(size_t i=0;i<actual.size();++i)actual[i].region=int(i/64);
   const auto& output=regional_.query(actual,margin_,true,m,true);
   result.pairs.assign(output.begin(),output.end());
   result.builds=m.regionalPairRebuilds;result.reuses=m.regionalPairReuses;
   result.dirty=m.dirtyRegions;result.aabbTests=m.broadAabbTests;
  }else throw std::logic_error("ML selector returned unsupported action");
  const auto backendEnd=Clock::now();
  if(bypass)features_.reset();
  else features_.observe(tight,result.pairs.size());
  result.learnedAction=learned::kActionNames[size_t(selected)];
  const auto finished=Clock::now();
  result.mlFeatureMs=std::chrono::duration<double,std::milli>(featureEnd-begin).count();
  result.mlInferenceMs=std::chrono::duration<double,std::milli>(inferenceEnd-featureEnd).count();
  result.mlDecisionMs=std::chrono::duration<double,std::milli>(decisionEnd-inferenceEnd).count();
  result.mlBackendMs=std::chrono::duration<double,std::milli>(backendEnd-decisionEnd).count();
  result.mlBookkeepingMs=std::chrono::duration<double,std::milli>(finished-backendEnd).count();
  result.queryMs=std::chrono::duration<double,std::milli>(finished-begin).count();
  return result;
 }
public:
 size_t switches()const{return switches_;}
};
} // namespace ccp::bench
