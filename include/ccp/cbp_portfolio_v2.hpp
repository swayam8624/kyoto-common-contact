#pragma once
// CBP Phase 10 — causal, bounded four-arm portfolio ablation.
//
// The Phase 9 reference portfolio tried only RegionalExhaustive, so it COULD
// NOT select the highly competitive SharedBVH. This independent implementation
// is an explicit hypothesis test, not a replacement for the frozen policy.
//
// Exactly ONE backend executes per frame, including on probe frames. Backend
// state is destroyed on every mode change; cold-build and migration time are
// included in the public query. No future-frame oracle, material-name lookup,
// retrospective winning-method labels or shadow execution is consulted.
//
// This is *scene-level* exploration, NOT local per-patch multi-arm execution.
// AABB candidate exactness relies on each tested primitive method; the replay
// and independent CTest exhaustively validate actual output on small inputs.
#include "ccp/cbp_region_router.hpp"
#include "ccp/final_broadphase.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

namespace ccp::cbp {
enum class PortfolioV2Arm {
  FrozenV4, SharedBVH, RegionSweep, RegionExhaustive
};

class CbpPortfolioV2 {
 using Clock = std::chrono::steady_clock;
 using V4 = ccp::final_broadphase::GuardSAPBoundedTrial;
 static constexpr std::size_t kWarmup = 8;
 static constexpr std::size_t kTrialFrames = 6;
 static constexpr std::size_t kCooldown = 12;
 static constexpr double kPromotionRatio = .94;
 static constexpr std::array<PortfolioV2Arm,4> kTour{
   PortfolioV2Arm::SharedBVH,PortfolioV2Arm::RegionSweep,
   PortfolioV2Arm::RegionExhaustive,PortfolioV2Arm::FrozenV4
 };
 V4 v4_{};
 RegionRouter region_{};
 PortfolioV2Arm active_{PortfolioV2Arm::FrozenV4};
 PortfolioV2Arm champion_{PortfolioV2Arm::FrozenV4};
 PortfolioV2Arm tested_{PortfolioV2Arm::FrozenV4};
 std::size_t nextTourArm_{},frame_{},activeFrames_{},nextTrial_{kWarmup};
 std::size_t trialFrames_{},switches_{},trials_{},promotions_{},rejections_{};
 std::size_t knownN_{};
 std::vector<int> knownLabels_{};
 bool initialized_{},inTrial_{},returnPending_{};
 double championEwmaMs_{},championReferenceMs_{},trialCostMs_{};
 double trialBackendMs_{};
 struct Observed {
  Pairs pairs;
  const char* v4Action{"none"};
 };
 static double millis(Clock::time_point a,Clock::time_point b){
  return std::chrono::duration<double,std::milli>(b-a).count();
 }
 void activate(PortfolioV2Arm next){
  if(active_==next)return;
  // Both full warm structures are discarded. This is conservative and
  // prevents an unselected index from receiving free maintenance.
  v4_=V4{};
  region_.reset();
  region_.setRegionSweep(next==PortfolioV2Arm::RegionSweep);
  region_.setCrossMode(next==PortfolioV2Arm::SharedBVH?
     CrossMode::SharedBVH:CrossMode::Exhaustive);
  active_=next;
  activeFrames_=0;
  ++switches_;
 }
 Observed dispatch(const std::vector<Bounds>& boxes,
                   const std::vector<int>& labels){
  if(active_==PortfolioV2Arm::FrozenV4){
   auto result=v4_.query(boxes);
   return {std::move(result.pairs),result.action};
  }
  return {region_.query(boxes,labels),"none"};
 }
public:
 struct Decision {
  PortfolioV2Arm mode{PortfolioV2Arm::FrozenV4};
  PortfolioV2Arm champion{PortfolioV2Arm::FrozenV4};
  std::size_t frame{},switches{},trials{},promotions{},rejections{},trialFrames{};
  bool switched{},probe{},promoted{},rejected{},returning{};
  double totalMs{},backendMs{},decisionMs{},migrationMs{};
  double championReferenceMs{},trialMeanMs{},championEwmaMs{};
  const char* v4Action{"none"};
 };
 static const char* name(PortfolioV2Arm a){
  switch(a){
   case PortfolioV2Arm::FrozenV4:return "frozen_v4";
   case PortfolioV2Arm::SharedBVH:return "shared_bvh";
   case PortfolioV2Arm::RegionSweep:return "region_sweep";
   case PortfolioV2Arm::RegionExhaustive:return "region_exhaustive";
  }
  return "invalid";
 }
 private:
 Decision last_{};
 public:
 const Decision& decision()const noexcept{return last_;}
 void setSapThreshold(std::size_t n){region_.setSapThreshold(n);}
 void reset(){
  v4_=V4{};
  region_.reset();
  region_.setCrossMode(CrossMode::SharedBVH);
  region_.setRegionSweep(false);
  active_=champion_=tested_=PortfolioV2Arm::FrozenV4;
  nextTourArm_=frame_=activeFrames_=0;
  nextTrial_=kWarmup;
  trialFrames_=switches_=trials_=promotions_=rejections_=0;
  knownN_=0;knownLabels_.clear();
  initialized_=inTrial_=returnPending_=false;
  championEwmaMs_=championReferenceMs_=trialCostMs_=trialBackendMs_=0.;
  last_={};
 }

