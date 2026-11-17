#pragma once
// CBP Phase 2 (experimental): conservative cross-region potential-contact
// certificates with INDEXED dirty-object incident-edge repair.
//
// Invariant after query:
//  (1) every current B_i is contained in stored envelope E_i;
//  (2) graph P contains every compatible CROSS-REGION envelope overlap;
//  (3) output = exact current intersections filtered from P.
// Region reassignment or primitive count changes rebuilds P. Metadata
// changes and envelope escapes synchronously repair incident edges.
// This is exact for DISCRETE current AABBs; it is NOT continuous CCD.
// NOTE: The dense-scene O(|P|) filter can cost more than a direct BVH.
#include "ccp/cbp_spatial_index.hpp"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <set>
#include <stdexcept>
#include <vector>

namespace ccp::cbp {
class CertifiedCrossCache {
public:
 using Bounds=ccp::live::Bounds;
 using Pair=ccp::live::Pair;
 using Pairs=std::vector<Pair>;
 struct Work {
  std::size_t graphBuilds{},quietReuses{},dirtyPrimitives{},
              incidentEdgesRemoved{},incidentEdgesInserted{},
              currentPairTests{},potentialPairCount{},
              exactPairs{},indexNodeVisits{},indexPrimitiveTests{},
              indexLeafUpdates{},indexAncestorRefits{};
 };
private:
 struct Signature {
  std::uint32_t category{},mask{};
  bool movable{};
  bool operator==(const Signature&)const=default;
 };
 std::vector<Bounds> covers_;
 std::vector<Signature> signatures_;
 std::vector<int> labels_;
 // Sorted adjacency lists allow removing only incident edges. The
 // std::set provides stable, canonical exact pair iteration.
 std::vector<std::vector<int>> neighbors_;
 std::set<Pair> potential_;
 SharedCrossBVH index_;
 bool initialized_{};
 double padding_{.045};
 Work work_{};

