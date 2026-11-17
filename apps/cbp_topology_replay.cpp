// Phase 5: topology-authentic equal-input broad-phase REPLAY from the
// existing coupled XPBD-style multi-cloth generator.
//
// The physics is advanced exactly once by RebuildSAP. Each resulting
// snapshot uses REAL triangle sheet/mesh-cell patch IDs from MultiCloth.
// Seven independent broad-phase objects process identical padded bounds.
// We time ONLY broad-phase calls; solver generation, padding, independent
// oracle and CSV serialization are outside timed regions. This is NOT yet
// end-to-end solver acceleration or continuous collision detection.
#include "ccp/research_multicloth.hpp"
#include "ccp/cbp_portfolio.hpp"
#include "ccp/cbp_cost_manager.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using ccp::live::Bounds;
using ccp::live::Pair;
using Pairs=std::vector<Pair>;
using ccp::cbp::CausalCostRouter;
using ccp::cbp::CbpPortfolio;
using ccp::cbp::PortfolioMode;
using ccp::cbp::CrossMode;
using ccp::cbp::RegionRouter;
using Clock=std::chrono::steady_clock;
constexpr std::array<const char*,7> methods{
 "fresh_sap","frozen_v4","topology_exhaustive","topology_shared_bvh",
 "topology_certified_cache","causal_cbp","portfolio_v4_regional"};
