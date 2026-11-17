// CBP Phase 1: exact indexed-vs-exhaustive cross-region regression.
// This test is independent of any V4/V5/V6 runtime benchmark result.
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
using ccp::cbp::Pair;
using ccp::cbp::Pairs;
using ccp::cbp::RegionRouter;
using ccp::cbp::CrossMode;
using ccp::cbp::Kernel;

void require(bool condition,const char* reason){
 if(!condition)throw std::runtime_error(reason);
}
Pairs oracle(const std::vector<Bounds>& boxes){
 Pairs out;
 for(std::size_t i=0;i<boxes.size();++i)
  for(std::size_t j=i+1;j<boxes.size();++j){
   const Bounds& a=boxes[i];const Bounds& b=boxes[j];
   if((a.mask&b.category)==0||(b.mask&a.category)==0)continue;
   if(a.min.x<=b.max.x&&b.min.x<=a.max.x&&
      a.min.y<=b.max.y&&b.min.y<=a.max.y&&
      a.min.z<=b.max.z&&b.min.z<=a.max.z)
    out.push_back({a.id,b.id});
  }
 return out;
}
void compare(RegionRouter& fast,RegionRouter& slow,
             const std::vector<Bounds>& boxes,const std::vector<int>& labels){
 const auto reference=oracle(boxes);
 const auto actual=fast.query(boxes,labels);
 const auto phase0=slow.query(boxes,labels);
 require(actual==reference,"Indexed CBP has missing, duplicate or fabricated pairs");
 require(phase0==reference,"Phase-0 exhaustive cross join differs from oracle");
}
}

int main(){
 RegionRouter indexed,reference;
 indexed.setCrossMode(CrossMode::SharedBVH);
 reference.setCrossMode(CrossMode::Exhaustive);
 indexed.setSapThreshold(24);
 reference.setSapThreshold(24);
 compare(indexed,reference,{},{});
 std::vector<Bounds> touching{
  {0,{0,0,0},{1,1,1},1,3,true},
  {1,{1,0,0},{2,1,1},2,3,true},
  {2,{2,0,0},{3,1,1},1,3,true},
  {3,{1.5,1.5,0},{2.5,2.5,1},1,3,true}
 };
 std::vector<int> smallLabels{-9,4,-9,4};
 compare(indexed,reference,touching,smallLabels);
 require(indexed.telemetry().indexBuilds==1,"New labels failed to build index");
 compare(indexed,reference,touching,smallLabels);
 require(indexed.telemetry().indexBuilds==0,"Unchanged scene rebuilt BVH");
 require(indexed.telemetry().indexChangedLeaves==0,"Unchanged bounds were refitted");

 // Metadata-only changes must affect filtering even without geometry refits.
 touching[1].mask=0;
 compare(indexed,reference,touching,smallLabels);
 require(indexed.telemetry().indexChangedLeaves==0,"Mask-only change unnecessarily refit geometry");
 touching[1].mask=3;
 touching[2].min.x=.75;touching[2].max.x=1.75;
 compare(indexed,reference,touching,smallLabels);
 require(indexed.telemetry().indexChangedLeaves==1,"Dirty leaf not registered");
 require(indexed.telemetry().indexAncestorRefits>0,"Dirty ancestors not refitted");
 smallLabels[2]=4;
 compare(indexed,reference,touching,smallLabels);
 require(indexed.telemetry().indexBuilds==1,"Ownership migration failed to rebuild index");

 std::mt19937_64 rng(20261117);
 std::uniform_real_distribution<double> position(-8.,8.);
 std::uniform_real_distribution<double> jump(-3.,3.);
 std::vector<Bounds> boxes;
 std::vector<int> labels;
 constexpr int N=256;
 for(int i=0;i<N;++i){
  const double x=position(rng),y=position(rng),z=position(rng);
  boxes.push_back({i,{x,y,z},{x+.9,y+.7,z+.5},
                   i%7==0?2u:1u,i%11==0?0u:3u,true});
  labels.push_back((i%9)-4);
 }
 std::size_t refits=0,partialRebuilds=0;
 for(int frame=0;frame<140;++frame){
  // Repeated small steps, sudden teleportation and degenerate boxes.
  for(int i=0;i<N;++i){
   Bounds& b=boxes[static_cast<std::size_t>(i)];
   const double dx=frame%19==0?jump(rng):.005*double((i%7)-3);
   b.min.x+=dx;b.max.x+=dx;
   if(frame%17==0){b.min.z+=.08;b.max.z+=.08;}
  }
  if(frame==24)boxes[7].max.z=boxes[7].min.z;
  if(frame==51)for(int i=0;i<N;++i)labels[i]=i%13;
  if(frame==85)std::fill(labels.begin(),labels.end(),-12);
  if(frame==106)for(int i=0;i<N;++i)labels[i]=(i%8)-4;
  if(frame%9==0)boxes[static_cast<std::size_t>(frame%N)].mask^=1u;
  compare(indexed,reference,boxes,labels);
  refits+=indexed.telemetry().indexChangedLeaves;
  partialRebuilds+=indexed.telemetry().indexBuilds;
 }
 require(refits>0,"No dynamic refits were exercised");
 require(partialRebuilds>=3,"No repartition rebuilds were exercised");

 // All region-union AABBs overlap, but individual boxes do not: this
 // adversarial case makes the old O(|Ra|*|Rb|) join do real wasted work.
 // The indexed method should eliminate those primitive-pair tests.
 indexed.reset();reference.reset();
 boxes.clear();labels.clear();
 for(int i=0;i<N;++i){
  const double x=4.*i;
  boxes.push_back({i,{x,0,0},{x+.4,.4,.4},1,3,true});
  labels.push_back(i%8);
 }
 compare(indexed,reference,boxes,labels);
 const auto indexedTests=indexed.telemetry().crossTests;
 const auto exhaustiveTests=reference.telemetry().crossTests;
 require(exhaustiveTests>1000,"Exhaustive control failed to exercise cross-patch work");
 require(indexedTests<exhaustiveTests/10,"Indexed cross join failed to prune separated primitives");

 // A stale leaf/ancestor would miss this newly created cross-region overlap.
 boxes[253].min.x=boxes[0].min.x;
 boxes[253].max.x=boxes[0].max.x;
 compare(indexed,reference,boxes,labels);
 require(indexed.telemetry().indexChangedLeaves==1,"Teleport did not refit the moved leaf");
 std::cout<<"PASS CBP shared BVH: 140 dynamic frames + transitions; "
          <<"spatial cross-tests "<<indexedTests<<" versus brute "<<exhaustiveTests<<"\n";
}
