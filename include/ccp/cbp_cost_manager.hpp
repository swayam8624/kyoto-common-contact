#pragma once
// Kyoto CBP Phase 3 (EXPERIMENTAL): causal whole-cross-join cost router.
//
// Selects ONE exact RegionRouter cross-mode per query. Does not measure
// inactive backends, consult future trajectories, or use material labels.
// The first routing signal is a small deterministic snapshot AABB sample;
// subsequent decisions also use realized maintenance/call-cost telemetry.
// All switches reset dormant state, so cold-start and migration costs are
// included in the timed public query boundary.
//
// Research boundary: this is whole cross-region routing, NOT yet a per-patch
// multi-kernel orchestrator. Correctness comes only from exact constituent
// backends. Switching heuristics can affect time, never candidate coverage.
#include "ccp/cbp_region_router.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <vector>

namespace ccp::cbp {
class CausalCostRouter {
 using Clock=std::chrono::steady_clock;
 static constexpr std::size_t kSample=16;
 struct Record {
  double ewmaMs{};
  std::size_t observations{},lastFrame{};
 };
 RegionRouter router_;
 std::array<Bounds,kSample> previousSample_{};
 std::size_t previousSampleN_{},previousBoxN_{};
 std::array<std::array<Record,3>,3> records_{};
 bool haveSample_{},initialized_{};
 CrossMode selected_{CrossMode::CertifiedCache};
 CrossMode pending_{CrossMode::CertifiedCache};
 std::size_t frame_{},pendingFrames_{},cooldown_{},switches_{};
 double meanMigrationMs_{};
 Telemetry previousWork_{};
 static std::size_t modeIndex(CrossMode mode){
  switch(mode){
   case CrossMode::Exhaustive:return 0;
   case CrossMode::SharedBVH:return 1;
   case CrossMode::CertifiedCache:return 2;
  }
  throw std::logic_error("Unknown CBP mode");
 }
 static int bucket(double density){
  return density>=.55?2:density>=.08?1:0;
 }
 static double ms(Clock::time_point start,Clock::time_point end){
  return std::chrono::duration<double,std::milli>(end-start).count();
 }
 static bool moving(const Bounds& a,const Bounds& b){
  constexpr double threshold=.015;
  return a.category!=b.category||a.mask!=b.mask||a.movable!=b.movable||
         std::abs(a.min.x-b.min.x)>threshold||
         std::abs(a.min.y-b.min.y)>threshold||
         std::abs(a.min.z-b.min.z)>threshold||
         std::abs(a.max.x-b.max.x)>threshold||
         std::abs(a.max.y-b.max.y)>threshold||
         std::abs(a.max.z-b.max.z)>threshold;
 }
 public:
 struct Decision {
  CrossMode chosen{CrossMode::CertifiedCache};
  double sampledDensity{},sampledMotion{},decisionMs{},
         migrationMs{},backendMs{},totalMs{};
  std::size_t densitySamples{},switchedTotal{},potentialPairs{},dirty{};
  bool switched{},coldStart{},forcedEscape{};
 };
 private:
 Decision last_{};
 public:
 static const char* name(CrossMode mode){
  switch(mode){
   case CrossMode::Exhaustive:return "exhaustive";
   case CrossMode::SharedBVH:return "shared_bvh";
   case CrossMode::CertifiedCache:return "certified_cache";
  }
  return "unknown";
 }
 const Decision& decision()const{return last_;}
 Telemetry backendWork()const{return router_.telemetry();}
 void setSapThreshold(std::size_t n){router_.setSapThreshold(n);}
 void setCachePadding(double padding){router_.setCachePadding(padding);}
 void reset(){
  router_.reset();
  previousSampleN_=previousBoxN_=0;
  haveSample_=initialized_=false;
  records_={};last_={};previousWork_={};
  selected_=pending_=CrossMode::CertifiedCache;
  pendingFrames_=frame_=cooldown_=switches_=0;
  meanMigrationMs_=0.;
 }

