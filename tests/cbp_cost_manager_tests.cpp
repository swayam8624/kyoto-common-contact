// CBP Phase 3: independently verified causal routing across transitions.
// Checks the actual returned pair set, not a predicted cost or hash alone.
#include "ccp/cbp_cost_manager.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

namespace {
using ccp::cbp::Bounds;
using ccp::cbp::CrossMode;
using ccp::cbp::CausalCostRouter;
using ccp::cbp::Pairs;
void require(bool x,const char* msg){if(!x)throw std::runtime_error(msg);}
Pairs oracle(const std::vector<Bounds>& boxes){
 Pairs out;
 for(std::size_t i=0;i<boxes.size();++i)
  for(std::size_t j=i+1;j<boxes.size();++j){
   const auto& a=boxes[i];const auto& b=boxes[j];
   if(!ccp::cbp::compatible(a,b)||!ccp::cbp::overlap(a,b))continue;
   out.push_back({a.id,b.id});
  }
 return out;
}
void generate(std::vector<Bounds>& boxes,std::vector<int>& labels,
              int scene,int time){
 const int n=static_cast<int>(boxes.size());
 for(int i=0;i<n;++i){
  double x{},y{},z{};
  if(scene==0||scene==3){ // Separated geometry: certificate reuse should work
   x=double(i)*3.5;
  }else if(scene==1||scene==4){ // Moderate grid contact
   x=double(i%16)*.44;
   y=double(i/16)*.44;
   z=double(i%3)*.06;
  }else{ // Dense all-overlap frame: O(N^2) pair output is inherent
   x=double(i%16)*.002;
   y=double(i/16)*.002;
  }
  // The high-motion stage forces broad spatial movement without
  // changing any future-step hint available to the controller.
  const double delta=scene==4?.07*double(time):.0004*double((i%5)-2)*time;
  x+=delta;
  const double size=scene==2?2.0:scene==0||scene==3?.5:.55;
  auto& b=boxes[static_cast<std::size_t>(i)];
  b={i,{x,y,z},{x+size,y+size,z+size},
      (i%9==0?2u:1u),(i%19==0?0u:3u),true};
  labels[static_cast<std::size_t>(i)]=i%8;
 }
}
}

int main(){
 constexpr int N=256;
 CausalCostRouter manager;
 manager.setSapThreshold(48);
 manager.setCachePadding(.045);
 require(manager.query({},{}).empty(),"Empty scene not accepted");
 manager.reset();

 std::vector<Bounds> boxes(static_cast<std::size_t>(N));
 std::vector<int> labels(static_cast<std::size_t>(N));
 std::size_t switches=0,cacheFrames=0,exhaustiveFrames=0,bvhFrames=0;
 for(int stage=0;stage<5;++stage){
  for(int t=0;t<24;++t){
   generate(boxes,labels,stage,t);
   // Test ownership migration and a category/mask-only update.
   if(stage==1&&t==9)for(int i=0;i<N;++i)labels[static_cast<std::size_t>(i)]=i%11;
   if(stage==3&&t==5)boxes[32].mask=0;
   if(stage==3&&t==11)boxes[7].max.z=boxes[7].min.z;
   const auto expected=oracle(boxes);
   const auto actual=manager.query(boxes,labels);
   require(actual==expected,"Causal controller broke exact current pair oracle");
   const auto d=manager.decision();
   require(d.totalMs>=d.backendMs,"Router timing omitted the backend");
   require(d.sampledDensity>=0.&&d.sampledDensity<=1.,"Invalid sampled density");
   require(d.densitySamples<=120,"Sampler evaluated too many pairs");
   if(d.chosen==CrossMode::CertifiedCache)++cacheFrames;
   if(d.chosen==CrossMode::Exhaustive)++exhaustiveFrames;
   if(d.chosen==CrossMode::SharedBVH)++bvhFrames;
   if(d.switched)++switches;
   // Dense stage must not maintain an expensive O(N^2) graph.
   if(stage==2 && t>=2)
    require(d.chosen==CrossMode::Exhaustive,"Dense output did not trigger exact fallback");
  }
 }
 require(cacheFrames>0 && exhaustiveFrames>0,"Manager never selected distinct backends");
 require(switches>=2,"Manager did not adapt to regime transitions");
 require(bvhFrames>0,"Manager failed to use direct BVH for rapid motion");
 require(manager.decision().switchedTotal==switches,"Switch telemetry inaccurate");

 bool rejected=false;
 try{
  boxes[0].min.x=std::numeric_limits<double>::quiet_NaN();
  (void)manager.query(boxes,labels);
 }catch(const std::invalid_argument&){rejected=true;}
 require(rejected,"Causal manager accepted NaN");
 generate(boxes,labels,0,0);
 require(manager.query(boxes,labels)==oracle(boxes),
         "Invalid input corrupted subsequent controller state");

 std::cout<<"PASS causal CBP 120 transition frames, exact pairs; cache="
          <<cacheFrames<<" exhaustive="<<exhaustiveFrames
          <<" bvh="<<bvhFrames<<" switches="<<switches<<"\n";
}
