// Independent exhaustive oracle / dynamic-transition regression for CBP Phase 0/1.
#include "ccp/cbp_region_router.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <random>
#include <stdexcept>
#include <vector>

using ccp::cbp::Bounds;
using ccp::cbp::Pairs;
using ccp::cbp::Kernel;
using ccp::cbp::RegionRouter;

Pairs oracle(const std::vector<Bounds>& b){
 Pairs result;
 for(std::size_t i=0;i<b.size();++i)
  for(std::size_t j=i+1;j<b.size();++j){
   const auto& a=b[i];const auto& c=b[j];
   if((a.mask&c.category)==0 || (c.mask&a.category)==0)continue;
   if(a.min.x<=c.max.x && c.min.x<=a.max.x &&
      a.min.y<=c.max.y && c.min.y<=a.max.y &&
      a.min.z<=c.max.z && c.min.z<=a.max.z)
    result.push_back({int(i),int(j)});
  }
 return result;
}
static void check(RegionRouter& router,const std::vector<Bounds>& scene,
                  const std::vector<int>& regions){
 const auto actual=router.query(scene,regions);
 const auto correct=oracle(scene);
 if(actual!=correct)throw std::runtime_error("CBP lost or fabricated a pair");
}
int main(){
 RegionRouter r;
 check(r,{},{});
 std::vector<Bounds> b{
  {0,{0,0,0},{1,1,1},1,3,true},
  {1,{1,0,0},{2,1,1},2,3,true},
  {2,{1.5,0,0},{2.5,1,1},1,3,true},
  {3,{25,0,0},{26,1,1},2,1,true}
 };
 std::vector<int> labels{0,1,1,2};
 check(r,b,labels);
 r.setKernel(1,Kernel::PersistentSAP);
 check(r,b,labels);
 b[2].min.x=0.5; b[2].max.x=1.5;
 check(r,b,labels);
 b[1].mask=0;
 check(r,b,labels);
 labels[1]=7;
 check(r,b,labels);
 if(r.telemetry().topologyResets<2)throw std::runtime_error("Repartition not registered");
 r.setSapThreshold(2);
 check(r,b,labels);

 std::mt19937_64 rng(20261117);
 std::uniform_real_distribution<double> u(-10,10);
 std::vector<Bounds> scene;
 for(int i=0;i<240;++i){
  const double x=u(rng),y=u(rng),z=u(rng);
  scene.push_back({i,{x,y,z},{x+1.3,y+.8,z+.6},
   (i%5==0?2u:1u),(i%11==0?0u:3u),true});
 }
 std::vector<int> parts(240);
 for(int i=0;i<240;++i)parts[std::size_t(i)]=i%7;
 r.reset();
 for(int t=0;t<90;++t){
  for(int i=0;i<240;++i){
   auto& a=scene[std::size_t(i)];
   const double delta=t%13==0?u(rng):.021*double((i%7)-3);
   a.min.x+=delta; a.max.x+=delta;
   if(t%17==0){a.min.y+=.2;a.max.y+=.2;}
  }
  if(t==35)std::fill(parts.begin(),parts.end(),0);
  if(t==55)for(int i=0;i<240;++i)parts[std::size_t(i)]=i%11;
  check(r,scene,parts);
 }
 bool rejected=false;
 try{auto bad=scene;bad[0].min.x=std::nan("");(void)r.query(bad,parts);}
 catch(const std::invalid_argument&){rejected=true;}
 if(!rejected)throw std::runtime_error("NaN bound accepted");
 rejected=false;
 try{auto bad=scene;bad[0].id=10;(void)r.query(bad,parts);}
 catch(const std::invalid_argument&){rejected=true;}
 if(!rejected)throw std::runtime_error("Changed primitive ID accepted");
 std::cout<<"PASS: CBP exact indexed cross-patch coverage / local SAP, 90 dynamic frames\n";
}
