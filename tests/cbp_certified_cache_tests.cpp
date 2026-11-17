// CBP Phase-2 certified-cross-cache adversarial proof-of-execution regression.
// The oracle enumerates every primitive pair independent of the implementation.
#include "ccp/cbp_region_router.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <random>
#include <stdexcept>
#include <vector>

namespace {
using ccp::cbp::Bounds;
using ccp::cbp::Pairs;
using ccp::cbp::RegionRouter;
using ccp::cbp::CrossMode;

void require(bool good,const char* message){
 if(!good)throw std::runtime_error(message);
}
Pairs oracle(const std::vector<Bounds>& boxes){
 Pairs out;
 for(std::size_t i=0;i<boxes.size();++i)
  for(std::size_t j=i+1;j<boxes.size();++j){
   const auto& a=boxes[i];const auto& b=boxes[j];
   if((a.mask&b.category)==0||(b.mask&a.category)==0)continue;
   if(a.min.x<=b.max.x&&b.min.x<=a.max.x&&
      a.min.y<=b.max.y&&b.min.y<=a.max.y&&
      a.min.z<=b.max.z&&b.min.z<=a.max.z)
    out.push_back({static_cast<int>(i),static_cast<int>(j)});
  }
 return out;
}
void verify(RegionRouter& cache,RegionRouter& direct,
            const std::vector<Bounds>& boxes,const std::vector<int>& labels){
 const auto expected=oracle(boxes);
 const auto got=cache.query(boxes,labels);
 const auto baseline=direct.query(boxes,labels);
 require(got==expected,"Certified graph missed or fabricated current overlaps");
 require(baseline==expected,"Exhaustive control disagreed with independent oracle");
}
}

int main(){
 RegionRouter cached,brute;
 cached.setCrossMode(CrossMode::CertifiedCache);
 brute.setCrossMode(CrossMode::Exhaustive);
 cached.setCachePadding(.15);
 verify(cached,brute,{},{});
 std::vector<Bounds> boxes{
  {0,{0,0,0},{1,1,1},1,3,true},
  {1,{.5,0,0},{1.5,1,1},1,3,true},
  {2,{3,0,0},{4,1,1},1,3,true},
  {3,{3,1,0},{4,2,1},2,3,true}
 };
 std::vector<int> labels{0,1,1,2};
 verify(cached,brute,boxes,labels);
 require(cached.telemetry().cacheGraphBuilds==1,"Initial potential graph not constructed");
 verify(cached,brute,boxes,labels);
 require(cached.telemetry().cacheQuietReuses==1,"Quiet graph not reused");
 // A new pair must be discovered after one object escapes its envelope.
 boxes[0].min.x=3.;boxes[0].max.x=4.;
 verify(cached,brute,boxes,labels);
 require(cached.telemetry().cacheDirtyPrimitives==1,"Escaping bound was not marked dirty");
 require(cached.telemetry().cacheEdgesInserted>0,"Indexed repair failed to add new contact edges");
 require(cached.telemetry().cacheEdgesRemoved>0,"Dirty incident edges were not removed");
 require(cached.telemetry().indexNodePairVisits>0,"Repair did not query the shared BVH");

 // Metadata-only change invalidates prior candidates with NO geometry change.
 boxes[0].mask=0;
 verify(cached,brute,boxes,labels);
 require(cached.telemetry().cacheDirtyPrimitives==1,"Mask change not invalidated");
 boxes[0].mask=3;
 verify(cached,brute,boxes,labels);

 // Motion within the padded envelope must not cause a graph rebuild.
 boxes[0].min.x+=.01;boxes[0].max.x+=.01;
 verify(cached,brute,boxes,labels);
 require(cached.telemetry().cacheQuietReuses==1,"Contained motion rebuilt certificate graph");

 // Changing labels must synchronously rebuild the cross-ownership graph.
 labels[2]=0;
 verify(cached,brute,boxes,labels);
 require(cached.telemetry().cacheGraphBuilds==1,"Ownership migration kept stale graph");
 labels[3]=-11;
 verify(cached,brute,boxes,labels);
 require(cached.telemetry().cacheGraphBuilds==1,"Negative-label migration kept stale graph");

 // Inclusive touching and zero extent remain valid.
 boxes[1].min.x=boxes[0].max.x;
 boxes[1].max.x=boxes[1].min.x;
 verify(cached,brute,boxes,labels);

 cached.reset();brute.reset();
 boxes.clear();labels.clear();
 constexpr int N=256;
 for(int i=0;i<N;++i){
  const double x=(i%16)*.42,y=(i/16)*.42;
  boxes.push_back({i,{x,y,0.},{x+.55,y+.55,.5},
                   i%7==0?2u:1u,i%11==0?0u:3u,true});
  labels.push_back(i%9);
 }
 std::mt19937_64 rng(20261119);
 std::uniform_real_distribution<double> jump(-1.2,1.2);
 cached.setSapThreshold(24);
 brute.setSapThreshold(24);
 std::size_t quiet=0,dirty=0,added=0,removed=0,builds=0;
 for(int frame=0;frame<160;++frame){
  for(int i=0;i<N;++i){
   auto& a=boxes[static_cast<std::size_t>(i)];
   const double dx=(frame%23==0 && i%23==0)?jump(rng):.001*double((i%5)-2);
   a.min.x+=dx;a.max.x+=dx;
  }
  if(frame==40)for(int i=0;i<N;++i)labels[static_cast<std::size_t>(i)]=i%11;
  if(frame==80)std::fill(labels.begin(),labels.end(),-2);
  if(frame==112)for(int i=0;i<N;++i)labels[static_cast<std::size_t>(i)]=i%8;
  if(frame%17==0){
   auto& b=boxes[static_cast<std::size_t>(frame%N)];
   b.mask^=1u;
  }
  verify(cached,brute,boxes,labels);
  const auto t=cached.telemetry();
  quiet+=t.cacheQuietReuses;
  dirty+=t.cacheDirtyPrimitives;
  added+=t.cacheEdgesInserted;
  removed+=t.cacheEdgesRemoved;
  builds+=t.cacheGraphBuilds;
 }
 require(quiet>0,"Quiet reuse was never exercised");
 require(dirty>0,"Dynamic invalidation was never exercised");
 require(added>0 && removed>0,"Indexed adjacency repair was never exercised");
 require(builds>=3,"Repartition rebuilds were not exercised");

 // Invalid input must fail instead of silently retaining stale certificates.
 bool rejected=false;
 try{auto invalid=boxes;invalid[0].min.x=std::nan("");
     (void)cached.query(invalid,labels);}
 catch(const std::invalid_argument&){rejected=true;}
 require(rejected,"NaN coordinates accepted");
 rejected=false;
 try{auto invalid=boxes;invalid[0].id=4;
     (void)cached.query(invalid,labels);}
 catch(const std::invalid_argument&){rejected=true;}
 require(rejected,"Unstable indexed IDs accepted");
 std::cout<<"PASS CBP Phase 2 certified cross-region graph: 160 dynamic frames, "
          <<"quiet="<<quiet<<" dirty="<<dirty<<" added="<<added
          <<" removed="<<removed<<" rebuilds="<<builds<<'\n';
}
