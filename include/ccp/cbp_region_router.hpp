#pragma once
// Kyoto Common Broad-Phase (CBP): Phase-1 exact regional routing prototype.
// EXPERIMENTAL, NOT A NOVELTY OR PERFORMANCE CLAIM.
//
// The input arrays are stable ID-indexed AABBs and persistent topology-aligned
// region labels. Same-region pairs delegate to coherent SAP or exhaustive.
// Cross-region interactions use conservative region-union AABBs and exact
// primitive checks. Region boundaries NEVER suppress actual overlaps.
//
// Default cross-region kernel is now one shared exact refittable primitive BVH.
// The original exhaustive cross-region join remains selectable as a control.
// This is NOT yet a cached certified-contact graph or cost-aware manager.
// Routing and all tests are exposed as telemetry. Keep V4 frozen.
#include "ccp/live_cloth.hpp"
#include "ccp/cbp_spatial_index.hpp"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <map>
#include <numeric>
#include <stdexcept>
#include <utility>
#include <vector>

namespace ccp::cbp {
using Bounds=ccp::live::Bounds;
using Pair=ccp::live::Pair;
using Pairs=std::vector<Pair>;

inline bool overlap(const Bounds& a,const Bounds& b){
 return a.min.x<=b.max.x && b.min.x<=a.max.x &&
        a.min.y<=b.max.y && b.min.y<=a.max.y &&
        a.min.z<=b.max.z && b.min.z<=a.max.z;
}
inline bool compatible(const Bounds& a,const Bounds& b){
 return (a.mask & b.category)!=0 && (b.mask & a.category)!=0;
}
inline bool valid(const Bounds& b){
 const double x[]={b.min.x,b.min.y,b.min.z,b.max.x,b.max.y,b.max.z};
 return std::all_of(std::begin(x),std::end(x),[](double v){return std::isfinite(v);}) &&
        b.min.x<=b.max.x && b.min.y<=b.max.y && b.min.z<=b.max.z;
}
inline void emit(const Bounds& a,const Bounds& b,Pairs& out){
 if(compatible(a,b) && overlap(a,b))
  out.push_back({std::min(a.id,b.id),std::max(a.id,b.id)});
}

enum class Kernel { Exhaustive, PersistentSAP };
// Exhaustive preserves Phase-0 as a direct A/B diagnostic control.
enum class CrossMode { Exhaustive, SharedBVH };
struct Telemetry {
 std::size_t regions{},localTests{},crossTests{},regionPairTests{},
             regionPairsPruned{},localExhaustiveRegions{},localSapRegions{},
             topologyResets{},coldSapInitializations{},
             indexBuilds{},indexChangedLeaves{},indexAncestorRefits{},
             indexNodePairVisits{},indexBoundsPruned{},indexSameRegionPruned{};
};

class RegionRouter {
 struct Region {
  std::vector<int> ids;
  std::vector<int> order;
  Bounds unionBounds{};
  bool initialized{};
 };
 std::map<int,Region> regions_;
 std::vector<int> ids_,labels_;
 std::map<int,Kernel> overrides_;
 SharedCrossBVH sharedIndex_;
 CrossMode crossMode_{CrossMode::SharedBVH};
 Telemetry last_{};
 std::size_t resets_{};
 std::size_t sapThreshold_{48};

