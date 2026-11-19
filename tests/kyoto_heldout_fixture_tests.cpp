// Independent geometry/connectivity tests for Kyoto held-out material fixtures.
// No PDF/render substitution and no policy retuning. These tests exercise
// REAL mass-spring nodes and graph topology, not different phase offsets.
#include "ccp/material_scenes.hpp"
#include "ccp/final_broadphase.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using ccp::materials::World;
using ccp::materials::Variation;
using ccp::live::Bounds;
using ccp::live::Pair;
using ccp::materials::Vec3;
void ensure(bool condition,const char* message){
 if(!condition)throw std::runtime_error(message);
}
bool same(const Vec3& a,const Vec3& b){
 return a.x==b.x&&a.y==b.y&&a.z==b.z;
}
bool geometryEqual(const World& a,const World& b){
 if(a.nodes()!=b.nodes())return false;
 for(std::size_t i=0;i<a.nodes();++i)
  if(!same(a.visualNodes()[i].x,b.visualNodes()[i].x))return false;
 return true;
}
bool connectivityEqual(const World& a,const World& b){
 if(a.elements()!=b.elements()||a.bonds()!=b.bonds())return false;
 for(std::size_t i=0;i<a.elements();++i)
  if(a.visualElements()[i].vertices!=b.visualElements()[i].vertices ||
     a.visualElements()[i].count!=b.visualElements()[i].count)return false;
 for(std::size_t i=0;i<a.bonds();++i)
  if(a.visualBonds()[i].a!=b.visualBonds()[i].a ||
     a.visualBonds()[i].b!=b.visualBonds()[i].b)return false;
 return true;
}
double triangleArea(Vec3 a,Vec3 b,Vec3 c){
 const auto u=ccp::materials::sub(b,a),v=ccp::materials::sub(c,a);
 const auto p=Vec3{u.y*v.z-u.z*v.y,u.z*v.x-u.x*v.z,u.x*v.y-u.y*v.x};
 return ccp::materials::length(p)*.5;
}
void auditPhysicalState(const World& world){
 ensure(world.nodes()>0&&world.elements()>0&&world.bonds()>0,
        "empty mass-spring fixture");
 std::size_t pinned=0,clothTriangles=0;
 for(const auto& node:world.visualNodes()){
  if(node.pin)++pinned;
  ensure(std::isfinite(node.x.x)&&std::isfinite(node.x.y)&&
         std::isfinite(node.x.z),"non-finite evolved physical coordinates");
 }
 ensure(pinned>=1,"fixture has no anchored nodes");
 for(const auto& bond:world.visualBonds()){
  ensure(bond.a>=0&&bond.b>=0&&
    std::size_t(bond.a)<world.nodes()&&std::size_t(bond.b)<world.nodes()&&
    bond.a!=bond.b&&bond.rest>0.,"bad bond topology or rest length");
 }
 for(const auto& e:world.visualElements()){
  ensure(e.count==2||e.count==3||e.count==8,
         "invalid physical mesh element arity");
  for(int j=0;j<e.count;++j)
   ensure(e.vertices[j]>=0&&std::size_t(e.vertices[j])<world.nodes(),
          "element contains invalid vertex");
  if(e.material==0){
   ++clothTriangles;
   ensure(e.count==3,"nontriangle cloth element");
   const auto& nodes=world.visualNodes();
   ensure(triangleArea(nodes[e.vertices[0]].x,nodes[e.vertices[1]].x,
                       nodes[e.vertices[2]].x)>1e-9,
          "collapsed or inverted initial cloth triangle");
  }
 }
 (void)clothTriangles;
 const auto snapshot=world.snapshot(64);
 ensure(snapshot.size()==world.elements(),"snapshot does not cover mesh elements");
 for(std::size_t i=0;i<snapshot.size();++i){
  const auto& p=snapshot[i];
  ensure(p.box.id==static_cast<int>(i),"unstable primitive ID");
  ensure(p.box.min.x<=p.box.max.x&&p.box.min.y<=p.box.max.y&&
         p.box.min.z<=p.box.max.z,"inverted physical AABB");
 }
}
void compareExactAABBs(World& world){
 ccp::final_broadphase::GuardSAPBoundedTrial frozen;
 for(int f=0;f<12;++f){
  world.step();
  auditPhysicalState(world);
  std::vector<Bounds> boxes;
  const auto items=world.snapshot(64);
  for(const auto& item:items){
   auto b=item.box;
   b.min.x-=.0125;b.min.y-=.0125;b.min.z-=.0125;
   b.max.x+=.0125;b.max.y+=.0125;b.max.z+=.0125;
   boxes.push_back(b);
  }
  const auto oracle=ccp::live::candidates(boxes,false);
  const auto sap=ccp::live::candidates(boxes,true);
  const auto actual=frozen.query(boxes);
  ensure(oracle==sap,"SAP differs from independent exhaustive oracle");
  ensure(oracle==actual.pairs,"frozen V4 missed/added held-out geometry AABB pairs");
 }
}
}
int main(){
 try{
  // Zero variation must use the same node positions and original two
  // diagonal spring bonds as the Phase 18 historical fixture construction.
  World original("cloth",128,3203,"rapid");
  World oldExplicit("cloth",128,3203,"rapid",Variation{});
  ensure(geometryEqual(original,oldExplicit)&&
         connectivityEqual(original,oldExplicit),
         "default variation changed original fixture construction");
  for(int frame=0;frame<6;++frame){
   original.step();oldExplicit.step();
   ensure(geometryEqual(original,oldExplicit),
          "zero variation changed historically stepped positions");
  }

  const Variation first{725101u,951317u,.012};
  const Variation second{725102u,951318u,.012};
  World clothA("cloth",128,3301,"rapid",first);
  World clothAagain("cloth",128,3301,"rapid",first);
  World clothB("cloth",128,3301,"rapid",second);
  World clothSameGeometryOtherTopo("cloth",128,3301,"rapid",
                                 Variation{first.geometrySeed,second.topologySeed,.012});
  World clothSameTopoOtherGeometry("cloth",128,3301,"rapid",
                                 Variation{second.geometrySeed,first.topologySeed,.012});
  ensure(geometryEqual(clothA,clothAagain) &&
         connectivityEqual(clothA,clothAagain),
         "held-out fixture not deterministic from independent seeds");
  ensure(!geometryEqual(clothA,clothB),
         "different geometry seeds did not vary actual coordinates");
  ensure(!connectivityEqual(clothA,clothB),
         "different topology seeds did not vary mesh connectivity");
  ensure(geometryEqual(clothA,clothSameGeometryOtherTopo),
         "topology seed unexpectedly changes initial node coordinates");
  ensure(!connectivityEqual(clothA,clothSameGeometryOtherTopo),
         "topology seed has no independent topology effect");
  ensure(!geometryEqual(clothA,clothSameTopoOtherGeometry),
         "geometry seed did not perturb initial coordinates");
  ensure(connectivityEqual(clothA,clothSameTopoOtherGeometry),
         "geometry seed unexpectedly changed graph connectivity");

  // Direct oracle checks on several real meshed material families.
  for(const auto* type:{"cloth","sponge","cushion","soft_ball","spring","mixed"}){
   World varied(type,128,3301,"rapid",first);
   auditPhysicalState(varied);
   compareExactAABBs(varied);
   std::cout<<"PASS independent seed material "<<type
            <<" elements="<<varied.elements()<<" nodes="<<varied.nodes()
            <<" bonds="<<varied.bonds()<<"\n";
  }
  World differentVolume("sponge",128,3301,"rapid",second);
  World originalVolume("sponge",128,3301,"rapid",first);
  ensure(!connectivityEqual(differentVolume,originalVolume),
         "volumetric cross-bracing did not vary with topology seed");
  // A change in topologySeed MUST change the physical connectivity,
  // independently of initial coordinates, across ALL six families.
  // Previously soft_ball and spring only varied vertex positions;
  // that would make the phrase "independent topology" misleading.
  for(const auto* type:{"cloth","sponge","cushion","soft_ball","spring","mixed"}){
   World base(type,128,3301,"rapid",
              Variation{73013u,86011u,.012});
   World changedTopology(type,128,3301,"rapid",
                         Variation{73013u,86019u,.012});
   ensure(geometryEqual(base,changedTopology),
          "independent topology seed unexpectedly changed initial node geometry");
   ensure(!connectivityEqual(base,changedTopology),
          "independent topology seed failed to change a material connectivity graph");
   auditPhysicalState(base);
   auditPhysicalState(changedTopology);
  }
  std::cout<<"PASS independent node geometry, material triangulation,"
              " volumetric connectivity, stable IDs, exact V4 AABB pairs,"
              " backward-compatible defaults and finite mesh evolution\n";
  return 0;
 }catch(const std::exception& e){
  std::cerr<<"HELDOUT MATERIAL FIXTURE FAILED: "<<e.what()<<"\n";return 1;
 }
}