 Pairs query(const std::vector<Bounds>& boxes,const std::vector<int>& labels){
  const auto start=Clock::now();
  if(boxes.size()!=labels.size())
   throw std::invalid_argument("Causal router requires one label per bound");
  if(boxes.size()>static_cast<std::size_t>(std::numeric_limits<int>::max()))
   throw std::length_error("Causal router exceeds indexed-primitive limit");

  // Stratified, deterministic sample of current IDs. No material labels,
  // hidden replay data, future frames or inactive-method timing involved.
  const std::size_t count=std::min(kSample,boxes.size());
  std::array<Bounds,kSample> sample{};
  std::array<int,kSample> sampledLabels{};
  for(std::size_t k=0;k<count;++k){
   const auto index=count==1?std::size_t{0}:
     k*(boxes.size()-1)/(count-1);
   sample[k]=boxes[index];
   sampledLabels[k]=labels[index];
  }
  std::size_t eligiblePairs=0,hits=0;
  for(std::size_t i=0;i<count;++i)
   for(std::size_t j=i+1;j<count;++j){
    if(sampledLabels[i]==sampledLabels[j])continue;
    if(!compatible(sample[i],sample[j]))continue;
    ++eligiblePairs;
    if(overlap(sample[i],sample[j]))++hits;
   }
  const double density=eligiblePairs?double(hits)/double(eligiblePairs):0.;
  std::size_t movingCount=0;
  if(haveSample_ && previousBoxN_==boxes.size() && previousSampleN_==count)
   for(std::size_t k=0;k<count;++k)
    if(moving(sample[k],previousSample_[k]))++movingCount;
  const double motion=haveSample_&&previousBoxN_==boxes.size()&&count?
    double(movingCount)/double(count):0.;

  // These thresholds are provisional engineering priors, not statistically
  // trained optimal boundaries or a claimed research novelty.
  CrossMode preferred=CrossMode::CertifiedCache;
  const bool saturated=boxes.size()<=24||density>=.55;
  // Local observation of LAST frame's paid certificate maintenance + this
  // frame's already-known geometry. No alternative backend is executed.
  const bool dirtyOverload=initialized_ &&
    selected_==CrossMode::CertifiedCache && boxes.size()>=64 &&
    previousWork_.cacheDirtyPrimitives>boxes.size()/3 &&
    motion>=.35;
  if(saturated)preferred=CrossMode::Exhaustive;
  else if(density>=.18 || motion>=.35 ||
          (initialized_ && selected_==CrossMode::CertifiedCache &&
           previousWork_.cacheDirtyPrimitives>boxes.size()/3) ||
          (initialized_ && selected_==CrossMode::CertifiedCache &&
           previousWork_.cachePotentialPairs>boxes.size()*12))
   preferred=CrossMode::SharedBVH;

  const int cell=bucket(density);
  ++frame_;
  bool switchNow=false,cold=!initialized_;
  if(!initialized_){
   selected_=preferred;
   pending_=preferred;
   initialized_=true;
   pendingFrames_=0;
  }else if(preferred!=selected_){
   // High density may trigger a quadratic potential graph; a hard safety
   // escape overrides hysteresis, but still pays full cold-start cost.
   if(preferred==pending_)++pendingFrames_;
   else{pending_=preferred;pendingFrames_=1;}
   const bool immediate=(saturated&&preferred==CrossMode::Exhaustive)||
     (dirtyOverload&&preferred==CrossMode::SharedBVH);
   if(immediate||(pendingFrames_>=3&&cooldown_==0)){
    bool allow=true;
    const auto& target=records_[modeIndex(preferred)][static_cast<std::size_t>(cell)];
    const auto& current=records_[modeIndex(selected_)][static_cast<std::size_t>(cell)];
    if(!immediate&&target.observations>=3&&current.observations>=3&&
       frame_-target.lastFrame<48&&frame_-current.lastFrame<48){
     // Reuse ONLY actual past whole-call times, including selector overhead.
     // Require predicted gain over a 16-frame horizon to offset cold reset.
     constexpr double horizon=16.;
     allow=(current.ewmaMs-target.ewmaMs)*horizon >
            meanMigrationMs_ + .02; // 20us switching uncertainty reserve
    }
    if(allow)switchNow=true;
   }
  }else{
   pending_=selected_;
   pendingFrames_=0;
  }
  if(cooldown_)--cooldown_;
  const auto decided=Clock::now();
  double migration=0.;
  if(switchNow){
   const auto before=Clock::now();
   router_.reset(); // Never reuse dormant stale certificates or warm indices.
   migration=ms(before,Clock::now());
   meanMigrationMs_=switches_?
     .75*meanMigrationMs_+.25*migration:migration;
   selected_=preferred;
   ++switches_;
   cooldown_=6;
   pending_=selected_;
   pendingFrames_=0;
  }
  router_.setCrossMode(selected_);
  const auto backendStart=Clock::now();
  // Delegated exactness also rejects non-finite bounds and unstable IDs.
  auto pairs=router_.query(boxes,labels);
  const auto end=Clock::now();
  const auto work=router_.telemetry();
  const double total=ms(start,end);
  auto& observed=records_[modeIndex(selected_)][static_cast<std::size_t>(cell)];
  observed.ewmaMs=observed.observations?
     .8*observed.ewmaMs+.2*total:total;
  ++observed.observations;
  observed.lastFrame=frame_;

  for(std::size_t i=0;i<count;++i)previousSample_[i]=sample[i];
  previousSampleN_=count;
  previousBoxN_=boxes.size();
  haveSample_=true;
  previousWork_=work;
  last_={selected_,density,motion,ms(start,decided),migration,
         ms(backendStart,end),total,eligiblePairs,switches_,
         work.cachePotentialPairs,work.cacheDirtyPrimitives,switchNow,cold,
         switchNow&&dirtyOverload};
  return pairs;
 }
};
} // namespace ccp::cbp