 static Bounds boundsUnion(const std::vector<Bounds>& boxes,const std::vector<int>& ids){
  Bounds b=boxes[std::size_t(ids.front())];
  for(const int id:ids){
   const auto& a=boxes[std::size_t(id)];
   b.min.x=std::min(b.min.x,a.min.x);b.min.y=std::min(b.min.y,a.min.y);
   b.min.z=std::min(b.min.z,a.min.z);b.max.x=std::max(b.max.x,a.max.x);
   b.max.y=std::max(b.max.y,a.max.y);b.max.z=std::max(b.max.z,a.max.z);
  }
  return b;
 }
 static void normalize(Pairs& result){
  std::sort(result.begin(),result.end());
  result.erase(std::unique(result.begin(),result.end()),result.end());
 }
 void local(const std::vector<Bounds>& boxes,Region& r,Kernel kernel,Pairs& out){
  const auto& ids=r.ids;
  if(kernel==Kernel::Exhaustive){
   ++last_.localExhaustiveRegions;
   for(std::size_t a=0;a<ids.size();++a)
    for(std::size_t b=a+1;b<ids.size();++b){
     ++last_.localTests;
     emit(boxes[std::size_t(ids[a])],boxes[std::size_t(ids[b])],out);
    }
   return;
  }
  ++last_.localSapRegions;
  if(!r.initialized){
   r.order.resize(ids.size());
   std::iota(r.order.begin(),r.order.end(),0);
   r.initialized=true;
   ++last_.coldSapInitializations;
  }
  const auto less=[&](int ia,int ib){
   const auto& a=boxes[std::size_t(ids[std::size_t(ia)])];
   const auto& b=boxes[std::size_t(ids[std::size_t(ib)])];
   return a.min.x!=b.min.x ? a.min.x<b.min.x : a.id<b.id;
  };
  std::size_t shifts=0;
  bool fallback=false;
  const std::size_t cap=ids.size()*8;
  for(std::size_t i=1;i<r.order.size();++i){
   int item=r.order[i];std::size_t j=i;
   while(j>0 && less(item,r.order[j-1])){
    r.order[j]=r.order[j-1];--j;
    if(++shifts>cap){fallback=true;break;}
   }
   r.order[j]=item;
   if(fallback)break;
  }
  if(fallback)std::sort(r.order.begin(),r.order.end(),less);
  for(std::size_t i=0;i<r.order.size();++i){
   const auto& a=boxes[std::size_t(ids[std::size_t(r.order[i])])];
   for(std::size_t j=i+1;j<r.order.size();++j){
    const auto& b=boxes[std::size_t(ids[std::size_t(r.order[j])])];
    if(b.min.x>a.max.x)break;
    ++last_.localTests;
    emit(a,b,out);
   }
  }
 }
public:
 void setKernel(int regionId,Kernel kernel){overrides_[regionId]=kernel;}
 void setCrossMode(CrossMode mode){crossMode_=mode;}
 CrossMode crossMode() const{return crossMode_;}
 void clearOverrides(){overrides_.clear();}
 void setSapThreshold(std::size_t count){sapThreshold_=count;}
 void reset(){regions_.clear();ids_.clear();labels_.clear();sharedIndex_.clear();resets_=0;last_={};}
 Telemetry telemetry() const{return last_;}

 Pairs query(const std::vector<Bounds>& boxes,const std::vector<int>& regionLabels){
  if(boxes.size()!=regionLabels.size())
   throw std::invalid_argument("CBP requires one region label per input bound");
  for(std::size_t i=0;i<boxes.size();++i)
   if(boxes[i].id!=int(i) || !valid(boxes[i]))
    throw std::invalid_argument("CBP requires finite valid ID-indexed AABBs");
  last_={};
  last_.topologyResets=resets_;
  bool rebuild=boxes.size()!=ids_.size() || labels_!=regionLabels;
  if(!rebuild)for(std::size_t i=0;i<boxes.size();++i)
   if(ids_[i]!=boxes[i].id){rebuild=true;break;}
  if(rebuild){
   regions_.clear();ids_.resize(boxes.size());
   std::iota(ids_.begin(),ids_.end(),0);
   labels_=regionLabels;
   for(std::size_t i=0;i<boxes.size();++i)
    regions_[regionLabels[i]].ids.push_back(int(i));
   ++resets_;
   last_.topologyResets=resets_;
  }
  Pairs out;
  for(auto& [label,r]:regions_){
   r.unionBounds=boundsUnion(boxes,r.ids);
   const auto overrideIt=overrides_.find(label);
   const Kernel mode=overrideIt!=overrides_.end()?overrideIt->second:
       (r.ids.size()>=sapThreshold_?Kernel::PersistentSAP:Kernel::Exhaustive);
   local(boxes,r,mode,out);
  }
  if(crossMode_==CrossMode::SharedBVH){
   // The single spatial index contains ALL primitives but emits ONLY pairs
   // whose current region owners differ. Same-region pairs are handled above.
   // Dirty leaves and ancestors are refitted before every traversal; no
   // delayed certificate repair or future-geometry assumption is involved.
   sharedIndex_.appendCrossPairs(boxes,regionLabels,out);
   const auto& w=sharedIndex_.work();
   last_.crossTests=w.primitiveTests;
   last_.indexBuilds=w.builds;
   last_.indexChangedLeaves=w.changedLeaves;
   last_.indexAncestorRefits=w.ancestorRefits;
   last_.indexNodePairVisits=w.nodePairVisits;
   last_.indexBoundsPruned=w.boundsPruned;
   last_.indexSameRegionPruned=w.sameRegionPruned;
  }else{
   // Phase-0 reference: exact but potentially O(sum |R_a|*|R_b|).
   // Keep this for same-input correctness and measured A/B comparisons.
   for(auto a=regions_.begin();a!=regions_.end();++a)
    for(auto b=std::next(a);b!=regions_.end();++b){
     ++last_.regionPairTests;
     if(!overlap(a->second.unionBounds,b->second.unionBounds)){
      ++last_.regionPairsPruned;
      continue;
     }
     for(int ai:a->second.ids)for(int bi:b->second.ids){
      ++last_.crossTests;
      emit(boxes[std::size_t(ai)],boxes[std::size_t(bi)],out);
     }
    }
  }
  last_.regions=regions_.size();
  normalize(out);
  return out;
 }
};
} // namespace ccp::cbp
