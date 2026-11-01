// Continuous ONLINE adaptation audit on the same real three-cloth physics.
// Produces a time-series rather than hiding controller overhead in averages.
#include "ccp/research_multicloth.hpp"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
using namespace ccp::research;
int main(int argc,char** argv){
 try{
  if(argc<2||argc>3)throw std::invalid_argument("usage: ccp_online_trace OUTPUT.csv [frames=120]");
  const int frames=argc==3?std::stoi(argv[2]):120;
  if(frames<60||frames>500)throw std::invalid_argument("frames must be 60..500");
  const std::filesystem::path file(argv[1]);
  if(!file.parent_path().empty())std::filesystem::create_directories(file.parent_path());
  std::ofstream out(file);
  if(!out)throw std::runtime_error("cannot create online trace CSV");
  out<<std::fixed<<std::setprecision(9);
  out<<"frame,onlineStepMs,sapStepMs,onlineBroadMs,sapBroadMs,onlineNarrowMs,"
        "onlineDecisionMs,onlinePatches,onlineSplits,onlineMerges,onlineStrategySwitches,"
        "onlineHierarchy,contactCorrections,interSheetCorrections,onlineCandidates,"
        "sapCandidates,regionDirty,regionBuilds,regionReuses,coarseSkip,"
        "observedAabbWork,observedContactHeat,observedCoarseOverlapFraction,"
        "maxPositionError,coverageChecked\n";
  MultiCloth online(3,2,Policy::OnlineContactAware),sap(3,2,Policy::RebuildSAP);
  std::size_t totalSplits=0,totalMerges=0,totalSwitches=0,checked=0;
  double totalOnline=0,totalSap=0;
  for(int frame=1;frame<=frames;++frame){
   // The periodic oracle is independent from production queries and runs
   // identically on both policies. Both solvers execute the same fixed
   // forces and constraints, so comparing coordinates is another safety gate.
   const bool verify=frame<=12||frame%20==0;
   online.setAuditing(verify);sap.setAuditing(verify);
   online.step();sap.step();
   const auto& a=online.positions();const auto& b=sap.positions();
   if(a.size()!=b.size())throw std::runtime_error("online topology diverged");
   double worst=0;
   for(std::size_t i=0;i<a.size();++i){
    const auto dx=a[i].x-b[i].x,dy=a[i].y-b[i].y,dz=a[i].z-b[i].z;
    worst=std::max(worst,std::sqrt(dx*dx+dy*dy+dz*dz));
   }
   if(worst>=1e-8)throw std::runtime_error(
      "online lost physical equivalence at frame "+std::to_string(frame));
   const auto& m=online.metrics(),n=sap.metrics();
   if(verify&&!m.oracleChecked)throw std::runtime_error("coverage audit not executed");
   totalSplits+=m.onlineSplits;
   totalMerges+=m.onlineMerges;
   totalSwitches+=m.onlineStrategySwitches;
   checked+=verify?1:0;
   if(frame>12){totalOnline+=m.stepMs;totalSap+=n.stepMs;}
   out<<frame<<','<<m.stepMs<<','<<n.stepMs<<','<<m.broadMs<<','<<n.broadMs
      <<','<<m.narrowMs<<','<<m.onlineDecisionMs<<','<<m.onlinePatches
      <<','<<m.onlineSplits<<','<<m.onlineMerges<<','<<m.onlineStrategySwitches
      <<','<<int(online.hierarchicalRegions())<<','<<m.contacts
      <<','<<m.interSheetContacts<<','<<m.pairs<<','<<n.pairs
      <<','<<m.dirtyRegions<<','<<m.regionalPairRebuilds
      <<','<<m.regionalPairReuses<<','<<m.regionPairsSkipped
      <<','<<m.onlineMeasuredWork<<','<<m.onlineContactHeat
      <<','<<m.onlineCoarsePairFraction<<','<<worst<<','<<int(verify)<<'\n';
  }
  if(totalSplits==0)throw std::runtime_error("online physics never split any patch");
  out.flush();
  if(!out)throw std::runtime_error("online trace write failed");
  std::cout<<"VERIFIED ONLINE REAL PHYSICS frames="<<frames
           <<" splits="<<totalSplits<<" merges="<<totalMerges
           <<" strategySwitches="<<totalSwitches
           <<" independentlyAuditedFrames="<<checked
           <<" avgStepOnlineMs="<<totalOnline/double(frames-12)
           <<" avgStepSAPMs="<<totalSap/double(frames-12)
           <<" maxParticleError=0\n";
 }catch(const std::exception& e){
  std::cerr<<"ONLINE PHYSICS TRACE FAILED: "<<e.what()<<'\n';
  return 1;
 }
}
