// CBP Phase 3: honest SAME-INPUT replay on existing KYOTOM01 deformable
// material snapshots. Compares frozen V4, immutable fresh SAP oracle, and
// three fixed exact regional modes against the causal manager.
//
// ALL 6 policies own independent state. Their query calls are timed in
// rotating order, not queried by the manager. Snapshot reading, CSV writes,
// and independent correctness oracles are outside timed query boundaries.
// The AABBs are already exporter-padded; do not pad input again.
//
// WARNING: The .kmcb ABI only stores bounds, not mesh adjacency or actual
// topology patch labels. The ID-span grouping below is an EXPLICIT synthetic
// partition control, NOT a topology-correct cloth patch assignment.
// These are material-generator trajectories, not end-to-end solver timings.
#include "ccp/cbp_cost_manager.hpp"
#include "ccp/final_broadphase.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using ccp::cbp::Bounds;
using ccp::cbp::Pairs;
using ccp::cbp::RegionRouter;
using ccp::cbp::CrossMode;
using ccp::cbp::CausalCostRouter;
using Clock=std::chrono::steady_clock;
static_assert(std::endian::native==std::endian::little,
              "KYOTOM01 material ABI requires little endian");
constexpr std::array<const char*,6> kMethods{
 "sap_rebuild","v4_guard_sap_bounded_trial","region_exhaustive",
 "region_shared_bvh","region_certified_cache","causal_cbp"};

