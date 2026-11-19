// Independent topology/geometry regression for the ACTUAL coupled contact
// solver. This complements the separate mass-spring fixture oracle. No
// frozen controller changes and no output speedup claims.
#include "ccp/research_multicloth.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using ccp::research::MultiCloth;
using ccp::research::Policy;
using ccp::research::Vec3;
void check(bool valid,const char* note){if(!valid)throw std::runtime_error(note);}
double distance(Vec3 a,Vec3 b){
 return std::hypot(std::hypot(a.x-b.x,a.y-b.y),a.z-b.z);
}
bool samePositions(const MultiCloth& a,const MultiCloth& b,double epsilon=0.){
 if(a.positions().size()!=b.positions().size())return false;
 for(std::size_t i=0;i<a.positions().size();++i)
  if(distance(a.positions()[i],b.positions()[i])>epsilon)return false;
 return true;
}
void meshValid(const MultiCloth& sim){
 const auto& nodes=sim.positions();
 const auto& triangles=sim.triangles();
 check(!nodes.empty()&&!triangles.empty(),"invalid empty coupled physical mesh");
 for(const auto& p:nodes)
  check(std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z),
        "coupled physical simulation has nonfinite vertex");
 for(const auto& t:triangles){
  for(int id:t)check(id>=0&&std::size_t(id)<nodes.size(),
                        "invalid coupled mesh vertex ID");
  const auto a=nodes[t[0]],b=nodes[t[1]],c=nodes[t[2]];
  const Vec3 u{b.x-a.x,b.y-a.y,b.z-a.z},v{c.x-a.x,c.y-a.y,c.z-a.z};
  const Vec3 cross{u.y*v.z-u.z*v.y,u.z*v.x-u.x*v.z,u.x*v.y-u.y*v.x};
  // Only for INITIAL or short stable simulations: a degenerate triangle
  // cannot reliably compute its normal for collision response.
  check(std::hypot(std::hypot(cross.x,cross.y),cross.z)>1e-8,
        "degenerate physical triangle in heldout coupled solver");
 }
}
void setup(MultiCloth& sim,std::uint32_t geometrySeed,
           std::uint32_t topologySeed){
 sim.setFixtureVariation(geometrySeed,topologySeed);
 sim.setSelfCollisions(true);
 sim.setAuditing(true);
}
}
int main(){
 try{
  MultiCloth legacy(2,2,Policy::RebuildSAP);
  MultiCloth explicitZero(2,2,Policy::RebuildSAP);
  setup(explicitZero,0u,0u);
  check(samePositions(legacy,explicitZero),"zero seeds changed old physical positions");
  check(legacy.triangles()==explicitZero.triangles(),
        "zero seeds changed old physical triangulation");
  for(int f=0;f<3;++f){
   legacy.step();explicitZero.step();
   check(samePositions(legacy,explicitZero),
         "zero seeds changed old coupled solver outcome");
  }

  MultiCloth a(2,2,Policy::RebuildSAP);
  MultiCloth again(2,2,Policy::RebuildSAP);
  MultiCloth changed(2,2,Policy::RebuildSAP);
  MultiCloth otherTopo(2,2,Policy::RebuildSAP);
  MultiCloth otherGeom(2,2,Policy::RebuildSAP);
  setup(a,73013u,86011u);
  setup(again,73013u,86011u);
  setup(changed,74017u,86019u);
  setup(otherTopo,73013u,86019u);
  setup(otherGeom,74017u,86011u);
  check(samePositions(a,again)&&a.triangles()==again.triangles(),
        "coupled seed experiment non-deterministic");
  check(!samePositions(a,changed),"different geometry seed did not affect solver vertices");
  check(a.triangles()!=changed.triangles(),
        "different topology seed did not affect physical triangle adjacency");
  check(samePositions(a,otherTopo),
        "topology seed independently changed geometry coordinates");
  check(a.triangles()!=otherTopo.triangles(),
        "different topology seed did not independently change triangles");
  check(!samePositions(a,otherGeom),
        "different geometry seed did not independently vary vertices");
  check(a.triangles()==otherGeom.triangles(),
        "geometry seed inadvertently changed triangle connectivity");
  // Topology seed 86019 is divisible by three, so the alternative
  // fixtures also pin the upper-middle node on each sheet. This is a
  // genuine, separately validated boundary-constraint variant.
  check(a.pinnedCount()==4&&changed.pinnedCount()==6 &&
        otherTopo.pinnedCount()==6&&otherGeom.pinnedCount()==4,
        "independent topology seed did not change anchored physical nodes");
  meshValid(a);meshValid(changed);

  MultiCloth guard(2,2,Policy::GlobalGuard);
  MultiCloth frozen(2,2,Policy::FinalBounded);
  setup(guard,73013u,86011u);
  setup(frozen,73013u,86011u);
  for(int f=0;f<8;++f){
   a.step();guard.step();frozen.step();
   meshValid(a);meshValid(guard);meshValid(frozen);
   check(samePositions(a,guard,1e-7),
         "Guard physically diverged from heldout SAP-driven contact solver");
   check(samePositions(a,frozen,1e-7),
         "frozen V4 physically diverged from heldout SAP-driven solver");
   check(a.metrics().oracleMisses==0&&guard.metrics().oracleMisses==0&&
         frozen.metrics().oracleMisses==0,
         "heldout solver returned missing candidates");
  }
  // Explicitly constructed self-overlap is an ISOLATED discrete-contact
  // diagnostic, separate from natural free/pull/fold scenarios. The
  // historical test-only displacePatchForTest shifts a non-anchored patch
  // onto a distant part of the SAME sheet; runContactPassForTest uses a
  // fresh SAP reference pair set and projects precise self-contacts.
  // It does not time V4 or demonstrate self-contact induced naturally
  // by a drag gesture. Its only purpose is fail-closed self-collision
  // coverage across genuinely different mesh triangulations.
  for(const auto [geometry,topology]:
      std::array<std::array<std::uint32_t,2>,3>{{
       {{73013u,86011u}},{{74017u,86019u}},{{75029u,86029u}}
      }}){
   MultiCloth folded(1,1,Policy::RebuildSAP);
   setup(folded,geometry,topology);
   folded.displacePatchForTest(0,2,2,3,{.915,0,.016});
   const auto before=folded.positions();
   const std::size_t self=folded.runContactPassForTest();
   check(self>0,"heldout constructed nonlocal overlap yielded zero self-contacts");
   check(folded.metrics().selfCandidatePairs>=self,
         "same-sheet contacts exceed independent SAP candidate set");
   check(folded.metrics().selfTopologyFiltered>0,
         "self-neighbor topology exclusions no longer apply");
   double maxMoved=0.;
   for(std::size_t j=0;j<before.size();++j)
    maxMoved=std::max(maxMoved,distance(before[j],folded.positions()[j]));
   check(maxMoved>1e-6&&maxMoved<.05,
         "same-sheet contact did not produce a finite bounded correction");
   MultiCloth control(1,1,Policy::RebuildSAP);
   setup(control,geometry,topology);
   control.setSelfCollisions(false);
   control.displacePatchForTest(0,2,2,3,{.915,0,.016});
   check(control.runContactPassForTest()==0,
         "disabled nonlocal self-contact emitted false response");
   std::cout<<"PASS ISOLATED NONLOCAL SELF CONTACT geometry="<<geometry
            <<" topology="<<topology
            <<" precise_self_responses="<<self
            <<" bounded_max_correction="<<maxMoved<<"\\n";
  }

  bool blocked=false;
  try{a.setFixtureVariation(17u,19u);}catch(const std::logic_error&){blocked=true;}
  check(blocked,"late topology change illegally invalidated running physics");
  std::cout<<"PASS: original MultiCloth fixture unchanged; independent"
              " physical vertex geometry/triangulation validated; frozen"
              " SAP/Guard/V4 solver outputs and full mesh state match\n";
  return 0;
 }catch(const std::exception& e){
  std::cerr<<"COUPLED TOPOLOGY FIXTURE FAILED: "<<e.what()<<"\n";
  return 1;
 }
}
