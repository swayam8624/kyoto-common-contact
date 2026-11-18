// Phase 6 exact region-envelope sweep contract. The comparison is against
// BOTH an independent O(N^2) primitive oracle and the frozen regional
// all-region-pairs baseline. This checks correctness, not speedup.
#include "ccp/cbp_region_router.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <random>
#include <stdexcept>
#include <vector>

namespace {
using ccp::cbp::Bounds;
using ccp::cbp::Pairs;
using ccp::cbp::RegionRouter;
using ccp::cbp::CrossMode;
void need(bool ok,const char* message){
 if(!ok)throw std::runtime_error(message);
}
Pairs oracle(const std::vector<Bounds>& boxes){
 Pairs result;
 for(std::size_t i=0;i<boxes.size();++i)
  for(std::size_t j=i+1;j<boxes.size();++j)
   if(ccp::cbp::compatible(boxes[i],boxes[j]) &&
      ccp::cbp::overlap(boxes[i],boxes[j]))
    result.push_back({static_cast<int>(i),static_cast<int>(j)});
 return result;
}
void exact(RegionRouter& old,RegionRouter& swept,
           const std::vector<Bounds>& boxes,const std::vector<int>& labels){
 const auto reference=oracle(boxes);
 const auto a=old.query(boxes,labels);
 const auto b=swept.query(boxes,labels);
 need(a==reference,"Frozen region-all-pairs oracle mismatch");
 need(b==reference,"New X-swept region join lost exact candidates");
 need(swept.telemetry().regionPairTests<=old.telemetry().regionPairTests,
      "X-sweep visited more region pairs than the all-pairs control");
 need(swept.telemetry().regions==old.telemetry().regions,
      "X-sweep changed topology partition");
}
}
int main(){
 RegionRouter old,swept;
 old.setCrossMode(CrossMode::Exhaustive);
 swept.setCrossMode(CrossMode::Exhaustive);
 swept.setRegionSweep(true);
 old.setSapThreshold(9);
 swept.setSapThreshold(9);
 exact(old,swept,{},{});

 constexpr int N=240;
 std::mt19937_64 rng(0xC0FFEE);
 std::uniform_real_distribution<double> jitter(-.13,.13);
 std::vector<Bounds> boxes;
 boxes.reserve(N);
 std::vector<int> labels(N);
 for(int i=0;i<N;++i){
  const double x=(i%30)*1.7,y=((i/30)%8)*1.3;
  boxes.push_back({i,{x,y,.0},{x+.8,y+.85,.5},
                   (i%5==0?2u:1u),(i%17==0?0u:3u),true});
  labels[static_cast<std::size_t>(i)]=i;
 }
 exact(old,swept,boxes,labels);
 need(swept.telemetry().regionPairTests <
      old.telemetry().regionPairTests/5,
      "Separated patches did not prune most X-disjoint region pairs");

 for(int frame=0;frame<110;++frame){
  for(int i=0;i<N;++i){
   auto& b=boxes[static_cast<std::size_t>(i)];
   const double dx=frame%10==0?double(i%30)*.02:jitter(rng)*.05;
   b.min.x+=dx;b.max.x+=dx;
   if(frame%7==0){
    const double dy=jitter(rng);
    b.min.y+=dy;b.max.y+=dy;
   }
   if(frame>=30&&frame<54){ // Dense overlapping scene
    const double x=double(i%12)*.003,y=double(i/12)*.003;
    b.min={x,y,0.};b.max={x+1.1,y+1.1,.7};
   }
   if(frame==62&&i%7==0)b.mask=0;
   if(frame==74&&i%9==0)b.mask=3;
  }
  if(frame==18)for(int i=0;i<N;++i)labels[static_cast<std::size_t>(i)]=i/4;
  if(frame==43)for(int i=0;i<N;++i)labels[static_cast<std::size_t>(i)]=i/20;
  if(frame==71)for(int i=0;i<N;++i)labels[static_cast<std::size_t>(i)]=i;
  if(frame==93)std::fill(labels.begin(),labels.end(),0);
  exact(old,swept,boxes,labels);
 }
 // Adversarial X touching: x_min == other.max cannot be pruned.
 boxes.resize(2);labels.resize(2);
 boxes[0]={0,{0,0,0},{1,1,1},1,3,true};
 boxes[1]={1,{1,0,0},{2,1,1},2,3,true};
 labels={0,1};
 exact(old,swept,boxes,labels);
 need(swept.query(boxes,labels).size()==1,"Boundary-touching X pair lost");

 bool rejected=false;
 boxes[0].max.y=std::numeric_limits<double>::infinity();
 try{(void)swept.query(boxes,labels);}
 catch(const std::invalid_argument&){rejected=true;}
 need(rejected,"Invalid infinite AABB accepted");
 std::cout<<"PASS exact X-swept region join: 110 transitions, dense, "
             "repartition, masks, boundary touches and sparse pruning\n";
}
