#pragma once
// Generic, scalable online policy for any fixed-identity AABB snapshot series.
// Shares controller behavior across material benchmark and GPU/CPU replay.
// IMPORTANT: This experimental policy is distinct from the 12x12 cloth solver
// OnlinePartitioner. It uses AABB candidate-work feedback, not confirmed
// triangle-level contact impulses.
#include "ccp/research_multicloth.hpp"
#include "ccp/selective_guard.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <stdexcept>
#include <cstddef>
#include <vector>
namespace ccp::bench {
using namespace ccp::research;
using Clock=std::chrono::steady_clock;
inline std::vector<Bounds> rawBounds(const std::vector<Primitive>& items){
 std::vector<Bounds> out;out.reserve(items.size());
 for(const auto& p:items)out.push_back(p.box);
 return out;
}
struct Result {
 double queryMs{};
 std::vector<Pair> pairs;
 std::size_t builds{},reuses{},dirty{},guardBuilds{},guardReuses{},selectiveRepairs{},aabbTests{};
 int span{64};
 bool usedGlobal{};
 bool changedSpan{};
 bool switched{};
};
// Generic scalable online extension: unlike the old 12x12 cloth controller,
// works on any ordered material topology. The regions are stable contiguous
// element subsets and may change span with observed work and amortized costs.
// A global certified cache is kept available as a strong fallback.
// This is a distinct EXPERIMENTAL controller, not a drop-in replacement for
// the original MultiCloth::OnlineContactAware physical solver controller.
class GenericOnline {
 RegionalCandidates regions_;
 ccp::research::SelectiveGuard guard_;
 int span_{64},frame_{},lastEdit_{-1000},lastModeSwitch_{-1000};
 bool global_{false};
 double regionEwma_{0},globalEwma_{0};
 int regionObservations_{},globalObservations_{};
 std::size_t switches_{},edits_{};
 double margin_{.025};
public:
 explicit GenericOnline(double margin=.025):guard_(margin,.045),margin_(margin){
  if(!std::isfinite(margin)||margin<0)throw std::invalid_argument("invalid online margin");
 }
 Result query(const std::vector<Primitive>& input){
  const auto fullStart=Clock::now();
  ++frame_;
  // Two startup windows sample both real methods, then choose the cheaper
  // measured one. Short re-probes adapt to changed contact distributions.
  // Time spent on probes and cache misses is NOT hidden from output.
  if(frame_<=8)global_=false;
  else if(frame_<=16)global_=true;
  else if(frame_-lastModeSwitch_>=18){
   bool proposed=global_;
   if(regionObservations_>0&&globalObservations_>0){
    const bool probe=(frame_%120>=100&&frame_%120<108);
    proposed=probe?false:(globalEwma_<regionEwma_);
   }
   if(proposed!=global_){
    global_=proposed;lastModeSwitch_=frame_;++switches_;
   }
  }
  Result r;
  r.usedGlobal=global_;r.span=span_;
  if(global_){
   const auto raw=rawBounds(input);
   ccp::research::SelectiveGuard::Metrics m;
   const auto start=Clock::now();
   const auto& list=guard_.query(raw,m);
   r.pairs.assign(list.begin(),list.end());
   r.queryMs=std::chrono::duration<double,std::milli>(Clock::now()-start).count();
   r.guardBuilds=m.fullBuilds;r.guardReuses=m.cacheReuses;
   r.selectiveRepairs=m.partialRepairs;r.aabbTests=m.pairChecks;

  }else{
   std::vector<Primitive> reindexed=input;
   for(size_t i=0;i<reindexed.size();++i)reindexed[i].region=int(i/size_t(span_));
   Metrics m;
   const auto start=Clock::now();
   const auto& list=regions_.query(reindexed,margin_,true,m,true,false);
   r.pairs.assign(list.begin(),list.end());
   r.queryMs=std::chrono::duration<double,std::milli>(Clock::now()-start).count();
   r.builds=m.regionalPairRebuilds;r.reuses=m.regionalPairReuses;
   r.dirty=m.dirtyRegions;r.aabbTests=m.broadAabbTests;

   // Explicit cost-based structural controller: avoid splitting when
   // bookkeeping dominates. Adjust only if measured regional work per
   // primitive is unusually high/low for several contiguous windows.
   if(frame_-lastEdit_>=30&&frame_%15==0&&input.size()>=128){
    const double testsPerItem=double(m.broadAabbTests)/double(input.size());
    int desired=span_;
    if(testsPerItem>28. && span_>16)desired=span_/2;
    else if(testsPerItem<3.&&span_<256)desired=span_*2;
    if(desired!=span_){
     // Region IDs change and all regional certificates must be invalidated;
     // the full rebuilding cost is paid on the NEXT measured regional query.
     span_=desired;regions_.reset();lastEdit_=frame_;++edits_;r.changedSpan=true;
    }
   }
  }
  // The whole controller is charged: policy selection, temporary
  // region mapping, candidate generation, certificate repair, cache resets.
  // Never report only the inner backend query as online policy runtime.
  r.queryMs=std::chrono::duration<double,std::milli>(Clock::now()-fullStart).count();
  if(r.usedGlobal){
   globalEwma_=globalObservations_?globalEwma_*.85+r.queryMs*.15:r.queryMs;
   ++globalObservations_;
  }else{
   regionEwma_=regionObservations_?regionEwma_*.85+r.queryMs*.15:r.queryMs;
   ++regionObservations_;
  }
  r.span=span_;r.switched=(frame_==lastModeSwitch_);
  return r;
 }
 std::size_t switches()const{return switches_;}
 std::size_t edits()const{return edits_;}
};

} // namespace ccp::bench
