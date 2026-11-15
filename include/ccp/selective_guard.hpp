#pragma once
// Selective global guard repair. Compared to a global cache that rebuilds on
// ANY envelope escape, update only pair blocks touching escaped primitives.
// The cached full pair set remains conservative because unchanged envelopes
// still contain the current AABBs; dirty envelopes are refreshed and every
// dirty-versus-any candidate is regenerated before returning output.
// Broad-phase contract: inclusive overlap of currently margin-padded boxes;
// this does not provide continuous collision detection or narrow-phase truth.
#include "ccp/live_cloth.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <unordered_set>
#include <vector>
namespace ccp::research {
class SelectiveGuard {
public:
 struct Metrics {
  std::size_t fullBuilds{},partialRepairs{},cacheReuses{},dirtyObjects{},
    pairChecks{},cachedPairs{},exactPairs{},rejectedCached{};
 };
 explicit SelectiveGuard(double margin=.025,double displacementBudget=.045)
  :margin_(margin),budget_(displacementBudget){
  if(!std::isfinite(margin)||margin<0||!std::isfinite(displacementBudget)||displacementBudget<0)
   throw std::invalid_argument("invalid selective guard margin/budget");
 }
 void reset(){valid_=false;ids_.clear();envelopes_.clear();cached_.clear();output_.clear();}
 const std::vector<ccp::live::Pair>& query(
     const std::vector<ccp::live::Bounds>& raw,Metrics& m){
  using ccp::live::Bounds;
  using ccp::live::Pair;
  // Validate before mutating any certificate.
  if(raw.size()>size_t(std::numeric_limits<int>::max()))
   throw std::invalid_argument("selective guard too many primitives");
  std::unordered_set<int> seen;seen.reserve(raw.size()*2);
  std::vector<Bounds> tight;tight.reserve(raw.size());
  std::vector<int> ids;ids.reserve(raw.size());
  for(size_t i=0;i<raw.size();++i){
   Bounds b=raw[i];
   if(!seen.insert(b.id).second)throw std::invalid_argument("duplicate primitive IDs");
   if(!finite(b)||b.min.x>b.max.x||b.min.y>b.max.y||b.min.z>b.max.z)
    throw std::invalid_argument("invalid broad-phase coordinates");
   ids.push_back(b.id);b.id=int(i);
   b=expanded(b,margin_*.5);
   if(!finite(b))throw std::invalid_argument("padded coordinates overflow");
   tight.push_back(b);
  }
  bool full=!valid_||ids!=ids_;
  std::vector<unsigned char> dirty(raw.size(),0);
  std::size_t dirtyCount=0;
  if(!full){
   for(size_t i=0;i<raw.size();++i){
    if(!contains(envelopes_[i],tight[i])){
     dirty[i]=1;++dirtyCount;
    }
   }
   if(dirtyCount>std::max(size_t(4),raw.size()/8))full=true;
  }
  if(full){
   std::vector<Bounds> next=tight;
   for(size_t i=0;i<next.size();++i){
    if(raw[i].movable)next[i]=expanded(next[i],budget_);
    if(!finite(next[i]))throw std::invalid_argument("guard expansion overflow");
   }
   auto candidate=ccp::live::candidates(next,true,&m.pairChecks);
   envelopes_=std::move(next);cached_=std::move(candidate);
   ids_=std::move(ids);valid_=true;++m.fullBuilds;
  }else if(dirtyCount){
   // Refresh only escaped envelopes. The retained original cache blocks
   // represent pairs of objects whose envelopes have not changed.
   auto next=envelopes_;
   for(size_t i=0;i<next.size();++i)if(dirty[i]){
    next[i]=tight[i];
    if(raw[i].movable)next[i]=expanded(next[i],budget_);
    if(!finite(next[i]))throw std::invalid_argument("guard expansion overflow");
   }
   std::vector<Pair> rebuilt;
   rebuilt.reserve(cached_.size());
   for(auto p:cached_)if(!dirty[size_t(p.a)]&&!dirty[size_t(p.b)])
    rebuilt.push_back(p);
   // Every pair with at least one dirty endpoint is generated exactly once.
   for(size_t i=0;i<next.size();++i)if(dirty[i]){
    for(size_t j=0;j<next.size();++j){
     if(j==i||(dirty[j]&&j<i))continue;
     const auto& a=next[i];const auto& b=next[j];
     if(!eligible(a,b))continue;
     ++m.pairChecks;
     if(overlap(a,b))rebuilt.push_back({
      int(std::min(i,j)),int(std::max(i,j))});
    }
   }
   std::sort(rebuilt.begin(),rebuilt.end());
   rebuilt.erase(std::unique(rebuilt.begin(),rebuilt.end()),rebuilt.end());
   envelopes_=std::move(next);cached_=std::move(rebuilt);
   ++m.partialRepairs;m.dirtyObjects+=dirtyCount;
  }else ++m.cacheReuses;
  output_.clear();output_.reserve(cached_.size());
  for(auto p:cached_){
   if(overlap(tight[size_t(p.a)],tight[size_t(p.b)])&&
      eligible(tight[size_t(p.a)],tight[size_t(p.b)])){
    const int ia=ids_[size_t(p.a)],ib=ids_[size_t(p.b)];
    output_.push_back({std::min(ia,ib),std::max(ia,ib)});
   }else ++m.rejectedCached;
  }
  std::sort(output_.begin(),output_.end());
  output_.erase(std::unique(output_.begin(),output_.end()),output_.end());
  m.cachedPairs=cached_.size();m.exactPairs=output_.size();
  return output_;
 }
private:
 static bool finite(const ccp::live::Bounds& a){
  return std::isfinite(a.min.x)&&std::isfinite(a.min.y)&&std::isfinite(a.min.z)&&
    std::isfinite(a.max.x)&&std::isfinite(a.max.y)&&std::isfinite(a.max.z);
 }
 static ccp::live::Bounds expanded(ccp::live::Bounds b,double x){
  b.min.x-=x;b.min.y-=x;b.min.z-=x;
  b.max.x+=x;b.max.y+=x;b.max.z+=x;return b;
 }
 static bool overlap(const ccp::live::Bounds& a,const ccp::live::Bounds& b){
  return a.min.x<=b.max.x&&b.min.x<=a.max.x&&
    a.min.y<=b.max.y&&b.min.y<=a.max.y&&a.min.z<=b.max.z&&b.min.z<=a.max.z;
 }
 static bool eligible(const ccp::live::Bounds& a,const ccp::live::Bounds& b){
  return (a.mask&b.category)!=0&&(b.mask&a.category)!=0;
 }
 static bool contains(const ccp::live::Bounds& e,const ccp::live::Bounds& b){
  return e.category==b.category&&e.mask==b.mask&&e.movable==b.movable&&
  e.min.x<=b.min.x&&e.min.y<=b.min.y&&e.min.z<=b.min.z&&
  e.max.x>=b.max.x&&e.max.y>=b.max.y&&e.max.z>=b.max.z;
 }
 double margin_{},budget_{};
 bool valid_{};
 std::vector<int> ids_;
 std::vector<ccp::live::Bounds> envelopes_;
 std::vector<ccp::live::Pair> cached_,output_;
};
}
