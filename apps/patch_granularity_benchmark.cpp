// Real multi-cloth patch-size ablation. Calibration and evaluation periods
// are disjoint. No physical solver configuration is changed by patch policy.
#include "ccp/research_multicloth.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {
using ccp::research::MultiCloth;
using ccp::research::Policy;
using ccp::research::Vec3;

struct Stats {
 double step{},broad{},narrow{},pairs{},skipped{},builds{},reuses{},contacts{},error{};
 int count{};
 void add(const ccp::research::Metrics& m,double maximumError) {
  step+=m.stepMs;broad+=m.broadMs;narrow+=m.narrowMs;
  pairs+=double(m.pairs);skipped+=double(m.regionPairsSkipped);
  builds+=double(m.regionalPairRebuilds);reuses+=double(m.regionalPairReuses);
  contacts+=double(m.interSheetContacts);
  error=std::max(error,maximumError);++count;
 }
};

double maxError(const std::vector<Vec3>& a,const std::vector<Vec3>& b) {
 if(a.size()!=b.size())throw std::runtime_error("particle topology changed");
 double worst=0;
 for(std::size_t i=0;i<a.size();++i){
  const double x=a[i].x-b[i].x,y=a[i].y-b[i].y,z=a[i].z-b[i].z;
  worst=std::max(worst,std::sqrt(x*x+y*y+z*z));
 }
 return worst;
}

void save(std::ofstream& out,int span,const char* frontier,const char* phase,
          const Stats& x) {
 if(x.count<=0)throw std::runtime_error("no sampled frames");
 const double n=double(x.count);
 out<<span<<','<<frontier<<','<<phase<<','<<x.count<<','
    <<x.step/n<<','<<x.broad/n<<','<<x.narrow/n<<','
    <<x.pairs/n<<','<<x.skipped/n<<','<<x.builds/n<<','
    <<x.reuses/n<<','<<x.contacts/n<<','<<x.error<<'\n';
}
}

int main(int argc,char** argv){
 try{
  if(argc<2||argc>3)throw std::runtime_error("usage: ccp_patch_sweep FILE.csv [frames]");
  const int frames=argc==3?std::stoi(argv[2]):48;
  if(frames<40||frames>160)throw std::invalid_argument("frames must be 40..160");
  const int warmup=std::max(8,frames/6);
  const int split=warmup+(frames-warmup)/2;
  std::filesystem::path path(argv[1]);
  if(!path.parent_path().empty())std::filesystem::create_directories(path.parent_path());
  std::ofstream out(path);
  if(!out)throw std::runtime_error("cannot create patch profile");
  out<<std::fixed<<std::setprecision(9);
  out<<"patchSpan,frontier,phase,frames,meanStepMs,meanBroadMs,meanNarrowMs,"
        "meanCandidates,meanSkippedPatchPairs,meanRegionBuilds,meanRegionReuses,"
        "meanInterContacts,maxPositionError\n";

  MultiCloth sap(3,2,Policy::RebuildSAP);
  sap.setAuditing(true);
  std::vector<std::vector<Vec3>> reference;
  Stats sapCalibration,sapEvaluation;
  for(int frame=0;frame<frames;++frame){
   sap.step();
   reference.push_back(sap.positions());
   if(frame>=warmup)(frame<split?sapCalibration:sapEvaluation).add(sap.metrics(),0);
  }
  save(out,0,"sap_rebuild","calibration",sapCalibration);
  save(out,0,"sap_rebuild","evaluation",sapEvaluation);

  const std::array<int,6> spans={1,2,3,4,6,12};
  for(bool hierarchical:{false,true}){
   for(int span:spans){
    MultiCloth physics(3,2,Policy::RegionalCertified);
    physics.setAuditing(true);
    physics.setAdaptive(false); // isolate region size, not guard heuristics
    physics.setHierarchicalRegions(hierarchical);
    physics.setPatchCellSpan(span);
    Stats calibration,evaluation;
    for(int frame=0;frame<frames;++frame){
     physics.step();
     const double error=maxError(reference[frame],physics.positions());
     if(error>=1e-8)throw std::runtime_error(
       "patch search changed physics: span="+std::to_string(span)+
       " frame="+std::to_string(frame));
     if(frame>=warmup)
      (frame<split?calibration:evaluation).add(physics.metrics(),error);
    }
    const char* method=hierarchical?"two_tier":"all_region_pairs";
    save(out,span,method,"calibration",calibration);
    save(out,span,method,"evaluation",evaluation);
    std::cout<<method<<" PATCH "<<span
             <<" CALIBRATION_STEP_MS "<<calibration.step/calibration.count
             <<" EVALUATION_STEP_MS "<<evaluation.step/evaluation.count
             <<" SKIPPED_PAIRS "<<evaluation.skipped/evaluation.count
             <<" POSITION_ERROR "<<evaluation.error<<'\n';
   }
  }
  out.flush();
  if(!out)throw std::runtime_error("patch profile write failed");
  std::cout<<"All 12 fixed-granularity configurations matched SAP reference physics.\n";
 }catch(const std::exception& e){
  std::cerr<<"PATCH PROFILE FAILED: "<<e.what()<<'\n';
  return 1;
 }
}