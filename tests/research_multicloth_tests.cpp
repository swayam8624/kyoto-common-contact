#include "ccp/research_multicloth.hpp"
#include <cmath>
#include <algorithm>
#include <map>
#include <limits>
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

  // A two-tier patch-union sweep must produce exactly the same current
  // AABB candidates as both the original quadratic region-pair scan and
  // the independent exhaustive oracle, even under abrupt displacements.
  RegionalCandidates flatFrontier,hierarchicalFrontier;
  std::size_t accumulatedSkipped=0,accumulatedReuse=0;
  for(int frame=0;frame<80;++frame){
   std::vector<Primitive> scene;
   for(int id=0;id<96;++id){
    const double jump=(frame%13==0 && id%11==0)?1.3:0.0;
    const double x=(id%12)*.42+.08*std::sin(.34*frame+id)+jump;
    const double y=(id/12)*.44+.07*std::cos(.22*frame+.3*id);
    const double z=.1*std::sin(.19*frame+.53*id);
    const std::uint32_t category=(id%2)?2u:1u;
    scene.push_back({{id,{x,y,z},{x+.25,y+.28,z+.21},
                     category,category==1u?2u:1u,true},id/4,id/2});
   }
   // Include a sparse, isolated region to exercise pruning.
   scene.push_back({{96,{60,60,60},{61,61,61},1u,2u,true},40,0});
   Metrics flatStats,sweptStats;
   const auto flat=flatFrontier.query(scene,.025,false,flatStats,false);
   const auto swept=hierarchicalFrontier.query(scene,.025,false,sweptStats,true);
   require(flat==swept,"hierarchical region sweep differs from flat region scan");
   std::vector<Bounds> padded;
   for(const auto& primitive:scene) {
    auto b=primitive.box;
    constexpr double h=.025/2;
    b.min.x-=h;b.min.y-=h;b.min.z-=h;
    b.max.x+=h;b.max.y+=h;b.max.z+=h;
    padded.push_back(b);
   }
   const auto oracle=ccp::live::candidates(padded,false);
   require(swept==oracle,"hierarchical region sweep missed or added AABB candidates");
   accumulatedSkipped+=sweptStats.regionPairsSkipped;
   accumulatedReuse+=sweptStats.regionalPairReuses;
  }
  require(accumulatedSkipped>0,"top-level region sweep never pruned disjoint blocks");
  require(accumulatedReuse>0,"top-level region sweep never reused any certified block");

  // Duplicate stable IDs and invalid coordinates make the certificate
  // non-sound. Reject them instead of silently losing candidates.
  {
   RegionalCandidates validated;
   const Primitive a{{1,{0,0,0},{1,1,1},1u,2u,true},0,0};
   const Primitive duplicate{{1,{.2,.2,.2},{.8,.8,.8},2u,1u,true},1,1};
   Metrics m;
   bool rejected=false;
   try{(void)validated.query({a,duplicate},.05,true,m,true);}
   catch(const std::invalid_argument&){rejected=true;}
   require(rejected,"duplicate primitive ID was not rejected");
   Primitive nan=a;
   nan.box.min.x=std::numeric_limits<double>::quiet_NaN();
   rejected=false;
   try{(void)validated.query({nan},.05,true,m,true);}
   catch(const std::invalid_argument&){rejected=true;}
   require(rejected,"nonfinite primitive bound was not rejected");
  }

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
  // Same physically coupled 3-cloth trajectory when using hierarchical
  // rather than full patch-pair enumeration. The precise solver is identical.
  MultiCloth full3(3,2,Policy::RegionalCertified);
  MultiCloth swept3(3,2,Policy::RegionalCertified);
  full3.setAdaptive(false);swept3.setAdaptive(false);
  swept3.setHierarchicalRegions(true);
  full3.setAuditing(true);swept3.setAuditing(true);
  std::size_t skippedInPhysics=0;
  for(int frame=0;frame<40;++frame){
   full3.step();swept3.step();
   skippedInPhysics+=swept3.metrics().regionPairsSkipped;
   const auto& a=full3.positions();const auto& b=swept3.positions();
   require(a.size()==b.size(),"hierarchical physics state size mismatch");
   for(std::size_t i=0;i<a.size();++i){
    const auto d=a[i],e=b[i];
    if(std::sqrt((d.x-e.x)*(d.x-e.x)+(d.y-e.y)*(d.y-e.y)+
                 (d.z-e.z)*(d.z-e.z))>1e-8)
     throw std::runtime_error("hierarchical frontier altered solver state");
   }
  }
  require(skippedInPhysics>0,"hierarchical frontier never excluded region pairs");
  // Varying patch granularity is a pure broad-phase experiment. Even
  // changing it mid-simulation must preserve the physical trajectory.
  MultiCloth patchReference(3,2,Policy::RebuildSAP);
  MultiCloth patchCandidate(3,2,Policy::RegionalCertified);
  patchReference.setAuditing(true);
  patchCandidate.setAuditing(true);
  patchCandidate.setHierarchicalRegions(true);
  const std::array<int,6> spans{1,2,3,4,6,12};
  for(int frame=0;frame<36;++frame){
   // Explicitly trigger cache invalidation on each partition change.
   if(frame%6==0)patchCandidate.setPatchCellSpan(spans[frame/6]);
   patchReference.step();
   patchCandidate.step();
   require(patchCandidate.patchCellSpan()==spans[frame/6],
           "new patch span not applied");
   const auto& a=patchReference.positions();
   const auto& b=patchCandidate.positions();
   for(std::size_t i=0;i<a.size();++i){
    const double dx=a[i].x-b[i].x,dy=a[i].y-b[i].y,dz=a[i].z-b[i].z;
    if(std::sqrt(dx*dx+dy*dy+dz*dz)>1e-8)
     throw std::runtime_error("patch-cell repartition changed physical trajectory");
   }
  }
  bool invalidSpanRejected=false;
  try{patchCandidate.setPatchCellSpan(5);}
  catch(const std::invalid_argument&){invalidSpanRejected=true;}
  require(invalidSpanRejected,"invalid patch cell span accepted");

  // The online controller must genuinely SPLIT and MERGE disjoint patches
  // in response to measured candidate work and confirmed contact activity.
  // This controlled signal test is separate from the real-physics test:
  // it proves the state machine can respond to changing contact regimes.
  {
   OnlinePartitioner controller;
   controller.reset(2);
   require(controller.patches().size()==8,"online initial 6x6 patch topology");
   std::size_t splits=0,merges=0,strategySwitches=0;
   for(int frame=1;frame<=160;++frame){
    std::map<int,RegionWork> signals;
    for(const auto& patch:controller.patches()){
     auto& w=signals[OnlinePartitioner::stableId(patch)];
     if(frame<=66){
      w.aabbChecks=1100;
      w.cachedChecks=240;
      w.candidateTouches=85;
      w.contactCorrections=22;
      w.invalidations=2;
     }
    }
    std::vector<Bounds> guards;
    // Initial dense patches: all union boxes overlap. Later separate them
    // to exercise runtime flat/hierarchical strategy feedback.
    for(std::size_t i=0;i<controller.patches().size();++i){
     const double x=frame<=66?0.:double(i)*8.;
     guards.push_back({int(i),{x,0,0},{x+1.,1,1},1u,2u,true});
    }
    const auto update=controller.observe(frame,signals,guards,1.8,5);
    splits+=update.splits;merges+=update.merges;
    strategySwitches+=update.strategySwitches;
    require(update.patches==controller.patches().size(),
            "online patch count in telemetry incorrect");
    bool coverage[2][12][12]={};
    for(const auto& patch:controller.patches()){
     for(int y=patch.y;y<patch.y+patch.span;++y)
      for(int x=patch.x;x<patch.x+patch.span;++x){
       require(!coverage[patch.sheet][y][x],"online patch overlap");
       coverage[patch.sheet][y][x]=true;
       require(controller.regionForCell(patch.sheet,x,y)==
               OnlinePartitioner::stableId(patch),"online cell ID incorrect");
      }
    }
    for(int sheet=0;sheet<2;++sheet)
     for(int y=0;y<12;++y)for(int x=0;x<12;++x)
      require(coverage[sheet][y][x],"online patch cover has a hole");
   }
   require(splits>0,"online controller never split contact-heavy patches");
   require(merges>0,"online controller never merged quiet sibling patches");
   require(strategySwitches>0,"online controller never changed search strategy");
  }

  // Against a real physically coupled 3-cloth SAP oracle, changing the
  // patch partition may NEVER change the precise contact path's outcome.
  // The simulated obstacle and anchor motion remains active throughout.
  {
   MultiCloth exact(3,2,Policy::RebuildSAP);
   MultiCloth adapted(3,2,Policy::OnlineContactAware);
   exact.setAuditing(true);
   adapted.setAuditing(true);
   std::size_t onlineSplits=0,onlineMerges=0,onlineModeSwitches=0;
   for(int frame=0;frame<96;++frame){
    exact.step();adapted.step();
    const auto& stats=adapted.metrics();
    onlineSplits+=stats.onlineSplits;
    onlineMerges+=stats.onlineMerges;
    onlineModeSwitches+=stats.onlineStrategySwitches;
    require(stats.onlinePatches>0,"online controller has no patches");
    require(std::isfinite(stats.onlineDecisionMs),
            "online decision time invalid");
    require(exact.positions().size()==adapted.positions().size(),
            "online controller changed particle topology");
    for(std::size_t i=0;i<exact.positions().size();++i){
     const auto a=exact.positions()[i],b=adapted.positions()[i];
     const double d=std::sqrt((a.x-b.x)*(a.x-b.x)+
                              (a.y-b.y)*(a.y-b.y)+
                              (a.z-b.z)*(a.z-b.z));
     if(d>1e-8)throw std::runtime_error(
       "online adaptive broad phase changed physical trajectory");
    }
   }
   require(onlineSplits>0,
           "three-cloth simulation never exercised online patch splits");
   (void)onlineMerges;(void)onlineModeSwitches;
  }

  std::cout<<"Region certification, local invalidation, moving 1/2/3 cloths, "
              "real inter-sheet contacts, and paired SAP state equivalence PASS\n";
 }catch(const std::exception& e){
  std::cerr<<"MULTICLOTH RESEARCH TEST FAILED: "<<e.what()<<'\n';
  return 1;
 }
}
