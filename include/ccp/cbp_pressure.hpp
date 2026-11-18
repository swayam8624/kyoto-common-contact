#pragma once
// Kyoto Phase 11: adversarial, deterministic 3D current-AABB benchmark.
//
// No spatial-island guarantee is used by either general algorithm. This
// COMMON geometry/ownership source drives CPU and Metal count-only kernels.
// Exactness: grid assigns each overlapping pair to the UNIQUE cell containing
// its coordinate-wise overlap minimum, preventing duplicate emission across
// multiple occupied cells. A separate X-sweep and brute oracle are references.
//
// Scope: synthetic moving AABBs, NOT real material integration/CCD.
// digest counts+two commutative 64-bit hashes, NOT materialized output pairs.
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace ccp::pressure {
struct alignas(16) Box { float lo[4]{},hi[4]{}; };
static_assert(sizeof(Box)==32);
struct Digest {
 std::uint64_t count{},sum{},xors{};
 // The native Metal pressure kernels also compute these independent
 // 32-bit modular fingerprints, avoiding unsupported 64-bit atomics.
 std::uint32_t sum32{},xor32{};
 bool operator==(const Digest&) const = default;
};
inline std::uint32_t mix32(std::uint32_t a,std::uint32_t b){
 std::uint32_t x=a*UINT32_C(0x9e3779b9)+b*UINT32_C(0x85ebca6b)+
                 UINT32_C(0x165667b1);
 x^=x>>16;x*=UINT32_C(0x7feb352d);
 x^=x>>15;x*=UINT32_C(0x846ca68b);
 return x^(x>>16);
}
inline std::uint64_t mix(std::uint64_t x){
 x+=UINT64_C(0x9e3779b97f4a7c15);
 x=(x^(x>>30))*UINT64_C(0xbf58476d1ce4e5b9);
 x=(x^(x>>27))*UINT64_C(0x94d049bb133111eb);
 return x^(x>>31);
}
inline void add(Digest& d,std::uint32_t a,std::uint32_t b){
 if(a>b)std::swap(a,b);
 const auto h=mix((std::uint64_t(a)<<32)|b);
 const auto h32=mix32(a,b);
 ++d.count;d.sum+=h;d.xors^=h;
 d.sum32+=h32;d.xor32^=h32;
}
inline void combine(Digest& a,const Digest& b){
 a.count+=b.count;a.sum+=b.sum;a.xors^=b.xors;
 a.sum32+=b.sum32;a.xor32^=b.xor32;
}
inline bool overlap(const Box& a,const Box& b){
 return a.lo[0]<=b.hi[0]&&b.lo[0]<=a.hi[0]&&
        a.lo[1]<=b.hi[1]&&b.lo[1]<=a.hi[1]&&
        a.lo[2]<=b.hi[2]&&b.lo[2]<=a.hi[2];
}
inline std::uint64_t number(const char* str,std::uint64_t low,std::uint64_t high){
 std::string s(str);std::size_t end=0;
 auto value=std::stoull(s,&end);
 if(end!=s.size()||value<low||value>high)
  throw std::invalid_argument("Out-of-range bounded integer: "+s);
 return value;
}
inline const std::array<const char*,6> scenes={
 "sparse","burst","slabs","teleport","bridge","all_overlap"
};
inline bool known(const std::string& scene){
 return std::find_if(scenes.begin(),scenes.end(),
  [&](const char* s){return scene==s;})!=scenes.end();
}
inline std::vector<Box> generate(std::size_t n,std::uint32_t frame,
                                 std::uint32_t totalFrames,
                                 std::uint32_t seed,const std::string& scene){
 if(!known(scene))throw std::invalid_argument("Unknown stress scene "+scene);
 if(n>2000000)throw std::invalid_argument("2M AABBs maximum in pressure generator");
 std::vector<Box> out(n);
 const auto groups=(n+63)/64;
 const auto side=std::size_t(std::ceil(std::cbrt(double(groups))));
 const double phase=double(frame)/double(std::max(1u,totalFrames-1u));
 for(std::size_t i=0;i<n;++i){
  const auto h=mix(i+std::uint64_t(seed)*UINT64_C(0x11791d291c13));
  const auto h2=mix(h);
  const auto group=i/64,member=i%64;
  double x=0,y=0,z=0,half=.12;
  if(scene=="slabs"){
   // Pathological 1D projection: all boxes overlap in X, almost none in
   // YZ. SAP can visit O(N^2) false candidates, while 3D hashing remains
   // efficient. No procedural island partition is used by the algorithm.
   const std::size_t edge=std::size_t(std::ceil(std::sqrt(double(n))));
   x=.006*double(int(h%13)-6);
   y=.65*double(i%edge);z=.65*double(i/edge);
   half=.16;
  }else if(scene=="all_overlap"){
   // True quadratic output cardinality. Never allocate all pairs.
   x=y=z=0;half=.35;
  }else if(scene=="sparse"){
   const auto j=mix(h+frame/9);
   const float scale=float(std::cbrt(double(n))*1.80);
   x=scale*double(j&0x1fffff)/double(0x1fffff);
   y=scale*double((j>>21)&0x1fffff)/double(0x1fffff);
   z=scale*double((h2>>11)&0x1fffff)/double(0x1fffff);
   half=.075;
  }else{
   // Each 64-body cluster is a continuously moving, independently indexed
   // MATERIAL SURROGATE, not a deformable mass-spring world.
   // Burst destroys the previous sparse-contact geometry mid-trajectory.
   // Teleport breaks temporal locality (but preserves stable primitive IDs).
   // Bridge makes true *cross-cluster* AABB candidates, which the island
   // benchmark could never produce.
   double step=.39;
   if(scene=="burst"){
    step=(phase>.25&&phase<.75)? .04:.39;
   }else if(scene=="teleport"){
    step=.14;
   }else if(scene=="bridge"){
    step=.115;
   }
   auto label=group;
   if(scene=="teleport"&&phase>.50)
    label=std::size_t(mix(group+seed)%std::uint64_t(groups));
   const double px=double(label%side),py=double((label/side)%side),
                pz=double(label/(side*side));
   const double global=scene=="bridge"? .56:3.5;
   const double shift=.045*std::sin(double(frame)*.077+double((h2>>20)%19));
   x=global*px+step*double(member%4)+shift;
   y=global*py+step*double((member/4)%4)-shift;
   z=global*pz+step*double((member/16)%4)+.035*std::cos(frame*.042+member);
   half=scene=="bridge"?.14:.155;
  }
  for(int k=0;k<3;++k){
   const double v=k==0?x:k==1?y:z;
   out[i].lo[k]=float(v-half);
   out[i].hi[k]=float(v+half);
   if(!std::isfinite(out[i].lo[k])||!std::isfinite(out[i].hi[k])||
       out[i].lo[k]>out[i].hi[k])
    throw std::runtime_error("Synthetic AABB geometry overflow or inversion");
  }
 }
 return out;
}
struct Cell {
 int x{},y{},z{};
 bool operator==(const Cell&)const=default;
};
struct CellHash {
 std::size_t operator()(Cell c)const noexcept{
  return std::size_t(mix(std::uint64_t(std::uint32_t(c.x))+
     UINT64_C(0x9e3779b97f4a7c15)*std::uint32_t(c.y)+
     UINT64_C(0xbf58476d1ce4e5b9)*std::uint32_t(c.z)));
 }
};
inline constexpr float kCellSize=1.f;
inline int cellCoord(float v){
 if(!std::isfinite(v)||std::abs(double(v))>1.e7)
  throw std::runtime_error("Unbounded coordinate rejected");
 return int(std::floor(double(v)/kCellSize));
}
inline Cell canonical(const Box& a,const Box& b){
 return {cellCoord(std::max(a.lo[0],b.lo[0])),
         cellCoord(std::max(a.lo[1],b.lo[1])),
         cellCoord(std::max(a.lo[2],b.lo[2]))};
}
struct Bin {Cell cell;std::vector<std::uint32_t> ids;};
struct Grid {
 std::vector<Bin> bins;
 std::uint64_t entries{},workUpperBound{};
 std::size_t estimatedBytes{};
};
inline Grid buildGrid(const std::vector<Box>& boxes,
                      std::size_t maxEntries=12000000){
 // Build independent exact *spatial* cell owners from the current AABBs.
 // AABB occupying multiple cells appears multiple times in the bin lists;
 // canonical-overlap-cell emission is unique, by construction.
 Grid g;
 std::unordered_map<Cell,std::size_t,CellHash> lookup;
 lookup.reserve(std::min(boxes.size()*2,std::size_t(2000000)));
 for(std::size_t i=0;i<boxes.size();++i){
  const auto& b=boxes[i];
  const std::array<int,3> lo={
   cellCoord(b.lo[0]),cellCoord(b.lo[1]),cellCoord(b.lo[2])};
  const std::array<int,3> hi={
   cellCoord(b.hi[0]),cellCoord(b.hi[1]),cellCoord(b.hi[2])};
  const std::uint64_t cells=std::uint64_t(hi[0]-lo[0]+1)*
    std::uint64_t(hi[1]-lo[1]+1)*std::uint64_t(hi[2]-lo[2]+1);
  if(cells>256||cells>maxEntries-g.entries)
   throw std::runtime_error("Spatial cell/entry budget exceeded; no incomplete result");
  for(int x=lo[0];x<=hi[0];++x)
   for(int y=lo[1];y<=hi[1];++y)
    for(int z=lo[2];z<=hi[2];++z){
     const Cell cell{x,y,z};
     auto [it,inserted]=lookup.try_emplace(cell,g.bins.size());
     if(inserted)g.bins.push_back({cell,{}});
     g.bins[it->second].ids.push_back(std::uint32_t(i));
     ++g.entries;
    }
 }
 for(const auto& b:g.bins){
  const std::uint64_t k=b.ids.size();
  g.workUpperBound+=k*(k-1)/2;
 }
 // Estimate only; std::unordered_map dynamic/allocator overhead is NOT
 // captured by sizeof. The driver also queries process RSS where available.
 g.estimatedBytes=boxes.size()*sizeof(Box)+std::size_t(g.entries)*4+
   g.bins.size()*(sizeof(Bin)+sizeof(Cell)+48);
 return g;
}
struct Result {
 Digest digest{};
 std::uint64_t tests{};
 bool complete{true};
 double ms{};
};
inline double since(std::chrono::steady_clock::time_point began){
 return std::chrono::duration<double,std::milli>(
   std::chrono::steady_clock::now()-began).count();
}
inline Result queryGrid(const std::vector<Box>& boxes,const Grid& grid,
                        std::uint64_t testBudget,double wallLimitMs){
 Result out;
 const auto start=std::chrono::steady_clock::now();
 for(const auto& cell:grid.bins){
  for(std::size_t i=0;i<cell.ids.size();++i)
   for(std::size_t j=i+1;j<cell.ids.size();++j){
    if(out.tests>=testBudget||(out.tests%65536==0 &&
       since(start)>wallLimitMs)){out.complete=false;out.ms=since(start);return out;}
    ++out.tests;
    const auto a=cell.ids[i],b=cell.ids[j];
    if(overlap(boxes[a],boxes[b])&&canonical(boxes[a],boxes[b])==cell.cell)
     add(out.digest,a,b);
   }
 }
 out.ms=since(start);return out;
}
inline Result querySweep(const std::vector<Box>& boxes,
                         std::uint64_t testBudget,double wallLimitMs){
 Result out;
 const auto start=std::chrono::steady_clock::now();
 std::vector<std::uint32_t> ids(boxes.size());
 for(std::size_t i=0;i<ids.size();++i)ids[i]=std::uint32_t(i);
 std::sort(ids.begin(),ids.end(),[&](auto a,auto b){
  if(boxes[a].lo[0]!=boxes[b].lo[0])
   return boxes[a].lo[0]<boxes[b].lo[0];
  return a<b;
 });
 for(std::size_t i=0;i<ids.size();++i){
  const auto a=ids[i];
  for(std::size_t j=i+1;j<ids.size();++j){
   const auto b=ids[j];
   if(boxes[b].lo[0]>boxes[a].hi[0])break;
   if(out.tests>=testBudget||(out.tests%65536==0 &&
      since(start)>wallLimitMs)){out.complete=false;out.ms=since(start);return out;}
   ++out.tests;
   if(overlap(boxes[a],boxes[b]))add(out.digest,a,b);
  }
 }
 out.ms=since(start);return out;
}
inline Digest brute(const std::vector<Box>& boxes){
 Digest d;
 for(std::size_t i=0;i<boxes.size();++i)
  for(std::size_t j=i+1;j<boxes.size();++j)
   if(overlap(boxes[i],boxes[j]))add(d,std::uint32_t(i),std::uint32_t(j));
 return d;
}
} // namespace ccp::pressure
