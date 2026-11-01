#include "ccp/research_multicloth.hpp"
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <vector>
using namespace ccp::research;
static void require(bool result,const char*message){if(!result)throw std::runtime_error(message);}
int main(){
 try{
  // Certificate and local invalidation operate on stable primitive IDs.
  RegionalCandidates regions;
  Metrics initial,stable,moved;
  std::vector<Primitive> items={
   {{{0},{0,0,0},{.25,.25,.25},1u,2u,true},0,0},
   {{{1},{.2,0,0},{.5,.25,.25},2u,1u,true},1,1},
   {{{2},{4,4,4},{4.3,4.3,4.3},1u,2u,true},2,0}
  };
  auto result=regions.query(items,.01,true,initial);
  require(result==std::vector<Pair>{{0,1}},"region initial proximity candidate");
  result=regions.query(items,.01,true,stable);
  require(stable.regionalPairReuses>0 && stable.dirtyRegions==0,
          "certified reuse of unaffected regions");
  require(result==std::vector<Pair>{{0,1}},"region exact pair agreement");
  items[2].box.min={.21,0,.01};items[2].box.max={.3,.25,.26};
  result=regions.query(items,.01,true,moved);
  // Primitive 2 is the same category as primitive 0 but overlaps primitive
  // 1. The new pair must be found, while unaffected region pairs are reused.
  require(result==std::vector<Pair>{{0,1},{1,2}},
          "local invalidation discovers newly eligible pair");
  require(moved.dirtyRegions>=1 && moved.regionalPairReuses>=1,
          "partial invalidation leaves certified region-pairs reusable");

  // Actual dynamic cloth with cross-sheet positional contact corrections.
  // It runs with 1, 2, and 3 deforming sheets, and 1-2 moving obstacles.
  for(int sheets=1;sheets<=3;++sheets){
   MultiCloth world(sheets,2,Policy::RegionalCertified);
   world.setAuditing(true);
   const auto initialBall=world.spheres().front().center;
   const auto initialPositions=world.positions();
   std::size_t totalInter=0,totalSphere=0;
   for(int i=0;i<75;++i){
    world.step();
    const auto& m=world.metrics();
    totalInter+=m.interSheetContacts;
    totalSphere+=m.sphereContacts;
    require(m.stepMs>=0&&std::isfinite(m.stepMs)&&std::isfinite(m.broadMs),
            "dynamic physics time finite");
    require(m.queryCount==5,"broad phase called for each constraint iteration");
    for(const auto& p:world.positions())
     require(std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z),
             "dynamic cloth position finite");
   }
   require(world.frame()==75 && world.positions().size()==std::size_t(sheets*13*13),
           "actual moving cloth scene dimensions");
   const auto moved=world.spheres().front().center;
   require(std::abs(moved.x-initialBall.x)>0.1,
           "obstacles move continuously and cannot settle forever");
   double displacement=0;
   for(std::size_t i=0;i<world.positions().size();++i)
    displacement=std::max(displacement,std::abs(world.positions()[i].z-initialPositions[i].z));
   require(displacement>.1,"cloth genuinely deforms under physics");
   if(sheets>1)require(totalInter>0,"distinct cloth sheets actually resolve contacts");
   require(totalSphere>0,"cloth interacts with moving sphere obstacle");
  }
  // Paired state equivalence: region policy must never alter physical
  // response versus independent SAP on the SAME dynamic geometry.
  MultiCloth standard(2,2,Policy::RebuildSAP),regional(2,2,Policy::RegionalCertified);
  standard.setAuditing(true);regional.setAuditing(true);
  for(int frame=0;frame<65;++frame){
   standard.step();regional.step();
   require(standard.positions().size()==regional.positions().size(),"paired particle count");
   for(std::size_t i=0;i<standard.positions().size();++i) {
    const auto a=standard.positions()[i],b=regional.positions()[i];
    const double d=std::sqrt((a.x-b.x)*(a.x-b.x)+(a.y-b.y)*(a.y-b.y)+(a.z-b.z)*(a.z-b.z));
    if(d>1e-8)throw std::runtime_error("regional broad phase changed actual solver state");
   }
  }
  std::cout<<"Region certification, local invalidation, moving 1/2/3 cloths, "
              "real inter-sheet contacts, and paired SAP state equivalence PASS\n";
 }catch(const std::exception& e){
  std::cerr<<"MULTICLOTH RESEARCH TEST FAILED: "<<e.what()<<'\n';
  return 1;
 }
}