 Pairs query(const std::vector<Bounds>& boxes,
             const std::vector<int>& labels){
  const auto begin=Clock::now();
  if(boxes.size()!=labels.size())
   throw std::invalid_argument("Four-arm selector requires stable ID labels");
  if(boxes.size()>static_cast<std::size_t>(std::numeric_limits<int>::max()))
   throw std::length_error("Four-arm selector exceeds 32-bit stable IDs");
  for(std::size_t k=0;k<boxes.size();++k)
   if(boxes[k].id!=static_cast<int>(k)||!valid(boxes[k]))
    throw std::invalid_argument("Four-arm selector requires valid finite indexed AABBs");

  // Changing primitive ownership changes the spatial partition, and makes
  // previous per-scene cost estimates invalid. Reset causal state. This
  // behavior is costly but is honest and regression-tested.
  if(initialized_&&(knownN_!=boxes.size()||knownLabels_!=labels))
   reset();
  if(!initialized_){
   initialized_=true;
   knownN_=boxes.size();
   knownLabels_=labels;
  }

  ++frame_;
  bool switched=false,probe=false,promoted=false,rejected=false,returning=false;
  double migration=0.;
  if(returnPending_){
   const auto s=Clock::now();
   activate(champion_);
   migration+=millis(s,Clock::now());
   switched=true;returning=true;
   returnPending_=false;
  }else if(!inTrial_&&boxes.size()>=128&&
           frame_>=nextTrial_&&championEwmaMs_>0.){
   // All arms are visited in a fixed order, using no future information.
   // Do not spend a trial on the current champion.
   auto candidate=kTour[nextTourArm_++%kTour.size()];
   if(candidate==champion_)candidate=kTour[nextTourArm_++%kTour.size()];
   tested_=candidate;
   championReferenceMs_=championEwmaMs_;
   const auto s=Clock::now();
   activate(candidate);
   migration+=millis(s,Clock::now());
   switched=true;
   inTrial_=true;
   trialFrames_=0;trialCostMs_=trialBackendMs_=0.;
   ++trials_;
  }

  probe=inTrial_;
  const auto selected=Clock::now();
  const auto backendStart=Clock::now();
  Observed result=dispatch(boxes,labels);
  const auto backendEnd=Clock::now();
  const double backend=millis(backendStart,backendEnd);
  // Total excludes work done in separate benchmark validation, but
  // INCLUDES the one executed backend + controller input validation,
  // decision, cold state reset, and telemetry accounting below.
  // Time reading happens after all work that can alter the output.
  const double current=millis(begin,Clock::now());
  ++activeFrames_;
  if(probe){
   ++trialFrames_;
   trialCostMs_+=current;
   trialBackendMs_+=backend;
   if(trialFrames_==kTrialFrames){
    const double sample=trialCostMs_/double(trialFrames_);
    // Conservative switching requires improvement including the cold
    // first trial frame. "Champion reference" is historical, not
    // simultaneous: drifting scene costs may bias this comparison.
    const bool accept=sample<kPromotionRatio*championReferenceMs_;
    if(accept){
     champion_=tested_;
     championEwmaMs_=sample;
     ++promotions_;promoted=true;
    }else{
     ++rejections_;rejected=true;
     returnPending_=true;
    }
    inTrial_=false;
    nextTrial_=frame_+kCooldown;
   }
  }else{
   championEwmaMs_=championEwmaMs_?
      .75*championEwmaMs_+.25*current:current;
  }
  last_={active_,champion_,frame_,switches_,trials_,promotions_,rejections_,
         trialFrames_,switched,probe,promoted,rejected,returning,
         millis(begin,Clock::now()),backend,millis(begin,selected),migration,
         championReferenceMs_,trialFrames_?trialCostMs_/double(trialFrames_):0.,
         championEwmaMs_,result.v4Action};
  return std::move(result.pairs);
 }
};
} // namespace ccp::cbp
