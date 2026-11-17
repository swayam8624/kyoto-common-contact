#pragma once
// CBP Phase 1: a single shared refittable BVH over current primitive AABBs.
// The index only enumerates CROSS-REGION pairs; same-region pairs remain
// the responsibility of RegionRouter's original exact local kernels.
// Every query uses CURRENT bounds; there are no predicted contacts, deferred
// correctness checks, or claims about continuous collision detection.
#include "ccp/live_cloth.hpp"
#include <algorithm>
#include <array>
#include <cstddef>
#include <limits>
#include <numeric>
#include <stdexcept>
#include <vector>

namespace ccp::cbp {
class SharedCrossBVH {
public:
 using Bounds=ccp::live::Bounds;
 using Pair=ccp::live::Pair;
 using Pairs=std::vector<Pair>;
 struct Work {
  std::size_t builds{},changedLeaves{},ancestorRefits{},
              nodePairVisits{},boundsPruned{},sameRegionPruned{},
              primitiveTests{},emittedPairs{};
 };
private:
 struct Node {
  Bounds box{};
  int parent{-1},left{-1},right{-1},primitive{-1},region{};
  std::size_t count{1};
  bool uniform{true};
  bool leaf()const{return primitive>=0;}
 };
 std::vector<Node> nodes_;
 std::vector<int> leaves_,labels_;
 int root_{-1};
 Work work_{};

