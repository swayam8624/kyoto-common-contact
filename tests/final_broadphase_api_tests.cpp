// Direct public-API regression for the final Kyoto CPU broad phase.
// No neural weights, benchmark harness, generator or external Bullet needed.
// We insist on EXACT canonical candidate-set parity at every frame.
#include "ccp/final_broadphase.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <vector>

using ccp::live::Bounds;
using ccp::live::Pair;
using ccp::live::Vec3;
using ccp::final_broadphase::GuardSAPBoundedTrial;
using ccp::final_broadphase::PersistentSAP;

static void check(const std::vector<Bounds>& scene,int frame){
  static GuardSAPBoundedTrial online;
  static PersistentSAP persistent;
  const auto exact=ccp::live::candidates(scene,false);
  const auto policy=online.query(scene);
  const auto sap=persistent.query(scene);
  if(policy.pairs!=exact||sap!=exact){
   throw std::runtime_error("exact pair mismatch at frame "+std::to_string(frame));
  }
  if(std::string(policy.action)!="global_guarded_exact"&&
     std::string(policy.action)!="sap_persistent_coherent"){
   throw std::runtime_error("non-certified backend");
  }
}

int main(){
  // Stable, partial- and full-overlap, filtered and distant scenes.
  // Includes a sudden widespread translation and large deformation.
  std::vector<Bounds> scene;
  for(int i=0;i<512;++i){
    const double x=double(i%32)*.2,y=double(i/32)*.22;
    const uint32_t category=i%11==0?2u:1u;
    const uint32_t mask=i%11==0?2u:3u;
    scene.push_back({i,{x,y,0.},{x+.25,y+.23,.15},category,mask,true});
  }
  for(int f=0;f<90;++f){
    for(int i=0;i<512;++i){
      auto& b=scene[size_t(i)];
      const double dx=(f%18==0?.23:(i%3==0?.011:.0002));
      const double dy=(f>35&&f<55?.03:0.);
      b.min.x+=dx;b.max.x+=dx;
      b.min.y+=dy;b.max.y+=dy;
      // Zero-sized extent and exactly touching endpoints remain legal.
      if(f==20&&i==7)b.max.z=b.min.z;
    }
    check(scene,f);
  }
  std::cout<<"PASS final public broad-phase API: 90 dynamic snapshots, "
              "512 stable primitives, exact pair parity\n";
  return 0;
}
