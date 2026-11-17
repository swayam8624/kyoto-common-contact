#pragma once
// V6 EXPERIMENTAL UNIFIED CERTIFIED INTERACTION HIERARCHY (UCIH).
// One persistent refittable BVH owns either current primitive bounds (active
// mode) or conservative per-primitive containment envelopes (quiet mode).
// Candidate pairs from overlapping envelopes are cached, not merely current
// exact pairs. Reuse is permitted ONLY after all current AABBs have passed
// their containment AND collision-mask invariants.
//
// No reference to GlobalGuard, SAP, neural inference, known material labels,
// runtime of another algorithm, future replay frames, or an oracle. The BVH
// index is the single spatial structure used both for envelope repairs and
// active direct traversal. Incremental incident-edge updates can avoid BVH
// traversal when only a few primitives invalidate.
//
// This is current-frame exact collision coverage, NOT continuous-time
// collision safety. Future-event deadlines require solver-certified bounds.
// All heuristics affect cost ONLY and can never permit missing candidates.
#include "ccp/experimental_field.hpp"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <stdexcept>
#include <vector>

namespace ccp::experiment {
class UnifiedCertifiedHierarchy {
 RefitBVH index_{true};
 std::vector<Bounds> covers_,previous_;
 Pairs potential_,output_;
 Statistics stats_;
 bool active_{false};
 unsigned highActivity_{},quietActivity_{};
 static double maxEndpointMotion(const Bounds& a,const Bounds& b){
  return std::max({std::abs(a.min.x-b.min.x),std::abs(a.min.y-b.min.y),
                   std::abs(a.min.z-b.min.z),std::abs(a.max.x-b.max.x),
                   std::abs(a.max.y-b.max.y),std::abs(a.max.z-b.max.z)});
 }
 static bool sameSignature(const Bounds& a,const Bounds& b){
  return a.id==b.id&&a.category==b.category&&a.mask==b.mask&&
         a.movable==b.movable;
 }
 static Bounds pad(const Bounds& raw,double extent){
  Bounds r=raw;
  r.min.x-=extent;r.min.y-=extent;r.min.z-=extent;
  r.max.x+=extent;r.max.y+=extent;r.max.z+=extent;
  return r;
 }
 void absorb(const Statistics& x){
  stats_.builds+=x.builds;stats_.reuses+=x.reuses;
  stats_.repairs+=x.repairs;stats_.aabbTests+=x.aabbTests;
 }
 void direct(const std::vector<Bounds>& boxes){
  output_=index_.query(boxes);
  absorb(index_.statistics());
 }
 void filter(const std::vector<Bounds>& raw){
  output_.clear();output_.reserve(potential_.size());
  for(const auto& pair:potential_){
   const auto& a=raw[size_t(pair.a)],&b=raw[size_t(pair.b)];
   ++stats_.aabbTests;
   if(compatible(a,b)&&hits(a,b))output_.push_back(pair);
  }
  // potential_ is always sorted unique; filtering preserves this invariant.
 }
public:
 const Pairs& query(const std::vector<Bounds>& boxes){
  if(boxes.empty())throw std::invalid_argument("Unified hierarchy requires non-empty snapshot");
  if(!previous_.empty()&&boxes.size()!=previous_.size())
   throw std::runtime_error("Unified hierarchy topology changed");
  for(size_t i=0;i<boxes.size();++i)
   if(boxes[i].id!=int(i))
    throw std::runtime_error("Unified hierarchy requires stable zero-based AABB IDs");
  stats_={};
  const size_t n=boxes.size();
  size_t moved=0;
  for(size_t i=0;i<n;++i)
   if(!previous_.empty() && maxEndpointMotion(previous_[i],boxes[i])>.0075)
    ++moved;
  const double activity=double(moved)/double(n);
  if(active_){
   // Exactly one BVH is reused across both phases. BVH topology and refit
   // work are charged on every direct frame. No second backend/cache is run.
   direct(boxes);
   if(activity<.08){if(++quietActivity_>=8){
    active_=false;quietActivity_=0;
    covers_.clear();potential_.clear();
   }}else quietActivity_=0;
   previous_=boxes;return output_;
  }

  if(covers_.empty()){
   covers_.reserve(n);
   for(const auto& b:boxes)covers_.push_back(pad(b,.04));
   potential_=index_.query(covers_);
   absorb(index_.statistics());++stats_.builds;
   filter(boxes);
   previous_=boxes;
   return output_;
  }

  std::vector<unsigned char> dirty(n,0);
  size_t dirtyCount=0;
  for(size_t i=0;i<n;++i){
   if(!sameSignature(previous_[i],boxes[i])||
      !contains(covers_[i],boxes[i])){
    dirty[i]=1;
    ++dirtyCount;
    // This is *padding only*. It does not predict safe future movement.
    // Spatial high-pass difference is a cheap local deformation proxy;
    // using an ID-neighbor proxy is not valid for arbitrary mesh adjacency
    // and must be replaced by actual solver topology when integrated.
    const double displacement=maxEndpointMotion(previous_[i],boxes[i]);
    const double neighbor=i?maxEndpointMotion(previous_[i-1],boxes[i-1]):displacement;
    const double highpass=std::abs(displacement-neighbor);
    const double padding=std::clamp(.024+1.5*displacement+.4*highpass,
                                    .024,.16);
    covers_[i]=pad(boxes[i],padding);
   }
  }
  stats_.events=dirtyCount;
  if(!dirtyCount){
   highActivity_=0;
   ++stats_.reuses;
  }else{
   if(n>=512 && double(dirtyCount)/double(n)>.25){
    ++highActivity_;
   }else highActivity_=0;

   if(highActivity_>=2){
    // Persistent high certificate invalidation: stop maintaining an
    // expensive potential-pair list. Traverse the SAME BVH on current boxes.
    active_=true;quietActivity_=0;highActivity_=0;
    potential_.clear();covers_.clear();
    ++stats_.repairs;
    direct(boxes);
    previous_=boxes;return output_;
   }

   if(dirtyCount<=std::max(size_t{1},n/32)){
    // Relational delta: remove every potential edge incident on a changed
    // envelope, then find ALL possible new edges for the changed IDs.
    // The O(dirty*N) scan is charged, and used only for very small deltas.
    potential_.erase(
      std::remove_if(potential_.begin(),potential_.end(),
        [&](const Pair& p){return dirty[size_t(p.a)]||dirty[size_t(p.b)];}),
      potential_.end());
    for(size_t i=0;i<n;++i)if(dirty[i])
      for(size_t j=0;j<n;++j)if(i!=j){
       ++stats_.aabbTests;
       if(compatible(covers_[i],covers_[j])&&hits(covers_[i],covers_[j]))
        potential_.push_back(canonical(covers_[i],covers_[j]));
      }
    normalize(potential_);
    ++stats_.repairs;
   }else{
    potential_=index_.query(covers_);
    absorb(index_.statistics());
    ++stats_.builds;
   }
  }
  filter(boxes);
  previous_=boxes;
  return output_;
 }
 Statistics statistics()const{return stats_;}
 bool active()const{return active_;}
};
} // namespace ccp::experiment