 static bool touches(const Bounds& a,const Bounds& b){
  return a.min.x<=b.max.x&&b.min.x<=a.max.x&&
         a.min.y<=b.max.y&&b.min.y<=a.max.y&&
         a.min.z<=b.max.z&&b.min.z<=a.max.z;
 }
 static bool sameGeometry(const Bounds& a,const Bounds& b){
  return a.min.x==b.min.x&&a.min.y==b.min.y&&a.min.z==b.min.z&&
         a.max.x==b.max.x&&a.max.y==b.max.y&&a.max.z==b.max.z;
 }
 static Bounds unite(Bounds a,const Bounds& b){
  a.min.x=std::min(a.min.x,b.min.x);a.min.y=std::min(a.min.y,b.min.y);
  a.min.z=std::min(a.min.z,b.min.z);a.max.x=std::max(a.max.x,b.max.x);
  a.max.y=std::max(a.max.y,b.max.y);a.max.z=std::max(a.max.z,b.max.z);
  return a;
 }
 static long double doubledCenter(const Bounds& b,int axis){
  if(axis==0)return static_cast<long double>(b.min.x)+b.max.x;
  if(axis==1)return static_cast<long double>(b.min.y)+b.max.y;
  return static_cast<long double>(b.min.z)+b.max.z;
 }
 int build(std::vector<int>& ids,std::size_t first,std::size_t last,
           int parent,const std::vector<Bounds>& boxes,
           const std::vector<int>& labels){
  const int here=static_cast<int>(nodes_.size());
  nodes_.emplace_back();
  nodes_[here].parent=parent;
  nodes_[here].count=last-first;
  if(last-first==1){
   const int id=ids[first];
   nodes_[here].primitive=id;
   nodes_[here].box=boxes[static_cast<std::size_t>(id)];
   nodes_[here].region=labels[static_cast<std::size_t>(id)];
   leaves_[static_cast<std::size_t>(id)]=here;
   return here;
  }
  Bounds total=boxes[static_cast<std::size_t>(ids[first])];
  for(std::size_t i=first+1;i<last;++i)
   total=unite(total,boxes[static_cast<std::size_t>(ids[i])]);
  const std::array<long double,3> widths={
   static_cast<long double>(total.max.x)-total.min.x,
   static_cast<long double>(total.max.y)-total.min.y,
   static_cast<long double>(total.max.z)-total.min.z};
  const int axis=static_cast<int>(std::max_element(widths.begin(),widths.end())-widths.begin());
  const std::size_t mid=first+(last-first)/2;
  std::nth_element(ids.begin()+static_cast<std::ptrdiff_t>(first),
                   ids.begin()+static_cast<std::ptrdiff_t>(mid),
                   ids.begin()+static_cast<std::ptrdiff_t>(last),
                   [&](int a,int b){
    const auto ca=doubledCenter(boxes[static_cast<std::size_t>(a)],axis);
    const auto cb=doubledCenter(boxes[static_cast<std::size_t>(b)],axis);
    return ca==cb?a<b:ca<cb;
   });
  const int left=build(ids,first,mid,here,boxes,labels);
  const int right=build(ids,mid,last,here,boxes,labels);
  Node& n=nodes_[here];
  n.left=left;n.right=right;
  n.box=unite(nodes_[left].box,nodes_[right].box);
  n.uniform=nodes_[left].uniform&&nodes_[right].uniform&&
            nodes_[left].region==nodes_[right].region;
  if(n.uniform)n.region=nodes_[left].region;
  return here;
 }
 void sync(const std::vector<Bounds>& boxes,const std::vector<int>& labels){
  work_={};
  if(labels!=labels_||boxes.size()!=leaves_.size()){
   nodes_.clear();leaves_.assign(boxes.size(),-1);labels_=labels;root_=-1;
   if(!boxes.empty()){
    if(boxes.size()>static_cast<std::size_t>(std::numeric_limits<int>::max())/2)
     throw std::length_error("CBP BVH node index range exceeded");
    nodes_.reserve(2*boxes.size()-1);
    std::vector<int> ids(boxes.size());
    std::iota(ids.begin(),ids.end(),0);
    root_=build(ids,0,boxes.size(),-1,boxes,labels);
   }
   ++work_.builds;
   return;
  }
  std::vector<unsigned char> dirty(nodes_.size(),0);
  for(std::size_t i=0;i<boxes.size();++i){
   Node& leaf=nodes_[static_cast<std::size_t>(leaves_[i])];
   if(sameGeometry(leaf.box,boxes[i]))continue;
   leaf.box=boxes[i];
   ++work_.changedLeaves;
   int parent=leaf.parent;
   while(parent>=0&&!dirty[static_cast<std::size_t>(parent)]){
    dirty[static_cast<std::size_t>(parent)]=1;
    parent=nodes_[static_cast<std::size_t>(parent)].parent;
   }
  }
  // Children are created after parents, so reverse order refits exactly
  // the affected ancestors and never leaves a stale union bound.
  for(std::size_t k=nodes_.size();k>0;--k){
   const auto idx=k-1;
   Node& n=nodes_[idx];
   if(!dirty[idx]||n.leaf())continue;
   n.box=unite(nodes_[static_cast<std::size_t>(n.left)].box,
               nodes_[static_cast<std::size_t>(n.right)].box);
   ++work_.ancestorRefits;
  }
 }
 void visit(int a,int b,const std::vector<Bounds>& boxes,
            const std::vector<int>& labels,Pairs& out){
  const Node& x=nodes_[static_cast<std::size_t>(a)];
  const Node& y=nodes_[static_cast<std::size_t>(b)];
  ++work_.nodePairVisits;
  if(x.uniform&&y.uniform&&x.region==y.region){
   ++work_.sameRegionPruned;return;
  }
  if(!touches(x.box,y.box)){++work_.boundsPruned;return;}
  if(a==b){
   if(x.leaf())return;
   visit(x.left,x.left,boxes,labels,out);
   visit(x.left,x.right,boxes,labels,out);
   visit(x.right,x.right,boxes,labels,out);
   return;
  }
  if(x.leaf()&&y.leaf()){
   const auto ia=static_cast<std::size_t>(x.primitive);
   const auto ib=static_cast<std::size_t>(y.primitive);
   if(labels[ia]==labels[ib])return;
   ++work_.primitiveTests;
   const Bounds& p=boxes[ia];const Bounds& q=boxes[ib];
   if((p.mask&q.category)!=0&&(q.mask&p.category)!=0&&touches(p,q)){
    out.push_back({std::min(p.id,q.id),std::max(p.id,q.id)});
    ++work_.emittedPairs;
   }
   return;
  }
  if(!x.leaf()&&(y.leaf()||x.count>=y.count)){
   visit(x.left,b,boxes,labels,out);
   visit(x.right,b,boxes,labels,out);
  }else{
   visit(a,y.left,boxes,labels,out);
   visit(a,y.right,boxes,labels,out);
  }
 }
 // Indexed incident-edge discovery used by the certified cross-region cache.
 // This is a one-to-many current-ENVELOPE query, not an O(dirty*N) scan.
 void visitIncident(int primitive,int nodeId,const std::vector<Bounds>& covers,
                    const std::vector<int>& labels,Pairs& out){
  const Node& n=nodes_[static_cast<std::size_t>(nodeId)];
  const Bounds& source=covers[static_cast<std::size_t>(primitive)];
  ++work_.nodePairVisits;
  if(n.uniform && n.region==labels[static_cast<std::size_t>(primitive)]){
   ++work_.sameRegionPruned;return;
  }
  if(!touches(source,n.box)){++work_.boundsPruned;return;}
  if(n.leaf()){
   const int other=n.primitive;
   if(other==primitive || labels[static_cast<std::size_t>(other)]==
                          labels[static_cast<std::size_t>(primitive)])return;
   ++work_.primitiveTests;
   const Bounds& target=covers[static_cast<std::size_t>(other)];
   if((source.mask&target.category)!=0 &&
      (target.mask&source.category)!=0 && touches(source,target)){
    out.push_back({std::min(primitive,other),std::max(primitive,other)});
    ++work_.emittedPairs;
   }
   return;
  }
  visitIncident(primitive,n.left,covers,labels,out);
  visitIncident(primitive,n.right,covers,labels,out);
 }
public:
 void clear(){nodes_.clear();leaves_.clear();labels_.clear();root_=-1;work_={};}
 const Work& work()const{return work_;}
 void appendCrossPairs(const std::vector<Bounds>& boxes,
                       const std::vector<int>& labels,Pairs& out){
  if(boxes.size()!=labels.size())
   throw std::invalid_argument("CBP BVH requires one label per primitive");
  sync(boxes,labels);
  if(root_>=0)visit(root_,root_,boxes,labels,out);
 }
 // Refit ONCE, then perform independent BVH-culled queries for every dirty
 // primitive. Callers must deduplicate results when both endpoints are dirty.
 void appendIncidentCrossPairs(const std::vector<Bounds>& covers,
                               const std::vector<int>& labels,
                               const std::vector<int>& dirty,Pairs& out){
  if(covers.size()!=labels.size())
   throw std::invalid_argument("CBP incident query needs one label per primitive");
  sync(covers,labels);
  for(const int primitive:dirty){
   if(primitive<0 || static_cast<std::size_t>(primitive)>=covers.size())
    throw std::out_of_range("CBP dirty primitive ID out of range");
   if(root_>=0)visitIncident(primitive,root_,covers,labels,out);
  }
 }
};
} // namespace ccp::cbp
