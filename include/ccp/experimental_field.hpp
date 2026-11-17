#pragma once
// Independent exact AABB broad-phase candidates for the Kyoto research rumble.
// All costs (including rebuilds, hash insertion, refits, signal analysis and
// canonical pair sorting) are charged to the actual query call.
// No lookahead, material labels, ground-truth hints or oracle-based decisions.
// "Signal" means *causal patch displacement activity*, not simulated acoustics.
// Safety is provided exclusively by exact AABB tests and conservative envelopes.
#include "ccp/live_cloth.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <numeric>
#include <stdexcept>
#include <unordered_map>
#include <utility>
#include <vector>

namespace ccp::experiment {
using Bounds=ccp::live::Bounds;
using Pair=ccp::live::Pair;
using Pairs=std::vector<Pair>;
struct Statistics {
 std::size_t builds{},reuses{},repairs{},aabbTests{},events{};
};
inline bool hits(const Bounds& a,const Bounds& b){
 return a.min.x<=b.max.x&&b.min.x<=a.max.x&&
        a.min.y<=b.max.y&&b.min.y<=a.max.y&&
        a.min.z<=b.max.z&&b.min.z<=a.max.z;
}
inline bool compatible(const Bounds& a,const Bounds& b){
 return (a.mask&b.category)!=0&&(b.mask&a.category)!=0;
}
inline bool identical(const Bounds& a,const Bounds& b){
 return a.id==b.id&&a.category==b.category&&a.mask==b.mask&&
 a.movable==b.movable&&
 a.min.x==b.min.x&&a.min.y==b.min.y&&a.min.z==b.min.z&&
 a.max.x==b.max.x&&a.max.y==b.max.y&&a.max.z==b.max.z;
}
inline Pair canonical(const Bounds& a,const Bounds& b){
 return {std::min(a.id,b.id),std::max(a.id,b.id)};
}
inline void normalize(Pairs& out){
 std::sort(out.begin(),out.end());
 out.erase(std::unique(out.begin(),out.end()),out.end());
}
inline Bounds envelope(const std::vector<Bounds>& b,size_t begin,size_t end,
                       double pad=0.){
 const double inf=std::numeric_limits<double>::infinity();
 Bounds result;
 result.min={inf,inf,inf};result.max={-inf,-inf,-inf};
 for(size_t i=begin;i<end;++i){
  const auto& q=b[i];
  result.min.x=std::min(result.min.x,q.min.x);
  result.min.y=std::min(result.min.y,q.min.y);
  result.min.z=std::min(result.min.z,q.min.z);
  result.max.x=std::max(result.max.x,q.max.x);
  result.max.y=std::max(result.max.y,q.max.y);
  result.max.z=std::max(result.max.z,q.max.z);
 }
 result.min.x-=pad;result.min.y-=pad;result.min.z-=pad;
 result.max.x+=pad;result.max.y+=pad;result.max.z+=pad;
 return result;
}
inline bool contains(const Bounds& outer,const Bounds& inner){
 return inner.min.x>=outer.min.x&&inner.min.y>=outer.min.y&&
 inner.min.z>=outer.min.z&&inner.max.x<=outer.max.x&&
 inner.max.y<=outer.max.y&&inner.max.z<=outer.max.z;
}
inline double center(const Bounds& b,int axis){
 if(axis==0)return (b.min.x+b.max.x)*.5;
 if(axis==1)return (b.min.y+b.max.y)*.5;
 return (b.min.z+b.max.z)*.5;
}

// Balanced median BVH built once, refitted every frame. Adapting the
// topology is optional; both policies always query actual current bounds.
// Recursive unordered node-node traversal emits each pair exactly once.
class RefitBVH {
 struct Node {
  Bounds box;
  int left{-1},right{-1};
  size_t begin{},end{};
  bool leaf()const{return left<0;}
 };
 std::vector<Node> nodes_;
 std::vector<int> permutation_,ids_;
 Pairs out_;
 Statistics stat_;
 bool adaptive_{},rebuildNext_{};
 bool aborted_{};
 std::size_t visits_{};
 static constexpr size_t kLeaf=8;
 void checkIDs(const std::vector<Bounds>& boxes){
  if(ids_.size()!=boxes.size()){
   ids_.clear();
   for(const auto& b:boxes)ids_.push_back(b.id);
   nodes_.clear();permutation_.clear();
  }else for(size_t i=0;i<boxes.size();++i)
   if(ids_[i]!=boxes[i].id)
    throw std::runtime_error("refit BVH requires fixed ordered IDs");
 }
 int buildNode(const std::vector<Bounds>& boxes,size_t begin,size_t end){
  const int index=int(nodes_.size());nodes_.push_back({});
  nodes_[size_t(index)].begin=begin;nodes_[size_t(index)].end=end;
  if(end-begin<=kLeaf)return index;
  std::array<double,3> minimum={1e300,1e300,1e300};
  std::array<double,3> maximum={-1e300,-1e300,-1e300};
  for(size_t i=begin;i<end;++i){
   const auto& b=boxes[size_t(permutation_[i])];
   for(int ax=0;ax<3;++ax){
    const double c=center(b,ax);
    minimum[size_t(ax)]=std::min(minimum[size_t(ax)],c);
    maximum[size_t(ax)]=std::max(maximum[size_t(ax)],c);
   }
  }
  int ax=0;
  for(int k=1;k<3;++k)
   if(maximum[size_t(k)]-minimum[size_t(k)]>
      maximum[size_t(ax)]-minimum[size_t(ax)])ax=k;
  const size_t mid=begin+(end-begin)/2;
  std::nth_element(permutation_.begin()+std::ptrdiff_t(begin),
                   permutation_.begin()+std::ptrdiff_t(mid),
                   permutation_.begin()+std::ptrdiff_t(end),
                   [&](int i,int j){
                    const double a=center(boxes[size_t(i)],ax);
                    const double b=center(boxes[size_t(j)],ax);
                    if(a!=b)return a<b;
                    return boxes[size_t(i)].id<boxes[size_t(j)].id;
                   });
  const int left=buildNode(boxes,begin,mid);
  const int right=buildNode(boxes,mid,end);
  nodes_[size_t(index)].left=left;
  nodes_[size_t(index)].right=right;
  return index;
 }
 Bounds refit(int i,const std::vector<Bounds>& boxes){
  auto& n=nodes_[size_t(i)];
  if(n.leaf()){
   const double inf=std::numeric_limits<double>::infinity();
   n.box.min={inf,inf,inf};n.box.max={-inf,-inf,-inf};
   for(size_t k=n.begin;k<n.end;++k){
    const auto& b=boxes[size_t(permutation_[k])];
    n.box.min.x=std::min(n.box.min.x,b.min.x);
    n.box.min.y=std::min(n.box.min.y,b.min.y);
    n.box.min.z=std::min(n.box.min.z,b.min.z);
    n.box.max.x=std::max(n.box.max.x,b.max.x);
    n.box.max.y=std::max(n.box.max.y,b.max.y);
    n.box.max.z=std::max(n.box.max.z,b.max.z);
   }
  }else{
   const auto l=refit(n.left,boxes),r=refit(n.right,boxes);
   n.box.min={std::min(l.min.x,r.min.x),std::min(l.min.y,r.min.y),
              std::min(l.min.z,r.min.z)};
   n.box.max={std::max(l.max.x,r.max.x),std::max(l.max.y,r.max.y),
              std::max(l.max.z,r.max.z)};
  }
  return n.box;
 }
 void emit(size_t i,size_t j,const std::vector<Bounds>& boxes){
  const auto& a=boxes[size_t(permutation_[i])];
  const auto& b=boxes[size_t(permutation_[j])];
  ++stat_.aabbTests;
  if(compatible(a,b)&&hits(a,b))out_.push_back(canonical(a,b));
 }
 void walk(int left,int right,const std::vector<Bounds>& boxes){
  if(aborted_)return;
  // A capped tree walk falls back to the independently measured exact
  // fresh-sort SAP, including *all* work expended on the aborted walk.
  if(++visits_>std::max(size_t{12000},boxes.size()*size_t{64})){
   aborted_=true;return;
  }
  const auto& a=nodes_[size_t(left)];
  const auto& b=nodes_[size_t(right)];
  ++stat_.aabbTests;
  if(!hits(a.box,b.box))return;
  if(left==right){
   if(a.leaf()){
    for(size_t i=a.begin;i<a.end;++i)
     for(size_t j=i+1;j<a.end;++j)emit(i,j,boxes);
   }else{
    walk(a.left,a.left,boxes);
    walk(a.left,a.right,boxes);
    walk(a.right,a.right,boxes);
   }
  }else if(a.leaf()&&b.leaf()){
   for(size_t i=a.begin;i<a.end;++i)
    for(size_t j=b.begin;j<b.end;++j)emit(i,j,boxes);
  }else if(b.leaf()||(!a.leaf()&&a.end-a.begin>=b.end-b.begin)){
   walk(a.left,right,boxes);walk(a.right,right,boxes);
  }else{
   walk(left,b.left,boxes);walk(left,b.right,boxes);
  }
 }
public:
 explicit RefitBVH(bool adaptive=false):adaptive_(adaptive){}
 const Pairs& query(const std::vector<Bounds>& boxes){
  if(boxes.empty())throw std::invalid_argument("empty BVH snapshot");
  stat_={};
  checkIDs(boxes);
  if(nodes_.empty()||rebuildNext_){
   permutation_.resize(boxes.size());
   std::iota(permutation_.begin(),permutation_.end(),0);
   nodes_.clear();nodes_.reserve(boxes.size()*2);
   buildNode(boxes,0,boxes.size());
   ++stat_.builds;rebuildNext_=false;
  }else ++stat_.reuses;
  refit(0,boxes);
  out_.clear();visits_=0;aborted_=false;
  walk(0,0,boxes);
  if(aborted_){
   out_=ccp::live::candidates(boxes,true);
   ++stat_.repairs;
  }else normalize(out_);
  // Decisions use *past* observed tree work to schedule a future rebuild.
  rebuildNext_=adaptive_&&(aborted_||visits_>boxes.size()*size_t{10});
  return out_;
 }
 Statistics statistics()const{return stat_;}
};

// Fresh uniform spatial grid. Overflow-size objects become "spill" objects
// compared with everyone, and excessive spilling falls back to exact SAP.
// Inclusive cell coverage ensures touching AABBs cannot be missed.
class SpatialGrid {
 struct Cell {
  int64_t x{},y{},z{};
  bool operator==(const Cell& c)const{
   return x==c.x&&y==c.y&&z==c.z;
  }
 };
 struct Hash {
  std::size_t operator()(const Cell& c)const{
   auto mix=[](uint64_t x){
    x^=x>>30;x*=UINT64_C(0xbf58476d1ce4e5b9);
    x^=x>>27;x*=UINT64_C(0x94d049bb133111eb);
    return x^(x>>31);
   };
   return size_t(mix(uint64_t(c.x))^
                 (mix(uint64_t(c.y))<<1)^
                 (mix(uint64_t(c.z))<<2));
  }
 };
 Pairs out_;
 Statistics stat_;
 static bool coord(double x,double h,int64_t& o){
  const long double q=std::floor(static_cast<long double>(x)/h);
  if(!std::isfinite(q)||
     q<static_cast<long double>(std::numeric_limits<int64_t>::min())+1||
     q>static_cast<long double>(std::numeric_limits<int64_t>::max())-1)
   return false;
  o=static_cast<int64_t>(q);return true;
 }
public:
 const Pairs& query(const std::vector<Bounds>& boxes){
  stat_={};out_.clear();
  if(boxes.empty())return out_;
  std::vector<double> side;side.reserve(boxes.size());
  for(const auto& b:boxes)
   side.push_back(std::max({b.max.x-b.min.x,b.max.y-b.min.y,b.max.z-b.min.z}));
  std::nth_element(side.begin(),side.begin()+std::ptrdiff_t(side.size()/2),side.end());
  const double typical=side[side.size()/2];
  const auto total=envelope(boxes,0,boxes.size());
  const double diagonal=std::sqrt(
   std::pow(total.max.x-total.min.x,2)+
   std::pow(total.max.y-total.min.y,2)+
   std::pow(total.max.z-total.min.z,2));
  const double h=std::max({1e-6,2.*typical,diagonal/std::sqrt(double(boxes.size()))});
  std::unordered_map<Cell,std::vector<int>,Hash> grid;
  std::vector<int> spills;
  for(size_t i=0;i<boxes.size();++i){
   const auto& b=boxes[i];
   int64_t lo[3],hi[3];
   if(!coord(b.min.x,h,lo[0])||!coord(b.min.y,h,lo[1])||
      !coord(b.min.z,h,lo[2])||!coord(b.max.x,h,hi[0])||
      !coord(b.max.y,h,hi[1])||!coord(b.max.z,h,hi[2])){
    spills.push_back(int(i));continue;
   }
   const long double nx=static_cast<long double>(hi[0])-lo[0]+1;
   const long double ny=static_cast<long double>(hi[1])-lo[1]+1;
   const long double nz=static_cast<long double>(hi[2])-lo[2]+1;
   if(nx*ny*nz>128 || nx>128 || ny>128 || nz>128){
    spills.push_back(int(i));continue;
   }
   for(int64_t x=lo[0];x<=hi[0];++x)
    for(int64_t y=lo[1];y<=hi[1];++y)
     for(int64_t z=lo[2];z<=hi[2];++z)
      grid[{x,y,z}].push_back(int(i));
  }
  if(spills.size()>std::max(size_t{64},boxes.size()/16)){
   out_=ccp::live::candidates(boxes,true);
   ++stat_.builds;return out_;
  }
  for(const auto& entry:grid){
   const auto& occupants=entry.second;
   for(size_t i=0;i<occupants.size();++i)
    for(size_t j=i+1;j<occupants.size();++j){
     const auto& a=boxes[size_t(occupants[i])];
     const auto& b=boxes[size_t(occupants[j])];
     ++stat_.aabbTests;
     if(compatible(a,b)&&hits(a,b))out_.push_back(canonical(a,b));
    }
  }
  for(int i:spills)
   for(size_t j=0;j<boxes.size();++j){
    if(size_t(i)==j)continue;
    const auto& a=boxes[size_t(i)],&b=boxes[j];
    ++stat_.aabbTests;
    if(compatible(a,b)&&hits(a,b))out_.push_back(canonical(a,b));
   }
  normalize(out_);++stat_.builds;
  return out_;
 }
 Statistics statistics()const{return stat_;}
};

// Incremental spatial JOIN: when only a few primitive AABBs changed,
// delete incident old pairs and re-evaluate those IDs. When most geometry
// changes, use a refitted BVH rather than a quadratic delta scan.
// This is a *fully exact* relational update, not a position forecast.
class DeltaJoin {
 std::vector<Bounds> previous_;
 Pairs pairs_;
 RefitBVH bvh_;
 Statistics stat_;
public:
 const Pairs& query(const std::vector<Bounds>& boxes){
  stat_={};
  if(!previous_.empty()&&previous_.size()!=boxes.size())
   throw std::runtime_error("delta join topology changed");
  std::vector<unsigned char> dirty(boxes.size(),0);
  size_t count=0;
  for(size_t i=0;i<boxes.size();++i){
   if(previous_.empty()||!identical(previous_[i],boxes[i])){
    dirty[i]=1;++count;
   }
  }
  if(!count){
   ++stat_.reuses;
   return pairs_;
  }
  if(previous_.empty()||count>std::max(size_t{1},boxes.size()/16)){
   pairs_=bvh_.query(boxes);
   auto s=bvh_.statistics();
   stat_.builds=s.builds;stat_.reuses=s.reuses;
   stat_.repairs=s.repairs;stat_.aabbTests=s.aabbTests;
  }else{
   pairs_.erase(std::remove_if(pairs_.begin(),pairs_.end(),
     [&](const Pair& p){return dirty[size_t(p.a)]||dirty[size_t(p.b)];}),
     pairs_.end());
   for(size_t i=0;i<boxes.size();++i)if(dirty[i]){
    for(size_t j=0;j<boxes.size();++j)if(j!=i){
     ++stat_.aabbTests;
     if(compatible(boxes[i],boxes[j])&&hits(boxes[i],boxes[j]))
      pairs_.push_back(canonical(boxes[i],boxes[j]));
    }
   }
   normalize(pairs_);++stat_.repairs;
  }
  previous_=boxes;
  stat_.events=count;
  return pairs_;
 }
 Statistics statistics()const{return stat_;}
};

// A *new* group-level certificate, not an invocation of Global Guard.
// Each contiguous patch stores a loose enclosing AABB. Until a patch exits
// its own envelope, its possible interacting patch neighbors are unchanged.
// On exit, repair only edges incident on that patch, then perform current
// exact primitive tests within cached plausible patch pairs.
//
// "signal" changes only the EXPANSION radius based on causal displacement
// and neighboring-patch activity. It never controls whether a required
// containment test is skipped. This is NOT a future-motion guarantee.
class PatchCertificate {
 struct Patch {
  size_t begin{},end{};
  Bounds envelope;
 };
 std::vector<Patch> patches_;
 std::vector<std::pair<int,int>> edges_;
 std::vector<Bounds> previous_;
 std::vector<double> previousActivity_;
 Pairs out_;
 Statistics stat_;
 bool signal_{};
 static constexpr size_t kPatchSize=16;
 static double movement(const Bounds& a,const Bounds& b){
  const double dx=center(a,0)-center(b,0);
  const double dy=center(a,1)-center(b,1);
  const double dz=center(a,2)-center(b,2);
  return std::sqrt(dx*dx+dy*dy+dz*dz);
 }
public:
 explicit PatchCertificate(bool signal=false):signal_(signal){}
 const Pairs& query(const std::vector<Bounds>& boxes){
  stat_={};
  if(boxes.empty())throw std::invalid_argument("empty patch snapshot");
  if(!previous_.empty()&&previous_.size()!=boxes.size())
   throw std::runtime_error("patch certificate topology changed");
  if(patches_.empty()){
   for(size_t begin=0;begin<boxes.size();begin+=kPatchSize)
    patches_.push_back({begin,std::min(boxes.size(),begin+kPatchSize),{}});
   previousActivity_.assign(patches_.size(),0.);
  }
  std::vector<double> activity(patches_.size(),0.);
  if(!previous_.empty())
   for(size_t p=0;p<patches_.size();++p){
    const auto& patch=patches_[p];
    for(size_t k=patch.begin;k<patch.end;++k){
     if(boxes[k].id!=previous_[k].id)
      throw std::runtime_error("patch certificate stable-ID mismatch");
     activity[p]=std::max(activity[p],movement(boxes[k],previous_[k]));
    }
   }
  std::vector<unsigned char> changed(patches_.size(),0);
  size_t count=0;
  for(size_t p=0;p<patches_.size();++p){
   const auto& patch=patches_[p];
   const Bounds raw=envelope(boxes,patch.begin,patch.end);
   if(previous_.empty()||!contains(patch.envelope,raw)){
    double pad=.03;
    if(signal_&&!previous_.empty()){
     const double left=p?activity[p-1]:activity[p];
     const double right=p+1<activity.size()?activity[p+1]:activity[p];
     const double contrast=std::max(std::abs(activity[p]-left),
                                    std::abs(activity[p]-right));
     const double impulse=std::abs(activity[p]-previousActivity_[p]);
     pad=std::clamp(.012+1.4*activity[p]+.6*impulse+.3*contrast,.012,.12);
    }
    patches_[p].envelope=envelope(boxes,patch.begin,patch.end,pad);
    changed[p]=1;++count;
   }
  }
  if(count){
   edges_.erase(std::remove_if(edges_.begin(),edges_.end(),
      [&](const auto& e){return changed[size_t(e.first)]||changed[size_t(e.second)];}),
      edges_.end());
   for(size_t i=0;i<patches_.size();++i)if(changed[i]){
    for(size_t j=0;j<patches_.size();++j){
     const auto& a=patches_[i].envelope;
     const auto& b=patches_[j].envelope;
     ++stat_.aabbTests;
     if(i==j||hits(a,b))
      edges_.emplace_back(int(std::min(i,j)),int(std::max(i,j)));
    }
   }
   std::sort(edges_.begin(),edges_.end());
   edges_.erase(std::unique(edges_.begin(),edges_.end()),edges_.end());
   ++stat_.repairs;
   if(previous_.empty())++stat_.builds;
  }else ++stat_.reuses;
  out_.clear();
  for(const auto& edge:edges_){
   const auto& a=patches_[size_t(edge.first)];
   const auto& b=patches_[size_t(edge.second)];
   for(size_t i=a.begin;i<a.end;++i){
    const size_t start=edge.first==edge.second?i+1:b.begin;
    for(size_t j=start;j<b.end;++j){
     ++stat_.aabbTests;
     if(compatible(boxes[i],boxes[j])&&hits(boxes[i],boxes[j]))
      out_.push_back(canonical(boxes[i],boxes[j]));
    }
   }
  }
  normalize(out_);
  previous_=boxes;
  previousActivity_=std::move(activity);
  stat_.events=count;
  return out_;
 }
 Statistics statistics()const{return stat_;}
};
} // namespace ccp::experiment