constexpr double kQueryHalfMargin=.0375; // Corresponds to multicloth.cpp kMargin/2.
int boundedInt(const char* value,int minValue,int maxValue){
 const std::string text=value;
 std::size_t consumed=0;
 const auto out=std::stoll(text,&consumed);
 if(consumed!=text.size()||out<minValue||out>maxValue)
  throw std::invalid_argument("integer argument outside supported range");
 return static_cast<int>(out);
}
void pad(Bounds& b){
 b.min.x-=kQueryHalfMargin;b.min.y-=kQueryHalfMargin;
 b.min.z-=kQueryHalfMargin;b.max.x+=kQueryHalfMargin;
 b.max.y+=kQueryHalfMargin;b.max.z+=kQueryHalfMargin;
 if(!ccp::cbp::valid(b))
  throw std::runtime_error("non-finite or inverted padded bound");
}
Pairs independentBrute(const std::vector<Bounds>& boxes){
 Pairs result;
 for(std::size_t i=0;i<boxes.size();++i)
  for(std::size_t j=i+1;j<boxes.size();++j)
   if(ccp::cbp::compatible(boxes[i],boxes[j])&&
      ccp::cbp::overlap(boxes[i],boxes[j]))
    result.push_back({boxes[i].id,boxes[j].id});
 return result;
}
std::uint64_t snapshotHash(const std::vector<Bounds>& boxes){
 // Deterministic fingerprint of the *entire current query*, independent of
 // mesh grouping. Compare across span sweeps to detect altered trajectories.
 // Not a cryptographic hash; exact pair-list oracle remains independent.
 std::uint64_t value=14695981039346656037ULL;
 const auto mix=[&](std::uint64_t word){
  value^=word;
  value*=1099511628211ULL;
 };
 for(const auto& box:boxes){
  mix(static_cast<std::uint32_t>(box.id));
  for(double coordinate:{box.min.x,box.min.y,box.min.z,
                          box.max.x,box.max.y,box.max.z})
   mix(std::bit_cast<std::uint64_t>(coordinate));
  mix(box.category);
  mix(box.mask);
  mix(static_cast<std::uint64_t>(box.movable));
 }
 return value;
}
double ms(Clock::time_point a,Clock::time_point b){
 return std::chrono::duration<double,std::milli>(b-a).count();
}
void validateTopology(const ccp::research::MultiCloth& physics,
                      const std::vector<ccp::research::Primitive>& primitives,
                      int sheetCount,int sphereCount,int patchSpan){
 const auto& triangles=physics.triangles();
 const auto& sheets=physics.triangleSheets();
 if(primitives.size()!=triangles.size()+static_cast<std::size_t>(sphereCount))
  throw std::runtime_error("primitive count differs from mesh and spheres");
 if(sheets.size()!=triangles.size())
  throw std::runtime_error("triangle-to-sheet array mismatch");
 // Verify actual mesh triangles and that cell/patch assignment is geometric
 // TOPOLOGY-based, not a grouping of primitive IDs into arbitrary spans.
 constexpr int cells=12;
 constexpr int trianglesPerSheet=2*cells*cells;
 const int blocks=cells/patchSpan;
 for(std::size_t i=0;i<triangles.size();++i){
  const int sheet=sheets[i];
  if(sheet<0||sheet>=sheetCount||primitives[i].sheet!=sheet)
   throw std::runtime_error("triangle sheet topology mismatch");
  const auto t=triangles[i];
  for(int vertex:t)
   if(vertex<sheet*169||vertex>=(sheet+1)*169)
    throw std::runtime_error("triangle vertex belongs to another sheet");
  const int cell=(static_cast<int>(i)%trianglesPerSheet)/2;
  const int x=cell%cells,y=cell/cells;
  const int expected=patchSpan==3?
       sheet*100+(y/3)*4+x/3:
       1000+sheet*400+(y/patchSpan)*blocks+x/patchSpan;
  if(primitives[i].region!=expected)
   throw std::runtime_error("region not derived from mesh cell topology");
 }
 for(std::size_t i=triangles.size();i<primitives.size();++i)
  if(primitives[i].sheet!=-1||
     primitives[i].region!=sheetCount*100+static_cast<int>(i-triangles.size()))
   throw std::runtime_error("sphere region or identity mismatch");
}
}
int main(int argc,char** argv){
 try{
  if(argc!=6)
   throw std::invalid_argument("Usage: ccp_cbp_topology_replay OUTPUT.csv SHEETS(1..3) SPHERES(1..3) FRAMES(12..120) PATCH_SPAN(1,2,3,4,6,12)");
  const std::filesystem::path destination(argv[1]);
  const int sheets=boundedInt(argv[2],1,3);
  const int spheres=boundedInt(argv[3],1,3);
  const int frames=boundedInt(argv[4],12,120);
  const int span=boundedInt(argv[5],1,12);
  if(12%span!=0)
   throw std::invalid_argument("patch span must divide 12");
  if(std::filesystem::exists(destination))
   throw std::runtime_error("refusing to overwrite existing immutable topology evidence");
  if(!destination.parent_path().empty())
   std::filesystem::create_directories(destination.parent_path());

  ccp::research::MultiCloth source(sheets,spheres,ccp::research::Policy::RebuildSAP);
  source.setPatchCellSpan(span);
  source.setAuditing(false);
  ccp::final_broadphase::GuardSAPBoundedTrial frozen;
  std::array<RegionRouter,3> regional;
  regional[0].setCrossMode(CrossMode::Exhaustive);
  regional[1].setCrossMode(CrossMode::SharedBVH);
  regional[2].setCrossMode(CrossMode::CertifiedCache);
  CausalCostRouter manager;
  CbpPortfolio portfolio;
  portfolio.setTrialKernel(CrossMode::Exhaustive);
  for(auto& r:regional)r.setSapThreshold(48);
  manager.setSapThreshold(48);
  portfolio.setSapThreshold(48);

  std::ofstream out(destination);
  if(!out)throw std::runtime_error("failed to open topology CSV output");
  out<<"frame,snapshot_hash,sheets,spheres,triangles,primitives,patch_span,regions,"
        "method,query_ms,pairs,oracle_equal,v4_action,"
        "causal_choice,causal_density,causal_motion,"
        "portfolio_choice,portfolio_trial,portfolio_accepted,"
        "portfolio_rejected,portfolio_switch,portfolio_v4_action,"
        "local_tests,cross_tests,cache_dirty,cache_potential,"
        "physics_source_step_ms\n";
  out<<std::fixed<<std::setprecision(9);

  std::size_t totalPairs=0;
  for(int frame=0;frame<frames;++frame){
   source.step();
   const auto primitive=source.currentPrimitives();
   validateTopology(source,primitive,sheets,spheres,span);
   std::vector<Bounds> boxes;
   std::vector<int> labels;
   boxes.reserve(primitive.size());labels.reserve(primitive.size());
   for(std::size_t i=0;i<primitive.size();++i){
    if(primitive[i].box.id!=static_cast<int>(i))
     throw std::runtime_error("primitive ID not stable and index aligned");
    auto box=primitive[i].box;
    pad(box);
    boxes.push_back(box);
    labels.push_back(primitive[i].region);
   }
   std::map<int,int> regionOccupancy;
   for(int label:labels)++regionOccupancy[label];
   const auto geometryFingerprint=snapshotHash(boxes);
   std::array<Pairs,7> results;
   std::array<double,7> times{};
   std::array<ccp::cbp::Telemetry,7> work{};
   const char* v4Action="none";
   for(int offset=0;offset<7;++offset){
    const int method=(frame+offset)%7; // varied external evaluation order
    const auto start=Clock::now();
    switch(method){
     case 0:results[0]=ccp::live::candidates(boxes,true);break;
     case 1:{
      auto outcome=frozen.query(boxes);
      results[1]=std::move(outcome.pairs);
      v4Action=outcome.action;
      break;
     }
     case 2:case 3:case 4:{
      auto& router=regional[static_cast<std::size_t>(method-2)];
      results[static_cast<std::size_t>(method)]=router.query(boxes,labels);
      work[static_cast<std::size_t>(method)]=router.telemetry();
      break;
     }
     case 5:
      results[5]=manager.query(boxes,labels);
      work[5]=manager.backendWork();
      break;
     case 6:
      results[6]=portfolio.query(boxes,labels);
      work[6]=portfolio.regionalWork();
      break;
     default:throw std::logic_error("invalid method index");
    }
    times[static_cast<std::size_t>(method)]=ms(start,Clock::now());
   }
   for(int method=1;method<7;++method){
    if(results[static_cast<std::size_t>(method)]!=results[0])
     throw std::runtime_error("candidate parity failure at frame "+
       std::to_string(frame+1)+" method "+methods[static_cast<std::size_t>(method)]);
   }
   // Strong brute-force independence for first, last and periodic snapshots.
   if(frame==0||frame==frames-1||frame%13==0){
    if(independentBrute(boxes)!=results[0])
     throw std::runtime_error("fresh SAP disagrees with independent brute-force");
   }
   totalPairs+=results[0].size();
   const auto d=manager.decision();
   const auto p=portfolio.decision();
   for(int method=0;method<7;++method){
    const auto index=static_cast<std::size_t>(method);
    const auto w=work[index];
    out<<(frame+1)<<','<<std::hex<<geometryFingerprint<<std::dec
       <<','<<sheets<<','<<spheres<<','<<source.triangles().size()
       <<','<<boxes.size()<<','<<span<<','<<regionOccupancy.size()
       <<','<<methods[index]<<','<<times[index]<<','<<results[index].size()
       <<",1,"<<(method==1?v4Action:"none")
       <<','<<(method==5?CausalCostRouter::name(d.chosen):"none")
       <<','<<(method==5?d.sampledDensity:0.)
       <<','<<(method==5?d.sampledMotion:0.)
       <<','<<(method==6?CbpPortfolio::name(p.mode):"none")
       <<','<<(method==6&&p.trialFrame?1:0)
       <<','<<(method==6&&p.trialAccepted?1:0)
       <<','<<(method==6&&p.trialRejected?1:0)
       <<','<<(method==6&&p.switched?1:0)
       <<','<<(method==6?p.v4Action:"none")
       <<','<<w.localTests<<','<<w.crossTests
       <<','<<w.cacheDirtyPrimitives<<','<<w.cachePotentialPairs
       <<','<<source.metrics().stepMs<<'\n';
   }
   if((frame+1)%std::max(1,frames/6)==0)
    std::cerr<<"[CBP topology] frame "<<frame+1<<"/"<<frames
             <<" regions "<<regionOccupancy.size()
             <<" current candidate pairs "<<results[0].size()<<"\n";
  }
  out.flush();
  if(!out)throw std::runtime_error("failed to flush topology replay CSV");
  std::cerr<<"[CBP topology PASS] sheets="<<sheets<<" spheres="<<spheres
           <<" span="<<span<<" frames="<<frames
           <<" total candidate-pair occurrences="<<totalPairs
           <<" exact parity among all 7 methods; topology IDs verified\n";
 }catch(const std::exception& e){
  std::cerr<<"CBP TOPOLOGY REPLAY FAILED: "<<e.what()<<'\n';
  return 1;
 }
}