 static bool valid(const Bounds& b){
  const double v[]={b.min.x,b.min.y,b.min.z,b.max.x,b.max.y,b.max.z};
  return std::all_of(std::begin(v),std::end(v),
                     [](double d){return std::isfinite(d);}) &&
         b.min.x<=b.max.x && b.min.y<=b.max.y && b.min.z<=b.max.z;
 }
 static bool overlaps(const Bounds& a,const Bounds& b){
  return a.min.x<=b.max.x&&b.min.x<=a.max.x&&
         a.min.y<=b.max.y&&b.min.y<=a.max.y&&
         a.min.z<=b.max.z&&b.min.z<=a.max.z;
 }
 static bool contains(const Bounds& envelope,const Bounds& current){
  return envelope.min.x<=current.min.x&&envelope.min.y<=current.min.y&&
         envelope.min.z<=current.min.z&&envelope.max.x>=current.max.x&&
         envelope.max.y>=current.max.y&&envelope.max.z>=current.max.z;
 }
 static bool compatible(const Bounds& a,const Bounds& b){
  return (a.mask&b.category)!=0&&(b.mask&a.category)!=0;
 }
 static Signature signature(const Bounds& b){
  return {b.category,b.mask,b.movable};
 }
 static double lower(double value,double p){
  const double expanded=value-p;
  return std::isfinite(expanded)?expanded:-std::numeric_limits<double>::max();
 }
 static double upper(double value,double p){
  const double expanded=value+p;
  return std::isfinite(expanded)?expanded:std::numeric_limits<double>::max();
 }
 Bounds envelope(Bounds b)const{
  b.min.x=lower(b.min.x,padding_);b.min.y=lower(b.min.y,padding_);
  b.min.z=lower(b.min.z,padding_);
  b.max.x=upper(b.max.x,padding_);b.max.y=upper(b.max.y,padding_);
  b.max.z=upper(b.max.z,padding_);
  return b;
 }
 void add(Pair p){
  if(!potential_.insert(p).second)return;
  const auto insertSorted=[&](int a,int b){
   auto& list=neighbors_[static_cast<std::size_t>(a)];
   list.insert(std::lower_bound(list.begin(),list.end(),b),b);
  };
  insertSorted(p.a,p.b);
  insertSorted(p.b,p.a);
  ++work_.incidentEdgesInserted;
 }
 void eraseIncidents(int id){
  const auto ii=static_cast<std::size_t>(id);
  const auto old=std::move(neighbors_[ii]);
  neighbors_[ii].clear();
  for(const int j:old){
   const Pair p{std::min(id,j),std::max(id,j)};
   if(potential_.erase(p)!=1)
    throw std::logic_error("CBP certificate adjacency corrupted");
   auto& other=neighbors_[static_cast<std::size_t>(j)];
   const auto it=std::lower_bound(other.begin(),other.end(),id);
   if(it==other.end()||*it!=id)
    throw std::logic_error("CBP certificate neighbor symmetry violated");
   other.erase(it);
   ++work_.incidentEdgesRemoved;
  }
 }
 void fullBuild(const std::vector<Bounds>& boxes,
                const std::vector<int>& labels){
  covers_.clear();
  covers_.reserve(boxes.size());
  signatures_.clear();
  signatures_.reserve(boxes.size());
  neighbors_.assign(boxes.size(),{});
  potential_.clear();
  labels_=labels;
  for(const auto& b:boxes){
   covers_.push_back(envelope(b));
   signatures_.push_back(signature(b));
  }
  Pairs all;
  index_.clear();
  index_.appendCrossPairs(covers_,labels_,all);
  for(const auto p:all)add(p);
  initialized_=true;
  ++work_.graphBuilds;
 }
 void accountIndex(){
  const auto& w=index_.work();
  work_.indexNodeVisits=w.nodePairVisits;
  work_.indexPrimitiveTests=w.primitiveTests;
  work_.indexLeafUpdates=w.changedLeaves;
  work_.indexAncestorRefits=w.ancestorRefits;
 }
public:
 void reset(){
  covers_.clear();signatures_.clear();labels_.clear();
  neighbors_.clear();potential_.clear();index_.clear();
  initialized_=false;work_={};
 }
 void setPadding(double p){
  if(!std::isfinite(p)||p<0.)
   throw std::invalid_argument("CBP cover padding must be finite and nonnegative");
  if(p!=padding_){padding_=p;reset();}
 }
 double padding()const{return padding_;}
 Work work()const{return work_;}
 void appendExactCrossPairs(const std::vector<Bounds>& boxes,
                            const std::vector<int>& labels,Pairs& out){
  if(boxes.size()!=labels.size())
   throw std::invalid_argument("CBP certificates need one region per AABB");
  for(std::size_t i=0;i<boxes.size();++i){
   if(boxes[i].id!=static_cast<int>(i)||!valid(boxes[i]))
    throw std::invalid_argument("CBP certificates require finite indexed valid AABBs");
  }
  work_={};
  if(!initialized_||labels!=labels_||boxes.size()!=covers_.size()){
   fullBuild(boxes,labels);
   accountIndex();
  }else{
   std::vector<int> dirty;
   for(std::size_t i=0;i<boxes.size();++i){
    if(!contains(covers_[i],boxes[i])||
       signatures_[i]!=signature(boxes[i]))
     dirty.push_back(static_cast<int>(i));
   }
   work_.dirtyPrimitives=dirty.size();
   if(dirty.empty()){
    ++work_.quietReuses;
   }else{
    // First invalidate ALL old incident edges, then replace all dirty
    // covers and refit BVH before running any repair query.
    for(const int id:dirty)eraseIncidents(id);
    for(const int id:dirty){
     covers_[static_cast<std::size_t>(id)]=
      envelope(boxes[static_cast<std::size_t>(id)]);
     signatures_[static_cast<std::size_t>(id)]=
      signature(boxes[static_cast<std::size_t>(id)]);
    }
    Pairs newlyPossible;
    index_.appendIncidentCrossPairs(covers_,labels_,dirty,newlyPossible);
    for(const auto p:newlyPossible)add(p);
    accountIndex();
   }
  }
  // The graph only stores cross-region compatible envelope neighbors.
  // Current AABBs are checked independently; cached contact guesses
  // never replace real geometry checks.
  for(const Pair p:potential_){
   ++work_.currentPairTests;
   if(compatible(boxes[static_cast<std::size_t>(p.a)],
                 boxes[static_cast<std::size_t>(p.b)]) &&
      overlaps(boxes[static_cast<std::size_t>(p.a)],
               boxes[static_cast<std::size_t>(p.b)])){
    out.push_back(p);
    ++work_.exactPairs;
   }
  }
  work_.potentialPairCount=potential_.size();
 }
};
} // namespace ccp::cbp