int integer(const char* s,int lo,int hi){
 std::size_t end=0;
 const auto parsed=std::stoll(s,&end);
 if(end!=std::string(s).size()||parsed<lo||parsed>hi)
  throw std::invalid_argument("Integer argument out of range");
 return static_cast<int>(parsed);
}
struct Reader{
 std::ifstream file;
 std::uint32_t n{},frames{},seed{},materialCode{};
 explicit Reader(const std::string& pathname):file(pathname,std::ios::binary){
  if(!file)throw std::runtime_error("Cannot open KYOTOM01 snapshot");
  char magic[8]{};
  file.read(magic,8);
  if(!file||std::string(magic,8)!="KYOTOM01")
   throw std::runtime_error("Expected KYOTOM01 magic");
  for(auto* field:{&n,&frames,&seed,&materialCode})
   file.read(reinterpret_cast<char*>(field),sizeof(*field));
  if(!file||n<16||n>4096||frames<1||frames>10000||materialCode>5)
   throw std::runtime_error("Corrupt or out-of-scope KYOTOM01 header");
  const auto position=file.tellg();
  file.seekg(0,std::ios::end);
  const auto size=file.tellg()-position;
  const auto expected=std::streamoff(n)*std::streamoff(frames)*24;
  if(size!=expected)
   throw std::runtime_error("KYOTOM01 length mismatch");
  file.seekg(position);
 }
 std::vector<Bounds> next(){
  std::vector<Bounds> boxes;
  boxes.reserve(n);
  for(std::uint32_t i=0;i<n;++i){
   std::array<float,6> v{};
   file.read(reinterpret_cast<char*>(v.data()),sizeof(v));
   if(!file)throw std::runtime_error("Truncated KYOTOM01 frame");
   for(float x:v)
    if(!std::isfinite(x))throw std::runtime_error("Nonfinite snapshot AABB");
   if(v[0]>v[3]||v[1]>v[4]||v[2]>v[5])
    throw std::runtime_error("Inverted snapshot AABB");
   boxes.push_back({static_cast<int>(i),{v[0],v[1],v[2]},
                    {v[3],v[4],v[5]},1u,1u,true});
  }
  return boxes;
 }
};
struct Summary{
 std::vector<double> sampleMs;
 std::size_t lastPairs{},switches{},cacheFrames{},bvhFrames{},exhaustiveFrames{};
 double selectorTotalMs{},migrationTotalMs{};
};
double avg(const std::vector<double>& x){
 return std::accumulate(x.begin(),x.end(),0.)/double(x.size());
}
double p95(std::vector<double> x){
 std::sort(x.begin(),x.end());
 return x[static_cast<std::size_t>(std::ceil(.95*x.size()))-1];
}
}
int main(int argc,char** argv){
 try{
  if(argc<2||argc>5)
   throw std::invalid_argument(
     "Usage: ccp_cbp_kmcb_replay SNAPSHOT.kmcb [ID_SPAN=64] [MAX_FRAMES=96] [TRACE.csv]");
  Reader source(argv[1]);
  const int span=argc>2?integer(argv[2],1,4096):64;
  const int limit=argc>3?integer(argv[3],12,240):96;
  const int frames=std::min(int(source.frames),limit);
  if(frames<12)throw std::invalid_argument("Need at least 12 replay frames");
  const int warmup=std::min(8,frames/8);

  std::ofstream trace;
  if(argc==5){
   const std::filesystem::path path(argv[4]);
   if(std::filesystem::exists(path))
    throw std::runtime_error("Refusing to overwrite existing trace CSV");
   trace.open(path);
   if(!trace)throw std::runtime_error("Cannot create trace CSV");
   trace<<"frame,method,query_ms,exact_pairs,manager_mode,switched,selector_ms,migration_ms\n";
  }

  ccp::final_broadphase::GuardSAPBoundedTrial frozenV4;
  std::array<RegionRouter,3> fixed;
  fixed[0].setCrossMode(CrossMode::Exhaustive);
  fixed[1].setCrossMode(CrossMode::SharedBVH);
  fixed[2].setCrossMode(CrossMode::CertifiedCache);
  CausalCostRouter manager;
  for(auto& r:fixed)r.setSapThreshold(48);
  manager.setSapThreshold(48);
  std::array<Summary,6> stats{};
  const auto n=static_cast<int>(source.n);
  std::vector<int> labels(source.n);
  for(int i=0;i<n;++i)labels[static_cast<std::size_t>(i)]=i/span;

  for(int frame=0;frame<frames;++frame){
   const auto boxes=source.next();
   std::array<Pairs,6> outcomes{};
   std::array<double,6> times{};
   for(int k=0;k<6;++k){
    const int method=(frame+k)%6;
    const auto start=Clock::now();
    switch(method){
     case 0:
      outcomes[0]=ccp::live::candidates(boxes,true);
      break;
     case 1:
      outcomes[1]=frozenV4.query(boxes).pairs;
      break;
     case 2:case 3:case 4:
      outcomes[static_cast<std::size_t>(method)]=
        fixed[static_cast<std::size_t>(method-2)].query(boxes,labels);
      break;
     case 5:
      outcomes[5]=manager.query(boxes,labels);
      break;
     default:throw std::logic_error("Invalid method");
    }
    times[static_cast<std::size_t>(method)]=
      std::chrono::duration<double,std::milli>(Clock::now()-start).count();
   }
   for(int method=1;method<6;++method)
    if(outcomes[static_cast<std::size_t>(method)]!=outcomes[0])
     throw std::runtime_error("Exact pair mismatch method="+
                              std::string(kMethods[static_cast<std::size_t>(method)])+
                              " frame="+std::to_string(frame));
   // Additional independent exhaustive oracle on selected small frames.
   if(n<=256 && frame%23==0 &&
      ccp::live::candidates(boxes,false)!=outcomes[0])
    throw std::runtime_error("Fresh SAP oracle disagrees with brute-force");
   if(frame<warmup)continue;

   const auto& d=manager.decision();
   for(int method=0;method<6;++method){
    auto& s=stats[static_cast<std::size_t>(method)];
    s.sampleMs.push_back(times[static_cast<std::size_t>(method)]);
    s.lastPairs=outcomes[static_cast<std::size_t>(method)].size();
    if(method==5){
     s.switches+=static_cast<std::size_t>(d.switched);
     s.selectorTotalMs+=d.decisionMs;
     s.migrationTotalMs+=d.migrationMs;
     s.cacheFrames+=static_cast<std::size_t>(d.chosen==CrossMode::CertifiedCache);
     s.bvhFrames+=static_cast<std::size_t>(d.chosen==CrossMode::SharedBVH);
     s.exhaustiveFrames+=static_cast<std::size_t>(d.chosen==CrossMode::Exhaustive);
    }
    if(trace){
     trace<<frame<<','<<kMethods[static_cast<std::size_t>(method)]<<','
          <<std::fixed<<std::setprecision(9)
          <<times[static_cast<std::size_t>(method)]<<','
          <<outcomes[static_cast<std::size_t>(method)].size()<<','
          <<(method==5?CausalCostRouter::name(d.chosen):"none")<<','
          <<(method==5&&d.switched?1:0)<<','
          <<(method==5?d.decisionMs:0.)<<','
          <<(method==5?d.migrationMs:0.)<<'\n';
    }
   }
  }
  const double baselineMs=avg(stats[1].sampleMs);
  std::cout<<"snapshot,N,frames,timed,seed,material_code,id_span,method,"
              "mean_ms,p95_ms,sum_ms,v4_over_method,"
              "last_pairs,switches,selector_sum_ms,migration_sum_ms,"
              "cache_frames,bvh_frames,exhaustive_frames,exact_parity\n";
  for(std::size_t method=0;method<kMethods.size();++method){
   const auto& s=stats[method];
   const auto a=avg(s.sampleMs);
   std::cout<<std::filesystem::path(argv[1]).filename().string()
            <<','<<source.n<<','<<frames<<','<<s.sampleMs.size()
            <<','<<source.seed<<','<<source.materialCode<<','<<span
            <<','<<kMethods[method]<<','
            <<std::fixed<<std::setprecision(6)
            <<a<<','<<p95(s.sampleMs)<<','<<a*s.sampleMs.size()
            <<','<<baselineMs/a<<','<<s.lastPairs<<','<<s.switches<<','
            <<s.selectorTotalMs<<','<<s.migrationTotalMs<<','
            <<s.cacheFrames<<','<<s.bvhFrames<<','<<s.exhaustiveFrames
            <<",verified\n";
  }
  if(trace.is_open()&&!trace.good())
   throw std::runtime_error("Trace CSV failed");
  std::cerr<<"[CBP MATERIAL] exact current-AABB pair parity for all 6 methods;"
           <<" ID-block regions are a surrogate, not real mesh topology\n";
 }catch(const std::exception& e){
  std::cerr<<"CBP material replay failure: "<<e.what()<<'\n';
  return 1;
 }
}
