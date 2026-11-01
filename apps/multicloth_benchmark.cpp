// Real moving multi-cloth benchmark. All policies receive identical
// positions and forces. The baseline SAP simulator supplies independent
// physical states; mismatches abort rather than generate a favorable chart.
#include "ccp/research_multicloth.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
namespace {
using ccp::research::MultiCloth;
using ccp::research::Policy;
using ccp::research::Vec3;
struct Aggregate {
 double broad{},precise{},step{},pairs{},inter{},sphere{},builds{},reuses{},rejected{},tests{};
 double maxError{};
 std::size_t frames{};
 void add(const ccp::research::Metrics& m,double error){
  broad+=m.broadMs;precise+=m.narrowMs;step+=m.stepMs;
  pairs+=double(m.pairs);inter+=double(m.interSheetContacts);
  sphere+=double(m.sphereContacts);
  builds+=double(m.regionalPairRebuilds);
  reuses+=double(m.regionalPairReuses);
  rejected+=double(m.cachedPairsRejected);
  tests+=double(m.preciseTests);
  maxError=std::max(maxError,error);
  ++frames;
 }
};
double maxDistance(const std::vector<Vec3>& a,const std::vector<Vec3>& b){
 if(a.size()!=b.size())throw std::runtime_error("mismatched physics topology");
 double maximum=0;
 for(std::size_t i=0;i<a.size();++i){
  const auto dx=a[i].x-b[i].x,dy=a[i].y-b[i].y,dz=a[i].z-b[i].z;
  maximum=std::max(maximum,std::sqrt(dx*dx+dy*dy+dz*dz));
 }
 return maximum;
}
const char* name(int method){
 switch(method){
  case 0:return "sap_rebuild";
  case 1:return "global_guarded";
  case 2:return "region_fixed";
  case 3:return "region_adaptive";
  default:return "invalid";
 }
}
}
int main(int argc,char** argv){
 try{
  if(argc<2||argc>3)throw std::runtime_error("usage: ccp_multicloth_benchmark CSV [frames=48]");
  const int frames=argc==3?std::stoi(argv[2]):48;
  if(frames<12||frames>600)throw std::runtime_error("frame range 12..600");
  std::filesystem::path file(argv[1]);
  if(!file.parent_path().empty())std::filesystem::create_directories(file.parent_path());
  std::ofstream csv(file);
  if(!csv)throw std::runtime_error("could not create multicloth CSV");
  csv<<std::fixed<<std::setprecision(9);
  csv<<"sheets,balls,policy,frames,meanBroadMs,meanNarrowMs,meanStepMs,"
        "meanCandidates,meanInterContacts,meanSphereContacts,meanRegionBuilds,"
        "meanRegionReuses,meanRejected,meanPreciseTests,maxPositionError\n";
  constexpr int warmup=8;
  for(int sheets=1;sheets<=3;++sheets){
   MultiCloth reference(sheets,2,Policy::RebuildSAP);
   reference.setAuditing(true);
   std::vector<std::vector<Vec3>> states;
   std::vector<ccp::research::Metrics> baselineStats;
   states.reserve(frames);baselineStats.reserve(frames);
   for(int frame=0;frame<frames;++frame){
    reference.step();
    states.push_back(reference.positions());
    baselineStats.push_back(reference.metrics());
   }
   for(int method=0;method<4;++method){
    Aggregate result;
    MultiCloth sim(sheets,2,method==0?Policy::RebuildSAP:
                   method==1?Policy::GlobalGuard:Policy::RegionalCertified);
    sim.setAdaptive(method==3);
    sim.setAuditing(true);
    for(int frame=0;frame<frames;++frame){
     if(method>0)sim.step();
     const auto& positions=method==0?states[frame]:sim.positions();
     const double error=maxDistance(positions,states[frame]);
     if(error>=1e-7){
      throw std::runtime_error(std::string(name(method))+
            " changed solver positions at cloth count "+std::to_string(sheets)+
            " frame "+std::to_string(frame)+" by "+std::to_string(error));
     }
     if(frame>=warmup)
      result.add(method==0?baselineStats[frame]:sim.metrics(),error);
    }
    if(result.frames==0)throw std::runtime_error("no benchmark frames");
    const double n=double(result.frames);
    csv<<sheets<<','<<2<<','<<name(method)<<','<<result.frames<<','
       <<result.broad/n<<','<<result.precise/n<<','<<result.step/n<<','
       <<result.pairs/n<<','<<result.inter/n<<','<<result.sphere/n<<','
       <<result.builds/n<<','<<result.reuses/n<<','<<result.rejected/n<<','
       <<result.tests/n<<','<<result.maxError<<'\n';
    std::cout<<"SHEETS "<<sheets<<" METHOD "<<name(method)
             <<" STEP_MS "<<result.step/n<<" BROAD_MS "<<result.broad/n
             <<" EXACT_PAIR_CANDIDATES "<<result.pairs/n
             <<" CROSS_SHEET_RESPONSES "<<result.inter/n
             <<" MAX_SOLVER_ERROR "<<result.maxError<<'\n';
   }
  }
  csv.flush();
  if(!csv)throw std::runtime_error("failed to write CSV");
  std::cout<<"All multicloth benchmark policies passed paired state equivalence.\n";
 }catch(const std::exception& e){
  std::cerr<<"MULTICLOTH BENCHMARK FAILED: "<<e.what()<<'\n';
  return 1;
 }
}
