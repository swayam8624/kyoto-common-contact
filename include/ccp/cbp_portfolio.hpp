#pragma once
// Kyoto CBP Phase 4: exact V4/Guard fallback plus bounded causal
// single-backend regional trials. This deliberately conservative reference
// controller NEVER runs the unchosen backend on a frame.
//
// Scientific scope: WHOLE-SCENE routing between frozen V4 and one selected
// regional kernel. It is not yet topology-local multi-method delegation.
// Cold starts, failed probes and migration all count in the public query time.
// Thresholds are DEVELOPMENT priors and must be evaluated on untouched scenes.
#include "ccp/cbp_region_router.hpp"
#include "ccp/final_broadphase.hpp"
#include <algorithm>
#include <chrono>
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace ccp::cbp {

enum class PortfolioMode {
 FrozenV4, RegionalExhaustive, RegionalSharedBVH, RegionalCertifiedCache
};

class CbpPortfolio {
 using Clock=std::chrono::steady_clock;
 using V4=ccp::final_broadphase::GuardSAPBoundedTrial;
 V4 v4_{};
 RegionRouter regional_{};
 PortfolioMode active_{PortfolioMode::FrozenV4};
 PortfolioMode trialMode_{PortfolioMode::RegionalExhaustive};
 PortfolioMode returnMode_{PortfolioMode::FrozenV4};
 std::size_t frames_{},activeFrames_{},nextProbe_{12},lastN_{};
 std::size_t switches_{},trials_{},accepted_{},rejected_{};
 std::size_t trialFrames_{},aboveTolerance_{};
 bool initialized_{},trialActive_{},returnNext_{};
 double activeEwma_{},trialTotalMs_{},referenceCostMs_{};
 static constexpr std::size_t kWarmup=12,kTrialLength=4,kCooldown=48;
 static constexpr double kPromote=0.85; // 15% margin for measurement uncertainty
 struct Internal {
  const char* v4Action{"none"};
  Pairs pairs;
 };
 static double elapsed(Clock::time_point a,Clock::time_point b){
  return std::chrono::duration<double,std::milli>(b-a).count();
 }
 static CrossMode regionalMode(PortfolioMode mode){
  switch(mode){
   case PortfolioMode::RegionalExhaustive:return CrossMode::Exhaustive;
   case PortfolioMode::RegionalSharedBVH:return CrossMode::SharedBVH;
   case PortfolioMode::RegionalCertifiedCache:return CrossMode::CertifiedCache;
   case PortfolioMode::FrozenV4:break;
  }
  throw std::logic_error("Frozen V4 is not a regional cross kernel");
 }
 void activate(PortfolioMode target){
  if(target==active_)return;
  if(target==PortfolioMode::FrozenV4){
   // No dormant hot Guard/SAP cache is assumed correct or free.
   v4_=V4{};
  }else{
   // Rebuild on first use, including local SAP and cross graph/index state.
   regional_.reset();
   regional_.setCrossMode(regionalMode(target));
  }
  active_=target;
  activeFrames_=0;
  activeEwma_=0.;
  ++switches_;
 }
 Internal dispatch(const std::vector<Bounds>& boxes,
                   const std::vector<int>& labels){
  if(active_==PortfolioMode::FrozenV4){
   auto outcome=v4_.query(boxes);
   return {outcome.action,std::move(outcome.pairs)};
  }
  return {"none",regional_.query(boxes,labels)};
 }
public:
 struct Decision {
  PortfolioMode mode{PortfolioMode::FrozenV4};
  bool switched{},trialFrame{},trialAccepted{},trialRejected{};
  const char* v4Action{"none"};
  std::size_t frame{},switches{},trials{},accepted{},rejected{};
  double backendMs{},totalMs{},decisionMs{},coldSwitchMs{},referenceMs{};
 };
 static const char* name(PortfolioMode mode){
  switch(mode){
   case PortfolioMode::FrozenV4:return "frozen_v4";
   case PortfolioMode::RegionalExhaustive:return "region_exhaustive";
   case PortfolioMode::RegionalSharedBVH:return "region_shared_bvh";
   case PortfolioMode::RegionalCertifiedCache:return "region_certified_cache";
  }
  return "unknown";
 }
private:
 Decision last_{};
public:
 const Decision& decision()const{return last_;}
 Telemetry regionalWork()const{
  return active_==PortfolioMode::FrozenV4?Telemetry{}:regional_.telemetry();
 }
 void setSapThreshold(std::size_t n){regional_.setSapThreshold(n);}
 void setCachePadding(double p){regional_.setCachePadding(p);}
 // Explicitly choose the regional kernel under test, without changing V4.
 // A complete automatic multi-arm planner is a separate future hypothesis.
 void setTrialKernel(CrossMode kernel){
  switch(kernel){
   case CrossMode::Exhaustive:trialMode_=PortfolioMode::RegionalExhaustive;break;
   case CrossMode::SharedBVH:trialMode_=PortfolioMode::RegionalSharedBVH;break;
   case CrossMode::CertifiedCache:trialMode_=PortfolioMode::RegionalCertifiedCache;break;
  }
 }
 void reset(){
  v4_=V4{};
  regional_.reset();
  active_=PortfolioMode::FrozenV4;
  returnMode_=PortfolioMode::FrozenV4;
  frames_=activeFrames_=switches_=trials_=accepted_=rejected_=0;
  nextProbe_=kWarmup;
  initialized_=trialActive_=returnNext_=false;
  trialFrames_=aboveTolerance_=0;
  activeEwma_=trialTotalMs_=referenceCostMs_=0.;
  lastN_=0;
  last_={};
 }

 Pairs query(const std::vector<Bounds>& boxes,
             const std::vector<int>& labels){
  const auto entered=Clock::now();
  if(boxes.size()!=labels.size())
   throw std::invalid_argument("CBP portfolio needs a label per primitive");
  if(boxes.size()>static_cast<std::size_t>(std::numeric_limits<int>::max()))
   throw std::length_error("CBP portfolio exceeds indexed ID range");
  // Validate before mutating any selector or backend state, even when V4
  // is selected; invalid input must not poison subsequent decisions.
  for(std::size_t i=0;i<boxes.size();++i)
   if(boxes[i].id!=static_cast<int>(i)||!valid(boxes[i]))
    throw std::invalid_argument("CBP portfolio requires finite indexed AABBs");

  if(initialized_&&boxes.size()!=lastN_){
   // Topology and object-count changes invalidate all learned costs.
   reset();
  }
  initialized_=true;
  lastN_=boxes.size();
  ++frames_;
  bool switched=false,probeFrame=false,probeAccepted=false,probeRejected=false;
  double switchMs=0.;
  if(returnNext_){
   const auto began=Clock::now();
   activate(returnMode_);
   switchMs+=elapsed(began,Clock::now());
   switched=true;
   returnNext_=false;
  }else if(!trialActive_&&boxes.size()>=128 &&
           frames_>=nextProbe_ && activeFrames_>=kWarmup){
   returnMode_=active_;
   referenceCostMs_=activeEwma_;
   const PortfolioMode target=
       active_==PortfolioMode::FrozenV4?trialMode_:PortfolioMode::FrozenV4;
   const auto began=Clock::now();
   activate(target);
   switchMs+=elapsed(began,Clock::now());
   switched=true;
   trialActive_=true;
   trialFrames_=0;
   trialTotalMs_=0.;
   ++trials_;
  }
  probeFrame=trialActive_;
  const auto decided=Clock::now();
  const auto backendStart=Clock::now();
  Internal out=dispatch(boxes,labels);
  const auto finished=Clock::now();
  const double cost=elapsed(entered,finished);
  const double backendMs=elapsed(backendStart,finished);
  activeEwma_=activeFrames_?activeEwma_*.75+cost*.25:cost;
  ++activeFrames_;
  if(probeFrame){
   ++trialFrames_;
   trialTotalMs_+=cost;
   if(trialFrames_>=kTrialLength){
    const double meanTrial=trialTotalMs_/double(trialFrames_);
    // All costs are observed on executed queries only. Comparison is
    // across adjacent frames (not simultaneous); that assumption must
    // be challenged with regime-transition and randomized A/A controls.
    const bool promote=referenceCostMs_>0. && meanTrial<=
         referenceCostMs_*kPromote;
    if(promote){
     ++accepted_;probeAccepted=true;
    }else{
     ++rejected_;probeRejected=true;
     returnNext_=true;
    }
    trialActive_=false;
    nextProbe_=frames_+kCooldown;
   }
  }
  last_={active_,switched,probeFrame,probeAccepted,probeRejected,
         out.v4Action,frames_,switches_,trials_,accepted_,rejected_,
         backendMs,cost,elapsed(entered,decided),switchMs,referenceCostMs_};
  return std::move(out.pairs);
 }
};
} // namespace ccp::cbp
