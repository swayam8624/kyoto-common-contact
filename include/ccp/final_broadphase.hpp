#pragma once
// Kyoto validated exact broad-phase selector — public reusable C++23 API.
//
// Frozen semantics: the two production candidates below are extracted from
// the previously measured KYOTO_REPLAY_ABLATION_V4 implementation without
// changing the decision thresholds or their cache/update behavior.
// This is a CPU current-AABB candidate generator, NOT continuous collision
// detection and NOT a coupled deformable solver integration.
//
// Input: stable ordered primitive IDs, finite AABBs, category/mask filtering.
// Every returned pair is sorted, unique and inclusive-AABB overlapping.
// Thread safety: one stateful controller instance per independent stream.
// Do NOT reuse the same controller across unrelated scenes without reset.
//
// Verified benchmark label: guard_sap_bounded_trial.
// The original ML selector is retained as a negative/comparison baseline;
// this champion does not require a learned model.
#include "ccp/live_cloth.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

namespace ccp::final_broadphase {
using Bounds=ccp::live::Bounds;
using Pair=ccp::live::Pair;
using Pairs=std::vector<Pair>;
using Clock=std::chrono::steady_clock;

inline bool overlap(const Bounds& a,const Bounds& b){
 return a.min.x<=b.max.x&&b.min.x<=a.max.x&&
        a.min.y<=b.max.y&&b.min.y<=a.max.y&&
        a.min.z<=b.max.z&&b.min.z<=a.max.z;
}
inline Pairs exactGuard(const Pairs& candidate,const std::vector<Bounds>& boxes){
 Pairs out;out.reserve(candidate.size());
 for(auto p:candidate)
  if(overlap(boxes.at(size_t(p.a)),boxes.at(size_t(p.b))))out.push_back(p);
 return out;
}

// Exact temporally coherent persistent SAP baseline.
class PersistentSAP {
 std::vector<int> order_;
 std::vector<int> ids_;
public:
 Pairs query(const std::vector<Bounds>& boxes){
  const size_t n=boxes.size();
  if(ids_.size()!=n){
   ids_.clear();ids_.reserve(n);order_.resize(n);
   for(size_t i=0;i<n;++i){ids_.push_back(boxes[i].id);order_[i]=int(i);}
  }else{
   for(size_t i=0;i<n;++i)
    if(ids_[i]!=boxes[i].id)
     throw std::runtime_error("persistent SAP requires stable ordered primitive IDs");
  }
  const auto less=[&](int ai,int bi){
   const auto& a=boxes[size_t(ai)],&b=boxes[size_t(bi)];
   if(a.min.x!=b.min.x)return a.min.x<b.min.x;
   return a.id<b.id;
  };
  // Initially sorted order is unknown. Later, use bounded insertion repair.
  // Bound the number of shifts by 8N; after that fall back to full sorting.
  const size_t shiftCap=n>std::numeric_limits<size_t>::max()/8?
                         std::numeric_limits<size_t>::max():n*8;
  size_t shifts=0;
  bool exceeded=false;
  for(size_t i=1;i<n;++i){
   const int item=order_[i];
   size_t j=i;
   while(j>0&&less(item,order_[j-1])){
    order_[j]=order_[j-1];--j;
    if(++shifts>shiftCap){exceeded=true;break;}
   }
   order_[j]=item;
   if(exceeded)break;
  }
  if(exceeded)
   std::sort(order_.begin(),order_.end(),less);
  Pairs out;
  for(size_t i=0;i<n;++i){
   const auto& a=boxes[size_t(order_[i])];
   for(size_t j=i+1;j<n;++j){
    const auto& b=boxes[size_t(order_[j])];
    if(b.min.x>a.max.x)break;
    if((a.mask&b.category)==0||(b.mask&a.category)==0)continue;
    if(overlap(a,b))out.push_back({std::min(a.id,b.id),std::max(a.id,b.id)});
   }
  }
  std::sort(out.begin(),out.end());
  out.erase(std::unique(out.begin(),out.end()),out.end());
  return out;
 }
};

// Best validated online selector for standalone CPU AABB replay.
class GuardSAPBoundedTrial {
 ccp::live::BroadPhaseService guard_;
 PersistentSAP sap_;
 enum class Mode { Guard, Trial, Sap };
 Mode mode_{Mode::Guard};
 int frame_{},guardRebuildStreak_{},trialFrames_{},sapFrames_{};
 int quietFrames_{},sapBadFrames_{},cooldown_{};
 double guardEWMA_{},sapEWMA_{};
 bool guardSeen_{},sapSeen_{},lastActionSAP_{};
 std::array<Bounds,24> lastSample_{};
 bool sampleValid_{};
 static bool moved(const Bounds& a,const Bounds& b){
  // A fraction of the guarded-displacement budget, in the same world units.
  constexpr double d=.01125;
  return std::abs(a.min.x-b.min.x)>d ||
         std::abs(a.min.y-b.min.y)>d ||
         std::abs(a.min.z-b.min.z)>d ||
         std::abs(a.max.x-b.max.x)>d ||
         std::abs(a.max.y-b.max.y)>d ||
         std::abs(a.max.z-b.max.z)>d;
 }
 bool motionQuiet(const std::vector<Bounds>& boxes){
  const size_t count=std::min(lastSample_.size(),boxes.size());
  size_t moving=0;
  for(size_t i=0;i<count;++i){
   const auto index=count==1?size_t{0}:
      i*(boxes.size()-1)/(count-1);
   const auto& b=boxes[index];
   if(sampleValid_&&moved(lastSample_[i],b))++moving;
   lastSample_[i]=b;
  }
  const bool hadHistory=sampleValid_;
  sampleValid_=true;
  // Require at least five-sixths of sampled elements to move very little.
  return hadHistory&&moving*6<=count;
 }
public:
 struct Outcome {
  Pairs pairs;
  const char* action{};
  bool switched{},trialFrame{};
 };
 Outcome query(const std::vector<Bounds>& boxes){
  ++frame_;
  if(cooldown_>0)--cooldown_;
  bool actionSAP=mode_!=Mode::Guard;
  Pairs result;
  bool trialFrame=mode_==Mode::Trial;
  if(!actionSAP){
   ccp::live::QueryMetrics metrics;
   const auto began=Clock::now();
   const auto& candidates=guard_.query(
      boxes,{0.,.045,ccp::live::UpdatePolicy::GuardedReuse},true,metrics);
   result=exactGuard(candidates,boxes);
   const auto elapsed=std::chrono::duration<double,std::milli>(
                          Clock::now()-began).count();
   guardEWMA_=guardSeen_?guardEWMA_*.7+elapsed*.3:elapsed;
   guardSeen_=true;
   if(metrics.builds && elapsed>=.25)
    ++guardRebuildStreak_;
   else guardRebuildStreak_=0;
   // Require several costly actual rebuilds, not one outlier.
   // No extra algorithm is executed on this decision frame.
   if(boxes.size()>=384 && cooldown_==0 &&
      guardRebuildStreak_>=3 && guardEWMA_>=.35){
    mode_=Mode::Trial;
    trialFrames_=0;quietFrames_=0;sapBadFrames_=0;
    sapEWMA_=0.;sapSeen_=false;
    sampleValid_=false;
    guardRebuildStreak_=0;
   }
  }else{
   const auto began=Clock::now();
   result=sap_.query(boxes);
   const auto elapsed=std::chrono::duration<double,std::milli>(
                         Clock::now()-began).count();
   sapEWMA_=sapSeen_?sapEWMA_*.7+elapsed*.3:elapsed;
   sapSeen_=true;
   if(mode_==Mode::Trial){
    ++trialFrames_;
    // Exclude the very first SAP initialization cost from the comparison.
    // A clearly losing SAP trial terminates after two SAP frames.
    if(trialFrames_>=2 && sapEWMA_>guardEWMA_*1.2){
     mode_=Mode::Guard;
     cooldown_=12;guardRebuildStreak_=0;sampleValid_=false;
    }else if(trialFrames_>=8){
     if(sapEWMA_<guardEWMA_*.85){
      mode_=Mode::Sap;sapFrames_=0;quietFrames_=0;sapBadFrames_=0;
     }else{
      mode_=Mode::Guard;cooldown_=12;
      guardRebuildStreak_=0;sampleValid_=false;
     }
    }
   }else{
    ++sapFrames_;
    if(motionQuiet(boxes))++quietFrames_;
    else quietFrames_=0;
    if(sapEWMA_>guardEWMA_*1.2)++sapBadFrames_;
    else sapBadFrames_=0;
    // The historical Guard EWMA is only a warning signal, NEVER a
    // hypothetical "warm Guard" runtime claim. Returning to Guard
    // incurs any real cache refresh cost on the next query.
    if(quietFrames_>=6 || sapBadFrames_>=6){
     mode_=Mode::Guard;cooldown_=12;
     guardRebuildStreak_=0;sampleValid_=false;
    }
   }
  }
  const bool changed=frame_>1&&actionSAP!=lastActionSAP_;
  lastActionSAP_=actionSAP;
  return {std::move(result),
          actionSAP?"sap_persistent_coherent":"global_guarded_exact",
          changed,trialFrame};
 }
};
} // namespace ccp::final_broadphase
