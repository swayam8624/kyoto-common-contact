// Phase 11 global-pressure adversarial exact AABB broad-phase harness.
//
// This deliberately DOES NOT exploit known disjoint shards or predefined
// local ID ownership. Real current AABBs are processed by two independent
// global candidate-search algorithms (3D spatial hashing and X sweep).
// Optional caps report INCOMPLETE instead of fabricated exact pair counts.
// Counts/hashes are computed without retaining potentially TB-sized lists.
#include "ccp/cbp_pressure.hpp"
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>
#if defined(__unix__) || defined(__APPLE__)
#include <sys/resource.h>
#endif
namespace {
namespace fs=std::filesystem;
using namespace ccp::pressure;
double rssMiB(){
#if defined(__unix__) || defined(__APPLE__)
 struct rusage r{};
 if(getrusage(RUSAGE_SELF,&r)!=0)return -1.;
#if defined(__APPLE__)
 return double(r.ru_maxrss)/(1024.*1024.);
#else
 return double(r.ru_maxrss)/1024.;
#endif
#else
 return -1.;
#endif
}
std::string status(const Result& r){return r.complete?"complete":"budget_stop";}
void emit(std::ostream& out,const std::string& scene,const char* method,
          std::uint64_t n,std::uint64_t frame,std::uint32_t seed,
          double generation,double gridBuild,const Grid& grid,
          const Result& r,bool validated,bool oracleChecked,bool analytic){
 out<<scene<<','<<method<<','<<n<<','<<frame<<','<<seed<<','
    <<status(r)<<','<<generation<<','<<gridBuild<<','<<r.ms<<','
    <<r.tests<<','<<grid.entries<<','<<grid.bins.size()<<','
    <<grid.workUpperBound<<','<<r.digest.count<<','
    <<r.digest.sum<<','<<r.digest.xors<<','
    <<r.digest.sum32<<','<<r.digest.xor32<<','
    <<int(validated)<<','<<int(oracleChecked)<<','
    <<int(analytic)<<','<<grid.estimatedBytes<<','<<rssMiB()<<"\n";
}
}
int main(int argc,char** argv){
 try{
  if(argc!=9)throw std::invalid_argument(
    "Usage: ccp_cbp_pressure_cpu OUT.csv SCENE N FRAMES SEED "
    "MAX_TESTS_PER_METHOD MAX_FRAME_MS MAX_GRID_ENTRIES");
  const fs::path output=argv[1];
  const std::string scene=argv[2];
  if(!known(scene))throw std::invalid_argument("Unknown adversarial scene");
  const auto n=number(argv[3],16,2000000),frames=number(argv[4],1,240);
  const auto seed=number(argv[5],1,2147483647);
  const auto maxTests=number(argv[6],1000,1000000000);
  const auto maxMs=number(argv[7],20,120000);
  const auto maxEntries=number(argv[8],10000,20000000);
  if(fs::exists(output)||fs::exists(output.string()+".part"))
   throw std::runtime_error("Existing stress evidence; refusing overwrite");
  if(!output.parent_path().empty())fs::create_directories(output.parent_path());
  std::ofstream out(output.string()+".part");
  if(!out)throw std::runtime_error("Cannot create stress evidence CSV");
  out<<std::fixed<<std::setprecision(6);
  out<<"scene,method,n,frame,seed,status,generate_ms,grid_build_ms,query_ms,"
        "tested_pairs,grid_entries,grid_cells,grid_upper_bound,exact_pairs,"
        "hash64_sum,hash64_xor,hash32_sum,hash32_xor,"
        "cross_method_verified,brute_oracle_checked,analytic_all_overlap,"
        "estimated_data_bytes,peak_rss_mib\n";
  bool incomplete=false;
  for(std::uint64_t f=0;f<frames;++f){
   const auto before=std::chrono::steady_clock::now();
   const auto boxes=generate(std::size_t(n),std::uint32_t(f),std::uint32_t(frames),
                             std::uint32_t(seed),scene);
   const double generateMs=since(before);
   const auto started=std::chrono::steady_clock::now();
   const auto grid=buildGrid(boxes,std::size_t(maxEntries));
   const double buildMs=since(started);
   auto gridResult=queryGrid(boxes,grid,maxTests,double(maxMs));
   auto sweepResult=querySweep(boxes,maxTests,double(maxMs));
   const bool oracle=n<=1024&&gridResult.complete&&sweepResult.complete;
   if(oracle){
    const auto truth=brute(boxes);
    if(truth!=gridResult.digest||truth!=sweepResult.digest)
     throw std::runtime_error("Independent O(N^2) brute oracle failed");
   }
   const bool verified=gridResult.complete&&sweepResult.complete;
   if(verified&&gridResult.digest!=sweepResult.digest)
    throw std::runtime_error("Independent global-sweep and hash-grid pair DIGEST MISMATCH");
   if(scene=="all_overlap"&&gridResult.complete){
    if(gridResult.digest.count!=n*(n-1)/2)
     throw std::runtime_error("Known all-overlap cardinality mismatch");
   }
   if(!gridResult.complete||!sweepResult.complete)incomplete=true;
   emit(out,scene,"global_3d_hash_grid",n,f,std::uint32_t(seed),generateMs,
        buildMs,grid,gridResult,verified,oracle,scene=="all_overlap");
   emit(out,scene,"global_x_sweep",n,f,std::uint32_t(seed),generateMs,
        buildMs,grid,sweepResult,verified,oracle,scene=="all_overlap");
   if((f+1)%std::max<std::uint64_t>(1,frames/12)==0){
    std::cerr<<"[PRESSURE] "<<scene<<" N="<<n<<" "<<f+1<<"/"<<frames
             <<" 3D-grid "<<gridResult.ms<<" ms "
             <<gridResult.digest.count<<" emitted count-only pairs"
             <<" X-sweep "<<sweepResult.ms<<" ms"
             <<" exact-cross-check "<<(verified?"YES":"NO (budget)")
             <<" memory_RSS_MiB="<<rssMiB()<<"\n";
   }
  }
  out.flush();if(!out)throw std::runtime_error("Incomplete stress CSV");
  out.close();fs::rename(output.string()+".part",output);
  std::cout<<"PASS bounded adversarial CPU benchmark: scene="<<scene
           <<" n="<<n<<" frames="<<frames
           <<" paired-verification="<<(incomplete?"SOME INCOMPLETE, see CSV":"ALL COMPLETE")
           <<" (COUNT-ONLY, no output pair materialization)."
           <<"\n";
 }catch(const std::exception& e){
  std::cerr<<"ADVERSARIAL CPU BENCHMARK FAILED: "<<e.what()<<"\n";return 1;
 }
}
