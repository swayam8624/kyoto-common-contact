// Kyoto CBP Phase 7: CPU methods on a single immutable mixed-material
// KYOTOM01 trajectory that is ALSO consumed unchanged by Apple Metal.
//
// Equality is FULL stable-ID sorted candidate-pair parity on EVERY frame.
// Regions are actual material-instance ownership, not arbitrary ID blocks.
// World generation happens separately; no physics or IO is inside timed calls.
// Full mode includes costly certificate/causal controllers. For N>16k use
// "core" first to avoid unbounded potential-graph memory and long trials.
#include "ccp/cbp_region_router.hpp"
#include "ccp/cbp_cost_manager.hpp"
#include "ccp/cbp_portfolio.hpp"
#include "ccp/final_broadphase.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <numeric>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
namespace fs=std::filesystem;
using ccp::cbp::Bounds;
using ccp::cbp::Pairs;
using ccp::cbp::RegionRouter;
using ccp::cbp::CrossMode;
using ccp::cbp::CausalCostRouter;
using ccp::cbp::CbpPortfolio;
using Clock=std::chrono::steady_clock;
constexpr std::array<const char*,8> modes={
 "sap_rebuild","frozen_v4","instance_exhaustive","instance_xsweep",
 "instance_shared_bvh","instance_certified_cache","causal_cbp",
 "portfolio_v4_regional"
};
struct Replay {
 std::ifstream file;
 std::uint32_t n{},frames{},seed{},material{};
 explicit Replay(const fs::path& path):file(path,std::ios::binary){
  if(!file)throw std::runtime_error("Cannot open KYOTOM01 binary");
  char magic[8]{};
  file.read(magic,8);
  if(!file||std::string(magic,8)!="KYOTOM01")
   throw std::runtime_error("Invalid KYOTOM01 magic");
  for(auto* v:{&n,&frames,&seed,&material})
   file.read(reinterpret_cast<char*>(v),4);
  if(!file||n==0||n>65536||frames<12||frames>240||material!=5)
   throw std::runtime_error("Unsupported mixed-material KYOTOM01 metadata");
  const auto start=file.tellg();
  file.seekg(0,std::ios::end);
  if(file.tellg()-start!=std::streamoff(n)*std::streamoff(frames)*24)
   throw std::runtime_error("Incorrect recorded frame byte count");
  file.seekg(start);
 }
 std::vector<Bounds> next(){
  std::vector<Bounds> boxes;
  boxes.reserve(n);
  for(std::uint32_t i=0;i<n;++i){
   std::array<float,6> v{};
   file.read(reinterpret_cast<char*>(v.data()),24);
   if(!file)throw std::runtime_error("Truncated mixed-material frame");
   for(float f:v)if(!std::isfinite(f))
    throw std::runtime_error("Invalid non-finite material AABB");
   if(v[0]>v[3]||v[1]>v[4]||v[2]>v[5])
    throw std::runtime_error("Inverted material AABB");
   boxes.push_back({static_cast<int>(i),{v[0],v[1],v[2]},
                    {v[3],v[4],v[5]},1U,1U,true});
  }
  return boxes;
 }
};
std::vector<int> ownership(const fs::path& file,std::uint32_t n){
 std::ifstream in(file);
 if(!in)throw std::runtime_error("Cannot open material instance ownership CSV");
 std::string line;
 if(!std::getline(in,line)||
    line!="id,kind,regime,first_primitive,primitive_count")
  throw std::runtime_error("Invalid material instance ownership schema");
 std::vector<int> labels(n,-1);
 int next=0,instances=0;
 while(std::getline(in,line)){
  if(line.empty())continue;
  std::stringstream stream(line);
  std::array<std::string,5> fields;
  for(std::size_t i=0;i<fields.size();++i)
   if(!std::getline(stream,fields[i],','))
    throw std::runtime_error("Missing instance field");
  if(std::getline(stream,line,','))
   throw std::runtime_error("Extra instance field");
  const auto id=std::stoi(fields[0]);
  const auto first=std::stoi(fields[3]),count=std::stoi(fields[4]);
  if(id!=instances||first!=next||count<=0||
     count>static_cast<int>(n)-first)
   throw std::runtime_error("Noncontiguous or invalid instance ownership");
  if(fields[1].empty()||fields[2].empty())
   throw std::runtime_error("Missing material type or deformation regime");
  for(int i=0;i<count;++i)labels[static_cast<std::size_t>(first+i)]=id;
  next+=count;++instances;
 }
 if(next!=static_cast<int>(n)||instances<12||instances>1000)
  throw std::runtime_error("Instance ownership does not cover all primitives");
 return labels;
}
std::uint64_t pairHash(const Pairs& pairs){
 std::uint64_t hash=14695981039346656037ULL;
 for(const auto& p:pairs)
  for(std::uint32_t value:{static_cast<std::uint32_t>(p.a),
                           static_cast<std::uint32_t>(p.b)}){
   hash^=value;hash*=1099511628211ULL;
  }
 return hash;
}
double ms(Clock::time_point begin,Clock::time_point end){
 return std::chrono::duration<double,std::milli>(end-begin).count();
}
}
int main(int argc,char** argv){
 try{
  if(argc<4||argc>5)
   throw std::invalid_argument(
    "Usage: ccp_cbp_mass_replay SNAPSHOT.kmcb instances.csv OUT.csv [core|full]");
  const bool full=argc==5&&std::string(argv[4])=="full";
  if(argc==5&&std::string(argv[4])!="full"&&std::string(argv[4])!="core")
   throw std::invalid_argument("Last argument must be core or full");
  Replay input(argv[1]);
  if(full && input.n>16000)
   throw std::invalid_argument(
    "Full graph/cache benchmark is capped at 16000 primitives; use core "
    "at large scale to protect the memory/output budget");
  const auto labels=ownership(argv[2],input.n);
  const int instances=*std::max_element(labels.begin(),labels.end())+1;
  const fs::path destination(argv[3]);
  if(fs::exists(destination)||fs::exists(destination.string()+".part"))
   throw std::runtime_error("Existing benchmark evidence will not be overwritten");
  if(!destination.parent_path().empty())
   fs::create_directories(destination.parent_path());
  const fs::path temp=destination.string()+".part";
  std::ofstream csv(temp);
  if(!csv)throw std::runtime_error("Cannot create benchmark CSV");
  csv<<"frame,mode,instances,primitives,ms,pairs,pair_hash,"
        "exact_sap_equal,oracle_checked,region_pair_tests,regions,"
        "cross_tests,cache_dirty,cache_potential,portfolio_mode,"
        "portfolio_trial,portfolio_accepted,portfolio_rejected,"
        "v4_action\n";
  csv<<std::fixed<<std::setprecision(8);
  std::array<RegionRouter,4> regional;
  regional[0].setCrossMode(CrossMode::Exhaustive);
  regional[1].setCrossMode(CrossMode::Exhaustive);
  regional[1].setRegionSweep(true);
  regional[2].setCrossMode(CrossMode::SharedBVH);
  regional[3].setCrossMode(CrossMode::CertifiedCache);
  for(auto& mode:regional)mode.setSapThreshold(48);
  ccp::final_broadphase::GuardSAPBoundedTrial v4;
  CausalCostRouter causal;
  CbpPortfolio portfolio;
  portfolio.setTrialKernel(CrossMode::Exhaustive);
  portfolio.setSapThreshold(48);
  causal.setSapThreshold(48);
  std::vector<int> active{0,1,2,3,4,7};
  if(full)active={0,1,2,3,4,5,6,7};
  for(std::uint32_t frame=0;frame<input.frames;++frame){
   const auto boxes=input.next();
   std::array<Pairs,8> output;
   std::array<double,8> elapsed{};
   std::array<ccp::cbp::Telemetry,8> work{};
   std::string v4Action="none";
   for(std::size_t index=0;index<active.size();++index){
    const int mode=active[(index+frame)%active.size()];
    const auto start=Clock::now();
    switch(mode){
     case 0:output[0]=ccp::live::candidates(boxes,true);break;
     case 1:{
      auto value=v4.query(boxes);
      v4Action=value.action;
      output[1]=std::move(value.pairs);
      break;
     }
     case 2:case 3:case 4:case 5:{
      auto& impl=regional[static_cast<std::size_t>(mode-2)];
      output[static_cast<std::size_t>(mode)]=impl.query(boxes,labels);
      work[static_cast<std::size_t>(mode)]=impl.telemetry();
      break;
     }
     case 6:
      output[6]=causal.query(boxes,labels);
      work[6]=causal.backendWork();
      break;
     case 7:
      output[7]=portfolio.query(boxes,labels);
      work[7]=portfolio.regionalWork();
      break;
     default:throw std::logic_error("Invalid mass replay method");
    }
    elapsed[static_cast<std::size_t>(mode)]=ms(start,Clock::now());
   }
   const auto& oracle=output[0];
   if(oracle.size()>16000000)
    throw std::runtime_error("Refusing >16-million candidate pairs in stress case");
   const bool audit=input.n<=512 && frame%7==0;
   if(audit && ccp::live::candidates(boxes,false)!=oracle)
    throw std::runtime_error("Independent O(N^2) oracle mismatch");
   for(const auto mode:active){
    if(output[static_cast<std::size_t>(mode)]!=oracle)
     throw std::runtime_error("Full pair parity FAILURE at frame "+
      std::to_string(frame)+" mode "+modes[static_cast<std::size_t>(mode)]);
    const auto& w=work[static_cast<std::size_t>(mode)];
    const auto pd=portfolio.decision();
    csv<<frame<<','<<modes[static_cast<std::size_t>(mode)]<<','
       <<instances<<','<<input.n<<','
       <<elapsed[static_cast<std::size_t>(mode)]<<','<<oracle.size()
       <<','<<pairHash(oracle)<<",1,"<<(audit?1:0)<<','
       <<w.regionPairTests<<','<<w.regions<<','<<w.crossTests<<','
       <<w.cacheDirtyPrimitives<<','<<w.cachePotentialPairs<<','
       <<(mode==7?CbpPortfolio::name(pd.mode):"none")<<','
       <<(mode==7&&pd.trialFrame?1:0)<<','
       <<(mode==7&&pd.trialAccepted?1:0)<<','
       <<(mode==7&&pd.trialRejected?1:0)<<','
       <<(mode==1?v4Action:mode==7?pd.v4Action:"none")<<'\n';
   }
   if((frame+1)%std::max(1U,input.frames/12)==0)
    std::cerr<<"[CBP MASS CPU] "<<frame+1<<"/"<<input.frames
             <<" materials "<<instances<<" AABBs "<<input.n
             <<" exact candidate pairs "<<oracle.size()<<"\n";
  }
  csv.flush();
  if(!csv)throw std::runtime_error("Incomplete CPU stress CSV");
  csv.close();
  fs::rename(temp,destination);
  std::cout<<"PASS CPU many-material replay: instances="<<instances
           <<" primitives="<<input.n<<" frames="<<input.frames
           <<" modes="<<active.size()
           <<"; full stable-ID candidate parity on EVERY frame\n";
 }catch(const std::exception& e){
  std::cerr<<"CBP MASS CPU BENCHMARK FAILED: "<<e.what()<<'\n';return 1;
 }
}
