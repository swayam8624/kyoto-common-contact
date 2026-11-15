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
               int cooldownFrames=12):
  model_(learned::Network::load(path)),
  selective_(margin,.045),
  margin_(margin),penaltyMs_(switchPenaltyMs),
  hysteresis_(hysteresis),cooldown_(cooldownFrames){
  if(margin<0||!std::isfinite(margin)||!std::isfinite(penaltyMs_)||
     penaltyMs_<0||!std::isfinite(hysteresis_)||hysteresis_<0||
     hysteresis_>=1||cooldown_<0)
   throw std::invalid_argument("invalid learned policy control parameters");
 }
 Result query(const std::vector<Primitive>& input){
  const auto begin=Clock::now();
  ++frame_;
  const auto raw=rawBounds(input);
  const auto tight=padded(raw,margin_);
  const auto features=features_.describe(tight);
  const auto predicted=model_.predict(features);
  int selected=active_;
  if(selected<0){
   selected=int(std::min_element(predicted.begin(),predicted.end())-predicted.begin());
  }else if(frame_-lastSwitch_>=cooldown_){
   const int fastest=int(std::min_element(predicted.begin(),predicted.end())-predicted.begin());
   if(fastest!=active_ && predicted[size_t(fastest)]+penaltyMs_<
       predicted[size_t(active_)]*(1.-hysteresis_))
    selected=fastest;
  }
  Result result;result.span=64;result.usedGlobal=selected==1||selected==2;
  if(selected!=active_){
   // Any method resumed after a gap MUST reject stale cached certificates.
   // This reset is included in end-to-end online controller timing.
   if(selected==1)global_.invalidate();
   if(selected==2)selective_.reset();
   if(selected==3)regional_.reset();
   if(active_>=0)++switches_;
   result.switched=active_>=0;active_=selected;lastSwitch_=frame_;
  }
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
   auto actual=input;
   for(size_t i=0;i<actual.size();++i)actual[i].region=int(i/64);
   const auto& output=regional_.query(actual,margin_,true,m,true);
   result.pairs.assign(output.begin(),output.end());
   result.builds=m.regionalPairRebuilds;result.reuses=m.regionalPairReuses;
   result.dirty=m.dirtyRegions;result.aabbTests=m.broadAabbTests;
  }else throw std::logic_error("ML selector returned unsupported action");
  features_.observe(tight,result.pairs.size());
  result.learnedAction=learned::kActionNames[size_t(selected)];
  result.queryMs=std::chrono::duration<double,std::milli>(Clock::now()-begin).count();
  return result;
 }
 size_t switches()const{return switches_;}
};
} // namespace ccp::bench
