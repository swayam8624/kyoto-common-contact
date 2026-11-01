#include "ccp/research_multicloth.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <limits>
#include <map>
#include <numeric>
#include <optional>
#include <stdexcept>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <vector>
namespace ccp::research {
namespace {
using Clock=std::chrono::steady_clock;
constexpr int kSide=13;
constexpr int kIterations=5;
constexpr double kDt=1.0/90.0;
constexpr double kThickness=0.062;
// Self-contact uses the same conservative 0.075m AABB query margin. A
// positive thickness and nonlocal topological exclusions are necessary
// for folding cloth, but this is NOT continuous collision detection.
constexpr double kSelfThickness=0.070;
constexpr double kMargin=0.075;
// Stability contracts for the discrete XPBD-style test solver:
// Contact projection is positional, not a perfectly elastic collision.
// Limit its per-particle work in each iteration, and dissipate the associated
// numerical velocity rather than reinjecting every correction next frame.
constexpr double kWindAcceleration=1.6; // m/s^2, integrated using dt^2
constexpr double kVelocityDamping=.982;
constexpr double kMaxFreeVelocityStep=.060; // m per 1/90-second frame
constexpr double kContactBudgetPerIteration=.030; // m, per dynamic vertex
constexpr double kContactDisplacementDamping=.94; // 94% inelastic contact
Vec3 add(Vec3 a,Vec3 b){return {a.x+b.x,a.y+b.y,a.z+b.z};}
Vec3 sub(Vec3 a,Vec3 b){return {a.x-b.x,a.y-b.y,a.z-b.z};}
Vec3 mul(Vec3 a,double x){return {a.x*x,a.y*x,a.z*x};}
double dot(Vec3 a,Vec3 b){return a.x*b.x+a.y*b.y+a.z*b.z;}
double len2(Vec3 a){return dot(a,a);}
double norm(Vec3 a){return std::sqrt(len2(a));}
Vec3 cross(Vec3 a,Vec3 b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
Vec3 normalized(Vec3 a,Vec3 fallback={0,0,1}){const auto n=norm(a);return n>1e-12?mul(a,1.0/n):fallback;}
Bounds expand(Bounds b,double d){
 b.min=sub(b.min,{d,d,d});b.max=add(b.max,{d,d,d});return b;
}
bool overlap(const Bounds& a,const Bounds& b) {
 return a.min.x<=b.max.x&&b.min.x<=a.max.x&&
        a.min.y<=b.max.y&&b.min.y<=a.max.y&&
        a.min.z<=b.max.z&&b.min.z<=a.max.z;
}
bool eligible(const Bounds& a,const Bounds& b){
 return (a.mask&b.category)!=0&&(b.mask&a.category)!=0;
}
bool contains(const Bounds& e,const Bounds& b){
 return e.id==b.id&&e.category==b.category&&e.mask==b.mask&&e.movable==b.movable&&
 e.min.x<=b.min.x&&e.min.y<=b.min.y&&e.min.z<=b.min.z&&
 e.max.x>=b.max.x&&e.max.y>=b.max.y&&e.max.z>=b.max.z;
}
Bounds unite(const Bounds& a,const Bounds& b){
 Bounds result=a;
 result.min={std::min(a.min.x,b.min.x),std::min(a.min.y,b.min.y),std::min(a.min.z,b.min.z)};
 result.max={std::max(a.max.x,b.max.x),std::max(a.max.y,b.max.y),std::max(a.max.z,b.max.z)};
 return result;
}
double maxChange(const Bounds&a,const Bounds&b){
 return std::max({std::abs(a.min.x-b.min.x),std::abs(a.min.y-b.min.y),
  std::abs(a.min.z-b.min.z),std::abs(a.max.x-b.max.x),
  std::abs(a.max.y-b.max.y),std::abs(a.max.z-b.max.z)});
}
std::uint64_t pairKey(std::size_t i,std::size_t j){
 return (static_cast<std::uint64_t>(i)<<32)|static_cast<std::uint64_t>(j);
}
// Closest point on triangle, including degenerate fallback. Barycentric
// weights are used to distribute positional projection back to its vertices.
struct Closest {Vec3 p;std::array<double,3> w;};
Closest segment(Vec3 p,Vec3 a,Vec3 b){
 const Vec3 v=sub(b,a);const double d=len2(v);
 const double t=d>1e-25?std::clamp(dot(sub(p,a),v)/d,0.0,1.0):0.0;
 return {add(a,mul(v,t)),{1.0-t,t,0}};
}
Closest trianglePoint(Vec3 p,Vec3 a,Vec3 b,Vec3 c){
 const Vec3 ab=sub(b,a),ac=sub(c,a),ap=sub(p,a);
 const double d1=dot(ab,ap),d2=dot(ac,ap);
 if(d1<=0&&d2<=0)return {a,{1,0,0}};
 const Vec3 bp=sub(p,b);
 const double d3=dot(ab,bp),d4=dot(ac,bp);
 if(d3>=0&&d4<=d3)return {b,{0,1,0}};
 const double vc=d1*d4-d3*d2;
 if(vc<=0&&d1>=0&&d3<=0){const double t=d1/(d1-d3);return {add(a,mul(ab,t)),{1-t,t,0}};}
 const Vec3 cp=sub(p,c);
 const double d5=dot(ab,cp),d6=dot(ac,cp);
 if(d6>=0&&d5<=d6)return {c,{0,0,1}};
 const double vb=d5*d2-d1*d6;
 if(vb<=0&&d2>=0&&d6<=0){const double t=d2/(d2-d6);return {add(a,mul(ac,t)),{1-t,0,t}};}
 const double va=d3*d6-d5*d4;
 if(va<=0&&(d4-d3)>=0&&(d5-d6)>=0){
  const double t=(d4-d3)/((d4-d3)+(d5-d6));
  return {add(b,mul(sub(c,b),t)),{0,1-t,t}};
 }
 const double den=va+vb+vc;
 if(std::abs(den)<1e-24) {
  auto q0=segment(p,a,b),q1=segment(p,b,c),q2=segment(p,c,a);
  const double e0=len2(sub(q0.p,p)),e1=len2(sub(q1.p,p)),e2=len2(sub(q2.p,p));
  if(e0<=e1&&e0<=e2)return q0;
  if(e1<=e2)return {q1.p,{0,q1.w[0],q1.w[1]}};
  return {q2.p,{q2.w[1],0,q2.w[0]}};
 }
 const double v=vb/den,w=vc/den;
 return {add(add(a,mul(ab,v)),mul(ac,w)),{1-v-w,v,w}};
}
struct SegPair {Vec3 a,b;double s{},t{};};
SegPair closestSegments(Vec3 p1,Vec3 q1,Vec3 p2,Vec3 q2){
 const Vec3 d1=sub(q1,p1),d2=sub(q2,p2),r=sub(p1,p2);
 const double a=dot(d1,d1),e=dot(d2,d2),f=dot(d2,r);
 double s=0,t=0;
 if(a<=1e-25&&e<=1e-25) return {p1,p2,0,0};
 if(a<=1e-25) t=std::clamp(f/e,0.0,1.0);
 else {
  const double c=dot(d1,r);
  if(e<=1e-25)s=std::clamp(-c/a,0.0,1.0);
  else {
   const double b=dot(d1,d2),den=a*e-b*b;
   if(std::abs(den)>1e-25)s=std::clamp((b*f-c*e)/den,0.0,1.0);
   t=(b*s+f)/e;
   if(t<0){t=0;s=std::clamp(-c/a,0.0,1.0);}
   else if(t>1){t=1;s=std::clamp((b-c)/a,0.0,1.0);}
  }
 }
 return {add(p1,mul(d1,s)),add(p2,mul(d2,t)),s,t};
}
// Segment/triangle piercing can be missed by nearest-vertex-only tests:
// the penetrating edge may already have crossed the face and its endpoints
// can be farther than the cloth's contact thickness. This detects ACTUAL
// current-segment crossings, not continuous time-of-impact.
struct SegmentFaceHit {
 Vec3 p{};
 std::array<double,3> bary{};
 double edgeParameter{};
};
std::optional<SegmentFaceHit> segmentFace(Vec3 p0,Vec3 p1,Vec3 a,Vec3 b,Vec3 c){
 const Vec3 ab=sub(b,a),ac=sub(c,a),n=cross(ab,ac),direction=sub(p1,p0);
 const double n2=len2(n),denom=dot(n,direction);
 if(n2<1e-19||std::abs(denom)<1e-12)return std::nullopt;
 const double t=dot(n,sub(a,p0))/denom;
 if(t<0||t>1)return std::nullopt;
 const Vec3 q=add(p0,mul(direction,t)),v=sub(q,a);
 const double d00=dot(ab,ab),d01=dot(ab,ac),d11=dot(ac,ac);
 const double d20=dot(v,ab),d21=dot(v,ac),den=d00*d11-d01*d01;
 if(std::abs(den)<1e-20)return std::nullopt;
 const double v1=(d11*d20-d01*d21)/den,v2=(d00*d21-d01*d20)/den;
 if(v1<-.00001||v2<-.00001||v1+v2>1.00001)return std::nullopt;
 return SegmentFaceHit{q,{1.-v1-v2,v1,v2},t};
}
}
void RegionalCandidates::reset(){
 regions_.clear();cachedRegionPairs_.clear();output_.clear();
 knownIds_.clear();knownRegions_.clear();previousMargin_=-1;
}
const std::vector<Pair>& RegionalCandidates::query(const std::vector<Primitive>& items,
 double margin,bool adaptive,Metrics& metrics,bool hierarchicalRegionSweep,
 bool collectFeedback){
 const auto start=Clock::now();
 if(!std::isfinite(margin)||margin<0)throw std::invalid_argument("invalid region query margin");
 // Caller supplies unique stable primitive IDs. A topology change invalidates
 // all dependent caches, not just whichever group happens to be visited.
 std::vector<int> ids,groupIds;
 ids.reserve(items.size());groupIds.reserve(items.size());
 std::map<int,std::vector<int>> partition;
 std::unordered_set<int> uniqueIds;
 uniqueIds.reserve(items.size()*2);
 for(std::size_t i=0;i<items.size();++i){
  const auto& b=items[i].box;
  // A certified pair cache needs unique stable IDs and finite AABBs.
  // Otherwise an ID-to-index map can silently hide a necessary pair.
  if(!uniqueIds.insert(b.id).second)
   throw std::invalid_argument("duplicate broad-phase primitive ID");
  const double values[]={b.min.x,b.min.y,b.min.z,b.max.x,b.max.y,b.max.z};
  for(double v:values)if(!std::isfinite(v))
   throw std::invalid_argument("non-finite broad-phase AABB coordinate");
  if(b.min.x>b.max.x||b.min.y>b.max.y||b.min.z>b.max.z)
   throw std::invalid_argument("inverted broad-phase AABB");
  ids.push_back(b.id);groupIds.push_back(items[i].region);
  partition[items[i].region].push_back(static_cast<int>(i));
 }
 if(ids!=knownIds_||groupIds!=knownRegions_||margin!=previousMargin_){
  reset();knownIds_=ids;knownRegions_=groupIds;previousMargin_=margin;
  for(const auto& [group,members]:partition){
   Region region;region.id=group;region.itemIndices=members;regions_.push_back(std::move(region));
  }
  metrics.lastEvent="REGION_TOPOLOGY_OR_REQUEST_REBUILD";
 }
 std::vector<Bounds> current;current.reserve(items.size());
 std::unordered_map<int,int> byId;byId.reserve(items.size()*2);
 for(std::size_t i=0;i<items.size();++i){
  current.push_back(expand(items[i].box,margin*0.5));byId[items[i].box.id]=static_cast<int>(i);
 }
 double budgetSum=0;
 for(auto& region:regions_){
  double motion=0;
  if(region.previous.size()==region.itemIndices.size())
   for(std::size_t j=0;j<region.itemIndices.size();++j)
    motion=std::max(motion,maxChange(current[region.itemIndices[j]],region.previous[j]));
  region.motionEwma=region.motionEwma*.65+motion*.35;
  if(collectFeedback){
   auto& feedback=metrics.regionWork[region.id];
   feedback.maxBoundMotion=std::max(feedback.maxBoundMotion,motion);
  }
  region.dirty=region.envelopes.size()!=region.itemIndices.size();
  if(!region.dirty){
   for(std::size_t j=0;j<region.itemIndices.size();++j)
    if(!contains(region.envelopes[j],current[region.itemIndices[j]])){
     region.dirty=true;break;
    }
  }
  if(region.dirty){
   // Provisional contact-work-aware region budget:
   // motion raises the guard; past candidate inflation shrinks it.
   const double motionBudget=adaptive?
     std::clamp(0.016+2.8*region.motionEwma,0.016,0.075):0.045;
   region.budget=adaptive?
     std::clamp(motionBudget/(1+1.2*region.previousInflation),0.010,0.075):0.045;
   region.envelopes.clear();region.previous.clear();
   for(int idx:region.itemIndices){
    const auto& raw=current[static_cast<std::size_t>(idx)];
    region.envelopes.push_back(expand(raw,items[static_cast<std::size_t>(idx)].box.movable?region.budget:0.0));
   }
   ++metrics.dirtyRegions;
   if(collectFeedback)++metrics.regionWork[region.id].invalidations;
  } else {
   region.previous.clear();
   for(int idx:region.itemIndices)region.previous.push_back(current[static_cast<std::size_t>(idx)]);
  }
  // Keep previous tight bounds even for a rebuilt region. The historic
  // envelope remains independent of the subsequent current configuration.
  if(region.dirty){
   region.previous.clear();
   for(int idx:region.itemIndices)region.previous.push_back(current[static_cast<std::size_t>(idx)]);
  }
  if(region.envelopes.empty())throw std::runtime_error("empty region unsupported");
  region.unionEnvelope=region.envelopes[0];
  for(std::size_t j=1;j<region.envelopes.size();++j)
   region.unionEnvelope=unite(region.unionEnvelope,region.envelopes[j]);
  budgetSum+=region.budget;
 }
 metrics.meanRegionBudget=regions_.empty()?0:budgetSum/regions_.size();
 metrics.regionGuards.clear();
 for(const auto& region:regions_)metrics.regionGuards.push_back(region.unionEnvelope);
 output_.clear();
 // Hierarchical alternative to the original O(R^2) scan of ALL region
 // pairs. First prune candidate PATCH PAIRS by sweeping union-envelope
 // intervals; only the surviving patch pairs enter the cached primitive
 // pair stage. The input envelopes contain every current padded AABB, so
 // this coarse pruning cannot remove a required primitive overlap.
 //
 // The original all-pairs scan stays available as an explicit ablation.
 using RegionPair=std::pair<std::size_t,std::size_t>;
 std::vector<RegionPair> activeRegionPairs;
 const std::size_t regionCount=regions_.size();
 const std::size_t possibleBlocks=regionCount*(regionCount+1)/2;
 if(hierarchicalRegionSweep){
  std::vector<std::size_t> order(regionCount);
  std::iota(order.begin(),order.end(),std::size_t{0});
  std::sort(order.begin(),order.end(),[&](std::size_t a,std::size_t b){
   const double x=regions_[a].unionEnvelope.min.x;
   const double y=regions_[b].unionEnvelope.min.x;
   return x==y?a<b:x<y;
  });
  for(std::size_t i=0;i<order.size();++i){
   const auto a=order[i];
   activeRegionPairs.emplace_back(a,a); // conservative even for one patch
   for(std::size_t j=i+1;j<order.size();++j){
    const auto b=order[j];
    ++metrics.regionTopLevelTests;
    // Sorting means every later minimum-X lies beyond A's maximum-X.
    if(regions_[b].unionEnvelope.min.x>regions_[a].unionEnvelope.max.x)
     break;
    if(!overlap(regions_[a].unionEnvelope,regions_[b].unionEnvelope))
     continue;
    activeRegionPairs.emplace_back(std::min(a,b),std::max(a,b));
   }
  }
  std::sort(activeRegionPairs.begin(),activeRegionPairs.end());
 }else{
  activeRegionPairs.reserve(possibleBlocks);
  for(std::size_t i=0;i<regionCount;++i)
   for(std::size_t j=i;j<regionCount;++j)
    activeRegionPairs.emplace_back(i,j);
 }
 metrics.regionTopLevelOverlaps=activeRegionPairs.size();
 metrics.regionPairsSkipped=possibleBlocks-activeRegionPairs.size();
 // Only cached blocks in the active top-level frontier need to be retained.
 // Evicting a disjoint block is safe: if envelopes overlap later, either a
 // changed region triggers a rebuild or the absent block is rebuilt fresh.
 // This bounds memory to currently relevant region interactions.
 if(hierarchicalRegionSweep){
  std::unordered_set<std::uint64_t> activeKeys;
  activeKeys.reserve(activeRegionPairs.size()*2);
  for(const auto [i,j]:activeRegionPairs)activeKeys.insert(pairKey(i,j));
  for(auto it=cachedRegionPairs_.begin();it!=cachedRegionPairs_.end();){
   if(!activeKeys.contains(it->first)){
    it=cachedRegionPairs_.erase(it);
    ++metrics.regionCacheEvictions;
   }else ++it;
  }
 }
 // The second tier produces primitive candidate IDs, and revalidates
 // cached envelope pairs with CURRENT tight padded primitive AABBs.
 for(const auto [i,j]:activeRegionPairs){
  auto& a=regions_[i];auto& b=regions_[j];
  const auto key=pairKey(i,j);
  auto it=cachedRegionPairs_.find(key);
  const bool rebuild=a.dirty||b.dirty||it==cachedRegionPairs_.end();
  const auto checksBefore=metrics.broadAabbTests;
  if(rebuild){
   std::vector<Pair> list;
   if(overlap(a.unionEnvelope,b.unionEnvelope)) {
    for(std::size_t u=0;u<a.itemIndices.size();++u)
    for(std::size_t v=0;v<b.itemIndices.size();++v){
     if(i==j&&u>=v)continue;
     const auto ia=a.itemIndices[u],ib=b.itemIndices[v];
     const auto& ba=a.envelopes[u];const auto& bb=b.envelopes[v];
     if(!eligible(ba,bb))continue;
     ++metrics.broadAabbTests;
     if(overlap(ba,bb))
      list.push_back({std::min(items[ia].box.id,items[ib].box.id),
                      std::max(items[ia].box.id,items[ib].box.id)});
    }
   }
   std::sort(list.begin(),list.end());
   list.erase(std::unique(list.begin(),list.end()),list.end());
   it=cachedRegionPairs_.insert_or_assign(key,std::move(list)).first;
   ++metrics.regionalPairRebuilds;
  } else ++metrics.regionalPairReuses;
  std::size_t rejected=0,kept=0;
  for(const Pair p:it->second){
   const auto ia=byId.find(p.a),ib=byId.find(p.b);
   if(ia==byId.end()||ib==byId.end())throw std::runtime_error("stale region pair ID");
   const auto& ca=current[static_cast<std::size_t>(ia->second)];
   const auto& cb=current[static_cast<std::size_t>(ib->second)];
   ++metrics.broadAabbTests;
   if(overlap(ca,cb)&&eligible(ca,cb)){
    output_.push_back(p);++kept;
   } else ++rejected;
  }
  // Observed inflation enters the *next* budget decision. We use an EWMA
  // so one transient collision burst cannot permanently collapse the guard.
  const double ratio=it->second.empty()?0.0:
     double(rejected)/double(it->second.size());
  a.previousInflation=a.previousInflation*.94+ratio*.06;
  b.previousInflation=b.previousInflation*.94+ratio*.06;
  metrics.cachedPairsRejected+=rejected;
  if(collectFeedback){
   const auto built=metrics.broadAabbTests-checksBefore-it->second.size();
   const auto credit=[&](int id){
    auto& v=metrics.regionWork[id];
    v.aabbChecks+=built;
    v.cachedChecks+=it->second.size();
    v.candidateTouches+=kept;
    v.rejectedCandidates+=rejected;
   };
   credit(a.id);
   if(a.id!=b.id)credit(b.id);
  }
 }
 std::sort(output_.begin(),output_.end());
 output_.erase(std::unique(output_.begin(),output_.end()),output_.end());
 metrics.pairs=output_.size();++metrics.queryCount;
 if(metrics.lastEvent!="REGION_TOPOLOGY_OR_REQUEST_REBUILD")
  metrics.lastEvent=metrics.dirtyRegions?"LOCAL_REGION_REBUILDS":"CERTIFIED_REGION_REUSE";
 metrics.broadMs+=std::chrono::duration<double,std::milli>(Clock::now()-start).count();
 return output_;
}
MultiCloth::MultiCloth(int sheetCount,int sphereCount,Policy policy):policy_(policy){
 reset(sheetCount,sphereCount);
}
void MultiCloth::reset(int sheetCount,int sphereCount){
 if(sheetCount<1||sheetCount>3||sphereCount<1||sphereCount>2)
  throw std::invalid_argument("supported configuration: 1-3 cloths and 1-2 moving spheres");
 sheets_=sheetCount;balls_=sphereCount;frame_=0;
 positions_.clear();previous_.clear();inverseMasses_.clear();
 triangles_.clear();triangleSheets_.clear();triangleRegions_.clear();
 pinned_.clear();initialPins_.clear();constraints_.clear();spheres_.clear();
 grab_={};
 regional_.reset();global_.invalidate();metrics_={};
 const double spacing=2.2/(kSide-1);
 for(int layer=0;layer<sheets_;++layer){
  const int base=static_cast<int>(positions_.size());
  const double phi=layer*.13;
  // At t=0, each separate sheet must have physical clearance. The old
  // 0.046m layer separation was already SMALLER than kThickness=0.062m,
  // triggering thousands of violent contact responses on frame one.
  const double dz=2.05+.19*layer;
  for(int y=0;y<kSide;++y)
   for(int x=0;x<kSide;++x){
    const double u=(x-(kSide-1)*.5)*spacing;
    const double v=(y-(kSide-1)*.5)*spacing;
    Vec3 p{u+0.055*layer+0.055*v*phi,v+0.045*layer,dz+0.025*std::sin(u*2.0+v*1.7+phi)};
    positions_.push_back(p);previous_.push_back(p);initialPins_.push_back(p);
    const bool anchor=y==kSide-1&&(x==0||x==kSide-1);
    pinned_.push_back(anchor?1:0);
    inverseMasses_.push_back(anchor?0.0:1.0);
   }
  const int blocks=(kSide-1)/patchCellSpan_;
  for(int y=0;y<kSide-1;++y)
   for(int x=0;x<kSide-1;++x){
    const int a=base+y*kSide+x,b=a+1,c=a+kSide,d=c+1;
    for(auto tri:{std::array<int,3>{a,b,d},std::array<int,3>{a,d,c}}){
     triangles_.push_back(tri);triangleSheets_.push_back(layer);
     // Span three preserves the already-benchmarked reference region IDs.
     // Other spans use a disjoint range, including span one (144 patches
     // per sheet), avoiding collisions with the sphere region IDs.
     const int region=patchCellSpan_==3
       ? layer*100+(y/3)*4+x/3
       : 1000+layer*400+(y/patchCellSpan_)*blocks+x/patchCellSpan_;
     triangleRegions_.push_back(region);
    }
   }
  auto addConstraint=[&](int a,int b,double compliance){
   constraints_.push_back({a,b,norm(sub(positions_[a],positions_[b])),compliance,0.0});
  };
  for(int y=0;y<kSide;++y)
   for(int x=0;x<kSide;++x){
    const int i=base+y*kSide+x;
    if(x+1<kSide)addConstraint(i,i+1,1e-6);
    if(y+1<kSide)addConstraint(i,i+kSide,1e-6);
    if(x+1<kSide&&y+1<kSide){
     addConstraint(i,i+kSide+1,5e-6);
     addConstraint(i+1,i+kSide,5e-6);
    }
    if(x+2<kSide)addConstraint(i,i+2,1.5e-4);
    if(y+2<kSide)addConstraint(i,i+2*kSide,1.5e-4);
   }
 }
 updateSpheres(0);
 if(policy_==Policy::OnlineContactAware){
  online_.reset(sheets_);
  hierarchicalRegions_=online_.hierarchical();
  assignOnlineRegionIds();
 }
}
std::vector<Primitive> MultiCloth::currentPrimitives() const {return makePrimitives();}
void MultiCloth::setPolicy(Policy policy){
 if(policy_==policy)return;
 policy_=policy;
 if(policy_==Policy::OnlineContactAware){
  online_.reset(sheets_);
  hierarchicalRegions_=online_.hierarchical();
  assignOnlineRegionIds();
 }else{
  const int span=patchCellSpan_;
  patchCellSpan_=0;
  setPatchCellSpan(span);
 }
 regional_.reset();global_.invalidate();
}
void MultiCloth::setAdaptive(bool adaptive){adaptive_=adaptive;regional_.reset();}
void MultiCloth::setHierarchicalRegions(bool enabled){
 if(hierarchicalRegions_!=enabled){
  hierarchicalRegions_=enabled;
  regional_.reset();
 }
}
void MultiCloth::setPatchCellSpan(int cells){
 if(policy_==Policy::OnlineContactAware)
  throw std::invalid_argument("manual patch span not available in online policy");
 if(cells!=1&&cells!=2&&cells!=3&&cells!=4&&cells!=6&&cells!=12)
  throw std::invalid_argument("patch span must be a divisor of 12: 1, 2, 3, 4, 6, 12");
 if(cells==patchCellSpan_)return;
 patchCellSpan_=cells;
 const int blocks=(kSide-1)/cells;
 for(std::size_t ti=0;ti<triangleRegions_.size();++ti){
  const int sheet=triangleSheets_[ti];
  const int cell=int((ti%std::size_t(2*(kSide-1)*(kSide-1)))/2);
  const int y=cell/(kSide-1),x=cell%(kSide-1);
  triangleRegions_[ti]=cells==3
    ? sheet*100+(y/3)*4+x/3
    : 1000+sheet*400+(y/cells)*blocks+x/cells;
 }
 // Topology grouping changed, so every previous block certificate and
 // candidate list must be rejected before this new partition is queried.
 regional_.reset();
}

void MultiCloth::assignOnlineRegionIds(){
 for(std::size_t ti=0;ti<triangleRegions_.size();++ti){
  const int sheet=triangleSheets_[ti];
  const int cell=int((ti%std::size_t(2*(kSide-1)*(kSide-1)))/2);
  const int y=cell/(kSide-1),x=cell%(kSide-1);
  triangleRegions_[ti]=online_.regionForCell(sheet,x,y);
 }
 regional_.reset();
}
void MultiCloth::setAuditing(bool enable){auditing_=enable;}
void MultiCloth::setSelfCollisions(bool enabled){
 if(selfCollisions_==enabled)return;
 selfCollisions_=enabled;
 regional_.reset();
 global_.invalidate(); // eligibility masks now include/exclude same-sheet pairs
}
// Same-cloth triangles in the same cell or neighboring mesh cells share
// an edge, vertex or structural one-ring neighborhood. Excluding them
// prevents the cloth from repelling its own resting local mesh.
bool MultiCloth::nonlocalSelfPair(int first,int second) const{
 if(first==second||first<0||second<0 ||
    first>=static_cast<int>(triangles_.size())||
    second>=static_cast<int>(triangles_.size()))return false;
 if(triangleSheets_[first]!=triangleSheets_[second])return false;
 constexpr int cellsPerSheet=2*(kSide-1)*(kSide-1);
 const int ca=(first%cellsPerSheet)/2,cb=(second%cellsPerSheet)/2;
 const int ax=ca%(kSide-1),ay=ca/(kSide-1);
 const int bx=cb%(kSide-1),by=cb/(kSide-1);
 return std::abs(ax-bx)>1||std::abs(ay-by)>1;
}
bool MultiCloth::beginGrab(int ti,std::array<double,3> bary,Vec3 pickedPoint){
 if(ti<0||ti>=static_cast<int>(triangles_.size()))return false;
 for(double x:bary)if(!std::isfinite(x)||x<0||x>1)return false;
 const double sum=bary[0]+bary[1]+bary[2];
 if(std::abs(sum-1.)>1e-5)return false;
 const auto tri=triangles_[static_cast<std::size_t>(ti)];
 double weightedInverseMass=0;
 for(int j=0;j<3;++j)
  weightedInverseMass+=bary[j]*bary[j]*inverseMasses_[tri[j]];
 if(weightedInverseMass<1e-10)return false;
 if(!std::isfinite(pickedPoint.x)||!std::isfinite(pickedPoint.y)||
    !std::isfinite(pickedPoint.z))return false;
 grab_={true,ti,bary,pickedPoint,pickedPoint};
 return true;
}
void MultiCloth::updateGrab(Vec3 target){
 if(!grab_.active)return;
 if(!std::isfinite(target.x)||!std::isfinite(target.y)||!std::isfinite(target.z))
  throw std::invalid_argument("nonfinite cloth grab target");
 // Keep mouse interaction within the demo's finite physical workspace.
 grab_.target={std::clamp(target.x,-4.,4.),
               std::clamp(target.y,-4.,4.),
               std::clamp(target.z,.10,5.0)};
}
void MultiCloth::endGrab(){grab_.active=false;}
Vec3 MultiCloth::grabPoint() const{
 if(!grab_.active)return {};
 const auto tri=triangles_.at(static_cast<std::size_t>(grab_.triangle));
 Vec3 point{};
 for(int j=0;j<3;++j)point=add(point,mul(positions_[tri[j]],grab_.bary[j]));
 return point;
}
void MultiCloth::projectGrab(std::vector<Vec3>& nonphysical){
 if(!grab_.active)return;
 const auto tri=triangles_[static_cast<std::size_t>(grab_.triangle)];
 const Vec3 delta=sub(grab_.filteredTarget,grabPoint());
 const double length=norm(delta);
 metrics_.grabError=length;
 if(length<1e-8)return;
 double inverse=0;
 for(int j=0;j<3;++j)inverse+=
   grab_.bary[j]*grab_.bary[j]*inverseMasses_[tri[j]];
 if(inverse<1e-12)return;
 // An XPBD-style, mass-weighted barycentric attachment. The held point
 // follows smoothly instead of teleporting vertices and exciting extreme
 // elastic energy. Contact projections may push it away to avoid overlap.
 const Vec3 corr=mul(delta,std::min(.022/length,1.0)*.77/inverse);
 for(int j=0;j<3;++j){
  const int id=tri[j];
  const Vec3 move=mul(corr,grab_.bary[j]*inverseMasses_[id]);
  positions_[id]=add(positions_[id],move);
  nonphysical[id]=add(nonphysical[id],move);
 }
 ++metrics_.grabCorrections;
}
void MultiCloth::displacePatchForTest(int sheet,int minX,int minY,int width,Vec3 delta){
 if(sheet<0||sheet>=sheets_||minX<0||minY<0||width<1||
    minX+width>kSide||minY+width>kSide)
  throw std::invalid_argument("test patch out of cloth bounds");
 for(int y=minY;y<minY+width;++y)for(int x=minX;x<minX+width;++x){
  const int id=sheet*kSide*kSide+y*kSide+x;
  if(pinned_[id])throw std::invalid_argument("cannot displace pinned cloth vertex");
  positions_[id]=add(positions_[id],delta);
  previous_[id]=add(previous_[id],delta);
 }
 regional_.reset();global_.invalidate();
}
void MultiCloth::updateSpheres(double t){
 spheres_.clear();
 spheres_.push_back({{0.43*std::sin(.92*t),.15*std::cos(1.12*t),.97+.08*std::sin(.71*t)},.68});
 if(balls_==2)spheres_.push_back({
  {-.53+.35*std::cos(1.40*t),.36+.36*std::sin(.81*t),1.31+.11*std::cos(.65*t)},.43});
}
std::vector<Primitive> MultiCloth::makePrimitives()const{
 std::vector<Primitive> out;out.reserve(triangles_.size()+spheres_.size());
 const std::uint32_t clothMask=(1u<<sheets_)-1u,sphereCat=1u<<3;
 for(std::size_t i=0;i<triangles_.size();++i){
  const auto t=triangles_[i];
  Vec3 lo=positions_[t[0]],hi=lo;
  for(int j=1;j<3;++j){
   const Vec3 p=positions_[t[j]];
   lo={std::min(lo.x,p.x),std::min(lo.y,p.y),std::min(lo.z,p.z)};
   hi={std::max(hi.x,p.x),std::max(hi.y,p.y),std::max(hi.z,p.z)};
  }
  const auto cat=std::uint32_t{1}<<triangleSheets_[i];
  out.push_back({{int(i),lo,hi,cat,
                 (selfCollisions_?clothMask:(clothMask^cat))|sphereCat,true},
                 triangleRegions_[i],triangleSheets_[i]});
 }
 for(std::size_t i=0;i<spheres_.size();++i){
  const Vec3 p=spheres_[i].center;
  const double r=spheres_[i].radius;
  out.push_back({{int(triangles_.size()+i),sub(p,{r,r,r}),add(p,{r,r,r}),
                 sphereCat,clothMask,true},sheets_*100+int(i),-1});
 }
 return out;
}
void MultiCloth::projectContacts(const std::vector<Pair>& pairs,
                                 const std::vector<Bounds>& rawBounds){
 // Multiple triangle pairs share the SAME vertices. Without a per-vertex
 // projection budget, dozens of valid local corrections can push a single
 // particle several meters in ONE step, giving the next Verlet step huge
 // artificial velocity. Bound the total motion PER solver iteration.
 std::vector<double> correctionRemaining(positions_.size(),kContactBudgetPerIteration);
 const auto limitedCorrection=[&](int id,Vec3 delta){
  if(inverseMasses_[id]<=0)return false;
  const double requested=norm(delta);
  if(!std::isfinite(requested))throw std::runtime_error("nonfinite contact correction");
  if(requested<=1e-14||correctionRemaining[id]<=0)return false;
  const double actual=std::min(requested,correctionRemaining[id]);
  positions_[id]=add(positions_[id],mul(delta,actual/requested));
  correctionRemaining[id]-=actual;
  return true;
 };
 struct Weighted {std::array<int,3> ids{-1,-1,-1};std::array<double,3> w{};Vec3 p{};};
 const auto position=[&](const Weighted& x){Vec3 p{};for(int j=0;j<3;++j)
   if(x.ids[j]>=0)p=add(p,mul(positions_[x.ids[j]],x.w[j]));return p;};
 const auto respond=[&](const Weighted& a,const Weighted& b,double distance,Vec3 n,double threshold){
  if(distance>=threshold)return false;
  double denom=0;
  for(int j=0;j<3;++j){
   if(a.ids[j]>=0)denom+=a.w[j]*a.w[j]*inverseMasses_[a.ids[j]];
   if(b.ids[j]>=0)denom+=b.w[j]*b.w[j]*inverseMasses_[b.ids[j]];
  }
  if(denom<=1e-16)return false;
  const Vec3 corr=mul(n,std::min(0.13,threshold-distance)/denom*.68);
  bool moved=false;
  for(int j=0;j<3;++j){
   if(a.ids[j]>=0)
    moved=limitedCorrection(a.ids[j],
      mul(corr,a.w[j]*inverseMasses_[a.ids[j]]))||moved;
   if(b.ids[j]>=0)
    moved=limitedCorrection(b.ids[j],
      mul(corr,-b.w[j]*inverseMasses_[b.ids[j]]))||moved;
  }
  return moved;
 };
 const auto weightedTri=[&](int ti,std::array<double,3> w,Vec3 p){
  Weighted result;result.ids=triangles_[static_cast<std::size_t>(ti)];result.w=w;result.p=p;return result;
 };
 const auto weightedVertex=[&](int id){
  Weighted result;result.ids={id,-1,-1};result.w={1,0,0};result.p=positions_[id];return result;
 };
 for(const auto pair:pairs){
  // The global guarded SAP explicitly emits a SUPSET of pairs from old
  // fat AABBs. Such pairs must not inject actual contact impulses after
  // separating! Revalidate against the CURRENT padded primitive boxes
  // before any feature-level response. Keep this cost in narrowMs, not in
  // the guarded broad-phase lookup metric.
  if(policy_==Policy::GlobalGuard){
   const auto& a=rawBounds.at(static_cast<std::size_t>(pair.a));
   const auto& b=rawBounds.at(static_cast<std::size_t>(pair.b));
   ++metrics_.broadAabbTests;
   if(a.max.x+kMargin<b.min.x||b.max.x+kMargin<a.min.x||
      a.max.y+kMargin<b.min.y||b.max.y+kMargin<a.min.y||
      a.max.z+kMargin<b.min.z||b.max.z+kMargin<a.min.z)
    continue;
  }
  const int ia=pair.a,ib=pair.b;
  const int triangles=static_cast<int>(triangles_.size());
  if(ia>=triangles&&ib>=triangles)continue;
  const bool sameSheet=ia<triangles&&ib<triangles&&
                       triangleSheets_[ia]==triangleSheets_[ib];
  if(sameSheet){
   if(!selfCollisions_||!nonlocalSelfPair(ia,ib)){
    ++metrics_.selfTopologyFiltered;
    continue;
   }
   ++metrics_.selfCandidatePairs;
   ++metrics_.selfPreciseTests;
  }
  ++metrics_.preciseTests;
  if(ib>=triangles||ia>=triangles){
   const int ti=ia<triangles?ia:ib,si=(ia>=triangles?ia:ib)-triangles;
   if(si<0||si>=static_cast<int>(spheres_.size()))throw std::runtime_error("invalid sphere pair");
   const auto& ball=spheres_[si];const auto t=triangles_[ti];
   const auto q=trianglePoint(ball.center,positions_[t[0]],positions_[t[1]],positions_[t[2]]);
   const Vec3 delta=sub(q.p,ball.center);const double d=norm(delta);
   // All three mesh vertices are dynamic unless explicitly pinned; the
   // sphere is a kinematic obstacle and receives no positional correction.
   Weighted mesh=weightedTri(ti,q.w,q.p),fixed;
   Vec3 n=d>1e-10?mul(delta,1.0/d):normalized(cross(
                 sub(positions_[t[1]],positions_[t[0]]),
                 sub(positions_[t[2]],positions_[t[0]])));
   double den=0;for(int j=0;j<3;++j)
    den+=mesh.w[j]*mesh.w[j]*inverseMasses_[mesh.ids[j]];
   if(d<ball.radius+kThickness&&den>1e-15){
    const auto correction=mul(n,std::min(0.15,ball.radius+kThickness-d)/den*.75);
    bool moved=false;
    for(int j=0;j<3;++j)
     moved=limitedCorrection(mesh.ids[j],
       mul(correction,mesh.w[j]*inverseMasses_[mesh.ids[j]]))||moved;
    if(moved){
     ++metrics_.contacts;++metrics_.sphereContacts;
     if(policy_==Policy::OnlineContactAware)
      ++metrics_.regionWork[triangleRegions_[ti]].contactCorrections;
     metrics_.contactPairs.push_back(pair);
    }
   }
   continue;
  }
  const auto ta=triangles_[ia],tb=triangles_[ib];
  // Choose the nearest primitive feature pair: six vertex-face and nine
  // edge-edge tests. This is an approximate *discrete* contact solver,
  // not an intersection-free or CCD method.
  double best=std::numeric_limits<double>::infinity();
  Weighted bestA,bestB;
  auto consider=[&](Weighted a,Weighted b){
   const double d=len2(sub(a.p,b.p));
   if(d<best){best=d;bestA=a;bestB=b;}
  };
  for(int v=0;v<3;++v){
   const int p=ta[v],q=tb[v];
   auto b=trianglePoint(positions_[p],positions_[tb[0]],positions_[tb[1]],positions_[tb[2]]);
   consider(weightedVertex(p),weightedTri(ib,b.w,b.p));
   auto a=trianglePoint(positions_[q],positions_[ta[0]],positions_[ta[1]],positions_[ta[2]]);
   consider(weightedTri(ia,a.w,a.p),weightedVertex(q));
  }
  for(int u=0;u<3;++u)for(int v=0;v<3;++v){
   const int ua=ta[u],ub=ta[(u+1)%3],va=tb[v],vb=tb[(v+1)%3];
   const auto pairS=closestSegments(positions_[ua],positions_[ub],
                                    positions_[va],positions_[vb]);
   Weighted a,b;
   a.ids={ua,ub,-1};a.w={1-pairS.s,pairS.s,0};a.p=pairS.a;
   b.ids={va,vb,-1};b.w={1-pairS.t,pairS.t,0};b.p=pairS.b;
   consider(a,b);
  }
  // For a nonlocal fold, an edge can PIERCE an interior triangle without
  // leaving a vertex within contact thickness. Use current edge-face
  // intersections to detect that discrete geometric self-intersection.
  // This is an extra DISCRETE safeguard, not guaranteed temporal CCD.
  bool piercing=false;
  if(sameSheet){
   const auto testPiercing=[&](const std::array<int,3>& edgeTri,int faceId,
                               bool edgeIsA){
    if(piercing)return;
    const auto face=triangles_[static_cast<std::size_t>(faceId)];
    for(int edge=0;edge<3&&!piercing;++edge){
     const int v0=edgeTri[edge],v1=edgeTri[(edge+1)%3];
     const auto hit=segmentFace(positions_[v0],positions_[v1],
       positions_[face[0]],positions_[face[1]],positions_[face[2]]);
     if(!hit)continue;
     Weighted edgePoint;
     edgePoint.ids={v0,v1,-1};
     edgePoint.w={1.-hit->edgeParameter,hit->edgeParameter,0};
     edgePoint.p=hit->p;
     const Weighted facePoint=weightedTri(faceId,hit->bary,hit->p);
     if(edgeIsA){bestA=edgePoint;bestB=facePoint;}
     else{bestA=facePoint;bestB=edgePoint;}
     piercing=true;
    }
   };
   testPiercing(ta,ib,true);
   testPiercing(tb,ia,false);
   if(piercing)++metrics_.selfCrossings;
  }
  const double distance=piercing?0.:std::sqrt(best);
  const Vec3 faceNormal=normalized(cross(sub(positions_[ta[1]],positions_[ta[0]]),
                                         sub(positions_[ta[2]],positions_[ta[0]])));
  Vec3 normal{};
  if(!piercing&&distance>1e-9)
   normal=mul(sub(bestA.p,bestB.p),1.0/distance);
  else if(sameSheet){
   // At geometric intersection, the two features coincide. Orient the
   // separating normal using their PREVIOUS timestep separation when
   // available; this avoids random front/back sign flips during folding.
   const auto before=[&](const Weighted& w){
    Vec3 value{};
    for(int j=0;j<3;++j)if(w.ids[j]>=0)
     value=add(value,mul(previous_[w.ids[j]],w.w[j]));
    return value;
   };
   const Vec3 oldSide=sub(before(bestA),before(bestB));
   normal=norm(oldSide)>1e-8?normalized(oldSide):faceNormal;
   if(norm(oldSide)<=1e-8){
    const Vec3 ca=mul(add(add(positions_[ta[0]],positions_[ta[1]]),positions_[ta[2]]),1./3);
    const Vec3 cb=mul(add(add(positions_[tb[0]],positions_[tb[1]]),positions_[tb[2]]),1./3);
    if(dot(normal,sub(ca,cb))<0)normal=mul(normal,-1);
   }
  }else normal=mul(faceNormal,triangleSheets_[ia]<triangleSheets_[ib]?1:-1);
  if(respond(bestA,bestB,distance,normal,sameSheet?kSelfThickness:kThickness)){
   ++metrics_.contacts;
   if(sameSheet)++metrics_.selfContacts;
   else ++metrics_.interSheetContacts;
   if(policy_==Policy::OnlineContactAware){
    ++metrics_.regionWork[triangleRegions_[ia]].contactCorrections;
    ++metrics_.regionWork[triangleRegions_[ib]].contactCorrections;
   }
   metrics_.contactPairs.push_back(pair);
  }
 }
}
void MultiCloth::step(){
 const auto start=Clock::now();
 ++frame_;
 metrics_={};const double time=frame_*kDt;
 const auto frameStartPositions=positions_;
 updateSpheres(time);
 if(grab_.active){
  // Smooth the TARGET at frame rate, regardless of how far or quickly a
  // mouse event moves. The solver receives no discontinuous teleport.
  const Vec3 toTarget=sub(grab_.target,grab_.filteredTarget);
  const double distance=norm(toTarget);
  const double travel=std::min(.055,distance);
  if(distance>1e-12)
   grab_.filteredTarget=add(grab_.filteredTarget,mul(toTarget,travel/distance));
 }
 // Track only collision/floor projections. Geometric corrections should
 // remove penetration without becoming an unintended giant velocity.
 std::vector<Vec3> contactDisplacements(positions_.size(),Vec3{});
 for(std::size_t i=0;i<positions_.size();++i){
  if(pinned_[i])continue;
  Vec3 velocity=mul(sub(positions_[i],previous_[i]),kVelocityDamping);
  const double speed=norm(velocity);
  if(speed>kMaxFreeVelocityStep){
   velocity=mul(velocity,kMaxFreeVelocityStep/speed);
   ++metrics_.velocityClamps;
  }
  const int sheet=int(i/(kSide*kSide));
  // The former 0.003m *position kick per frame* was equivalent to roughly
  // 24m/s^2 lateral acceleration: >2x gravity, and badly destabilizing.
  const double windAcceleration=kWindAcceleration*
    std::sin(2.8*time+sheet*.55+positions_[i].y*.9);
  const double wind=windAcceleration*kDt*kDt;
  const Vec3 predicted=add(add(positions_[i],velocity),
                           {wind,wind*.20,-9.81*kDt*kDt});
  previous_[i]=positions_[i];
  positions_[i]=predicted;
 }
 for(auto& c:constraints_)c.lambda=0;
 live::QueryMetrics globalMetrics;
 for(int iter=0;iter<kIterations;++iter){
  // Nonstationary pinned corners keep the cloth geometry changing. The
  // constraints transmit that motion to free cloth particles.
  for(std::size_t i=0;i<pinned_.size();++i)if(pinned_[i]){
   const int layer=int(i/(kSide*kSide));
   const double phase=time*1.4+.7*layer;
   const double initialPhase=.7*layer;
   const auto p=initialPins_[i];
   // Subtract the t=0 oscillation offset. Previously sheet 2's corners
   // teleported ~0.24m on the first solver step simply because each sheet
   // had a different initial phase. Drive the anchors continuously instead.
   positions_[i]={
    p.x+.22*(std::sin(phase)-std::sin(initialPhase)),
    p.y+.12*(std::cos(phase*.75)-std::cos(initialPhase*.75)),
    p.z+.10*(std::sin(phase*.8)-std::sin(initialPhase*.8))
   };
   previous_[i]=positions_[i];
  }
  constexpr double invDt2=1.0/(kDt*kDt);
  for(auto& c:constraints_){
   const Vec3 delta=sub(positions_[c.a],positions_[c.b]);
   const double length=norm(delta);
   if(length<1e-12)continue;
   const double wa=inverseMasses_[c.a],wb=inverseMasses_[c.b];
   const double alpha=c.compliance*invDt2;
   const double dl=(-(length-c.length)-alpha*c.lambda)/(wa+wb+alpha);
   c.lambda+=dl;
   const auto corr=mul(delta,dl/length);
   positions_[c.a]=add(positions_[c.a],mul(corr,wa));
   positions_[c.b]=sub(positions_[c.b],mul(corr,wb));
  }
  for(std::size_t i=0;i<positions_.size();++i)
   if(inverseMasses_[i]>0&&positions_[i].z<.04){
    contactDisplacements[i].z+=.04-positions_[i].z;
    positions_[i].z=.04;
   }
  // The interactive grab is a mass-weighted barycentric positional
  // constraint. It participates in the SAME five solver iterations,
  // before collision queries so self-contact can resist a pulled fold.
  projectGrab(contactDisplacements);
  const auto primitives=makePrimitives();
  std::vector<Bounds> bounds;bounds.reserve(primitives.size());
  for(const auto& primitive:primitives)bounds.push_back(primitive.box);
  const auto queryStart=Clock::now();
  std::vector<Pair> pairs;
  if(policy_==Policy::RegionalCertified||policy_==Policy::OnlineContactAware){
   pairs=regional_.query(primitives,kMargin,adaptive_,metrics_,hierarchicalRegions_,
     policy_==Policy::OnlineContactAware);
  }else if(policy_==Policy::GlobalGuard){
   const auto& actual=global_.query(bounds,{kMargin,.045,live::UpdatePolicy::GuardedReuse},
                                     true,globalMetrics);
   pairs=actual;
  }else{
   // Pad only the query copy. The independent brute-force oracle below
   // starts from RAW geometry bounds and must not pad a second time.
   auto paddedBounds=bounds;
   for(auto& box:paddedBounds){
    box.min=sub(box.min,{kMargin*.5,kMargin*.5,kMargin*.5});
    box.max=add(box.max,{kMargin*.5,kMargin*.5,kMargin*.5});
   }
   pairs=live::candidates(paddedBounds,true,&metrics_.broadAabbTests);
  }
  if(policy_!=Policy::RegionalCertified&&policy_!=Policy::OnlineContactAware){
   metrics_.broadMs+=std::chrono::duration<double,std::milli>(Clock::now()-queryStart).count();
   ++metrics_.queryCount;metrics_.pairs=pairs.size();
  }
  metrics_.candidatePairs=pairs;
  if(auditing_&&(iter==0||iter==kIterations-1)&&(frame_%15==0||frame_==1)){
   std::vector<Bounds> exact;
   exact.reserve(bounds.size());
   for(auto b:bounds)exact.push_back(expand(b,kMargin*.5));
   const auto oracle=live::candidates(exact,false);
   if(!std::includes(pairs.begin(),pairs.end(),oracle.begin(),oracle.end())) {
    std::ostringstream error;error<<"MISS frame="<<frame_<<" iter="<<iter
      <<" policy="<<int(policy_)<<" oracle="<<oracle.size()
      <<" actual="<<pairs.size();
    for(const auto pair:oracle) {
     if(!std::binary_search(pairs.begin(),pairs.end(),pair)) {
      error<<" missing=("<<pair.a<<","<<pair.b<<")";
      break;
     }
    }
    throw std::runtime_error(error.str());
   }
   if(policy_==Policy::RegionalCertified&&pairs!=oracle)
    throw std::runtime_error("REGIONAL EXACT-CURRENT AABB REVALIDATION DISAGREES");
   metrics_.oracleChecked=true;
  }
  const auto narrowStart=Clock::now();
  const auto beforeContact=positions_;
  projectContacts(pairs,bounds);
  for(std::size_t i=0;i<positions_.size();++i){
   if(inverseMasses_[i]<=0)continue;
   // Floor is a unilateral constraint: contact may have pushed the cloth
   // downward after the pre-query floor projection.
   if(positions_[i].z<.04)positions_[i].z=.04;
   contactDisplacements[i]=add(contactDisplacements[i],
                               sub(positions_[i],beforeContact[i]));
  }
  metrics_.narrowMs+=std::chrono::duration<double,std::milli>(Clock::now()-narrowStart).count();
 }
 // PBD collision projections remove penetration but should not create
 // artificial high-frequency velocity. In Verlet coordinates, the velocity
 // for the next frame is (current - previous); shift previous by most of
 // the collision displacement to make collisions approximately inelastic.
 // This is deliberately separate from the structural XPBD constraints.
 for(std::size_t i=0;i<positions_.size();++i)
  if(inverseMasses_[i]>0){
   previous_[i]=add(previous_[i],
                    mul(contactDisplacements[i],kContactDisplacementDamping));
   const double speed=norm(sub(positions_[i],previous_[i]));
   if(!std::isfinite(speed))throw std::runtime_error("nonfinite cloth velocity");
   if(speed>kMaxFreeVelocityStep){
    previous_[i]=sub(positions_[i],
      mul(sub(positions_[i],previous_[i]),kMaxFreeVelocityStep/speed));
    ++metrics_.velocityClamps;
   }
  }
 if(policy_==Policy::GlobalGuard){
  // Preserve current-AABB revalidation work already counted in the
  // downstream guarded candidate filter; do not overwrite it with the
  // separate cached-SAP rebuild test count.
  metrics_.broadAabbTests+=globalMetrics.pairTests;
  metrics_.lastEvent="GLOBAL_GUARD_REUSE";
 }
 if(policy_==Policy::RebuildSAP)metrics_.lastEvent="FULL_SAP_REBUILD";
 if(policy_==Policy::OnlineContactAware){
  const auto update=online_.observe(frame_,metrics_.regionWork,metrics_.regionGuards,
                                    metrics_.broadMs,metrics_.queryCount);
  metrics_.onlinePatches=update.patches;
  metrics_.onlineSplits=update.splits;
  metrics_.onlineMerges=update.merges;
  metrics_.onlineStrategySwitches=update.strategySwitches;
  metrics_.onlineDecisionMs=update.decisionMs;
  metrics_.onlineMeasuredWork=update.observedWork;
  metrics_.onlineContactHeat=update.observedContacts;
  metrics_.onlineCoarsePairFraction=update.coarsePairFraction;
  const bool hierarchyChanged=hierarchicalRegions_!=update.hierarchy;
  hierarchicalRegions_=update.hierarchy;
  if(update.splits||update.merges){
   assignOnlineRegionIds();
   metrics_.lastEvent=update.splits?"ONLINE_PATCH_SPLIT":"ONLINE_PATCH_MERGE";
  }else if(hierarchyChanged){
   regional_.reset();
   metrics_.lastEvent="ONLINE_FRONTIER_CHANGE";
  }else metrics_.lastEvent="ONLINE_OBSERVING";
 }
 std::sort(metrics_.contactPairs.begin(),metrics_.contactPairs.end());
 metrics_.contactPairs.erase(std::unique(metrics_.contactPairs.begin(),metrics_.contactPairs.end()),
                             metrics_.contactPairs.end());
 if(grab_.active)metrics_.grabError=norm(sub(grab_.target,grabPoint()));
 // A solver can be exactly equivalent across broad phases while exploding.
 // Measure its actual motion and grid edge strain each frame.
 for(std::size_t i=0;i<positions_.size();++i){
  const auto p=positions_[i];
  if(!std::isfinite(p.x)||!std::isfinite(p.y)||!std::isfinite(p.z))
   throw std::runtime_error("nonfinite cloth particle");
  metrics_.maxFrameParticleTravel=std::max(metrics_.maxFrameParticleTravel,
                              norm(sub(p,frameStartPositions[i])));
  metrics_.maxWorldCoordinate=std::max(metrics_.maxWorldCoordinate,
                             std::max({std::abs(p.x),std::abs(p.y),std::abs(p.z)}));
 }
 for(int sheet=0;sheet<sheets_;++sheet)for(int y=0;y<kSide;++y)for(int x=0;x<kSide;++x){
  const std::size_t a=sheet*kSide*kSide+y*kSide+x;
  for(const std::size_t b:{x+1<kSide?a+1:a,y+1<kSide?a+kSide:a}){
   if(a==b)continue;
   const double rest=norm(sub(initialPins_[a],initialPins_[b]));
   if(rest>1e-12)
    metrics_.maxStructuralEdgeStretch=std::max(metrics_.maxStructuralEdgeStretch,
                         norm(sub(positions_[a],positions_[b]))/rest);
  }
 }
 metrics_.stepMs=std::chrono::duration<double,std::milli>(Clock::now()-start).count();
}
} // namespace ccp::research
