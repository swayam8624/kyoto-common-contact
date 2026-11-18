// Phase 14 — full-cost exact dynamic spatial index + optional dense sweep.
//
// Uses current-frame geometry only, with NO known isolated-object partition.
// Index persists across frames, and only cell-range crossing boxes update
// memberships. If many boxes move between cells, rebuild (charged) instead.
// Each frame executes exactly ONE complete candidate query, either the
// maintained grid or an X-projection sweep chosen by a bounded preflight.
//
// Every query's complete digest must agree with a separately executed frozen
// global reference in CI / the Mac CSV report. Small frame sizes are checked
// by the independent O(N^2) brute oracle within this executable.
// This tests a familiar incremental index design, NOT a research novelty.
#include "ccp/cbp_pressure_incremental_flat.hpp"
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <stdexcept>
#include <string>
#include <vector>
#if defined(__APPLE__) || defined(__linux__)
#include <sys/resource.h>
#endif
namespace {
namespace fs=std::filesystem;
using namespace ccp::pressure;
using Clock=std::chrono::steady_clock;
double rssMiB(){
#if defined(__APPLE__)
 struct rusage r{};
 return getrusage(RUSAGE_SELF,&r)==0?
     double(r.ru_maxrss)/(1024.*1024.): -1.;
#elif defined(__linux__)
 struct rusage r{};
 return getrusage(RUSAGE_SELF,&r)==0?
     double(r.ru_maxrss)/1024.:-1.;
#else
 return -1.;
#endif
}
struct SweepPlan {
 std::vector<std::uint32_t> order;
 std::uint64_t projectedPairs{};
 double planMs{};
};
SweepPlan planSweep(const std::vector<Box>& boxes){
 const auto start=Clock::now();
 SweepPlan p;
 p.order.resize(boxes.size());
 std::iota(p.order.begin(),p.order.end(),0u);
 std::sort(p.order.begin(),p.order.end(),[&](auto a,auto b){
  if(boxes[a].lo[0]!=boxes[b].lo[0])
   return boxes[a].lo[0]<boxes[b].lo[0];
  return a<b;
 });
 std::vector<float> sortedLower;
 sortedLower.reserve(boxes.size());
 for(const auto id:p.order)sortedLower.push_back(boxes[id].lo[0]);
 for(std::size_t i=0;i<boxes.size();++i){
  const auto first=sortedLower.begin()+std::ptrdiff_t(i+1);
  const auto last=std::upper_bound(first,sortedLower.end(),
                                  boxes[p.order[i]].hi[0]);
  p.projectedPairs+=std::uint64_t(last-first);
 }
 p.planMs=since(start);
 return p;
}
Result queryPreparedSweep(const std::vector<Box>& boxes,
                          const SweepPlan& p,
                          std::uint64_t budget,double frameCap){
 Result result;
 const auto start=Clock::now();
 for(std::size_t i=0;i<p.order.size();++i){
  const auto a=p.order[i];
  for(std::size_t j=i+1;j<p.order.size();++j){
   const auto b=p.order[j];
   if(boxes[b].lo[0]>boxes[a].hi[0])break;
   if(result.tests>=budget||
      (result.tests%65536==0&&since(start)>frameCap)){
    result.complete=false;result.ms=since(start);return result;
   }
   ++result.tests;
   if(overlap(boxes[a],boxes[b]))add(result.digest,a,b);
  }
 }
 result.ms=since(start);
 if(result.tests!=p.projectedPairs)
  throw std::logic_error("Projection preflight contradicted exact sweep work");
 return result;
}
}
int main(int argc,char** argv){
 try{
  if(argc!=9&&argc!=11)throw std::invalid_argument(
    "Usage: ccp_cbp_pressure_incremental_flat_cpu OUTPUT.csv SCENE N FRAMES "
    "SEED MAX_TESTS MAX_FRAME_MS MAX_GRID_ENTRIES [--threads 1..32]");
  const fs::path destination=argv[1];
  const std::string scene=argv[2];
  if(!known(scene))throw std::invalid_argument("Unknown stress scene");
  const auto n=number(argv[3],16,2000000),
             frames=number(argv[4],1,240),
             seed=number(argv[5],1,2147483647),
             maxTests=number(argv[6],1000,600000000),
             maxMs=number(argv[7],20,120000),
             maxEntries=number(argv[8],10000,20000000);
  if(argc==11&&std::string(argv[9])!="--threads")
   throw std::invalid_argument("Additional argument must be --threads");
  const std::size_t requestedThreads=argc==11?
    std::size_t(number(argv[10],1,32)):1u;
  if(fs::exists(destination)||fs::exists(destination.string()+".part"))
   throw std::runtime_error("Refusing to overwrite immutable experiment CSV");
  if(!destination.parent_path().empty())
   fs::create_directories(destination.parent_path());
  std::ofstream csv(destination.string()+".part");
  if(!csv)throw std::runtime_error("Cannot open incremental evidence output");
  csv<<std::fixed<<std::setprecision(6);
  csv<<"scene,method,n,frame,seed,status,selected_backend,generate_ms,"
        "preflight_ms,index_maintenance_ms,decision_ms,query_ms,"
        "complete_query_ms,rebuilt,changed_cell_ranges,changed_box_bounds,"
        "membership_insertions,membership_removals,active_cells,grid_entries,"
        "grid_work_upper_bound,projected_sweep_pairs,selected_pair_tests,"
        "exact_pairs,hash64_sum,hash64_xor,hash32_sum,hash32_xor,"
        "analytical_construct_verified,brute_oracle_checked,peak_rss_mib,"
        "thread_limit,workers_used\n";
  FlatPersistentGrid index(maxEntries);
  std::uint64_t rebuilds=0,gridSelected=0,sweepSelected=0,incomplete=0;
  std::uint64_t totalRepairs=0;
  for(std::uint64_t f=0;f<frames;++f){
   const auto frameStart=Clock::now();
   const auto boxes=generate(std::size_t(n),std::uint32_t(f),
     std::uint32_t(frames),std::uint32_t(seed),scene);
   const double genMs=since(frameStart);
   const auto totalStart=Clock::now();
   const auto change=index.advance(boxes);
   if(change.rebuilt)++rebuilds;
   totalRepairs+=change.changedRanges;
   const auto decideStart=Clock::now();
   SweepPlan plan;
   const bool needCompare=change.workUpper>n*UINT64_C(128);
   bool sweep=false;
   if(needCompare){
    plan=planSweep(index.currentBoxes());
    sweep=plan.projectedPairs<=maxTests &&
      (plan.projectedPairs<=change.workUpper/2 ||
       change.workUpper>maxTests);
   }
   const double decisionMs=since(decideStart);
   const auto selected=Clock::now();
   std::size_t actualWorkers=1;
   const auto result=sweep?
       queryPreparedSweep(index.currentBoxes(),plan,maxTests,double(maxMs)):
       index.enumerateParallel(maxTests,double(maxMs),requestedThreads,
                               &actualWorkers);
   const double queried=since(selected);
   const double fullMs=since(totalStart);
   if(sweep)++sweepSelected;else ++gridSelected;
   if(!result.complete)++incomplete;
   bool bruteVerified=false,analyticVerified=false;
   if(result.complete&&n<=1024){
    if(result.digest!=brute(boxes))
     throw std::runtime_error("Incremental grid/sweep full brute oracle mismatch");
    bruteVerified=true;
   }
   const auto knownCount=analyticCandidates(
     scene,n,std::uint32_t(f),std::uint32_t(frames));
   if(result.complete&&knownCount){
    if(result.digest.count!=*knownCount)
     throw std::runtime_error("Known procedural pair count contradicts index");
    analyticVerified=true;
   }
   csv<<scene<<",persistent_flat_geometry_router,"<<n<<','<<f<<','<<seed<<','
      <<(result.complete?"complete":"budget_stop")<<','
      <<(sweep?"projected_x_sweep":
          actualWorkers>1?"persistent_flat_parallel_3d_grid":
          "persistent_flat_3d_grid")<<','
      <<genMs<<','<<change.preflightMs<<','<<change.maintenanceMs<<','
      <<decisionMs<<','<<queried<<','<<fullMs<<','
      <<int(change.rebuilt)<<','<<change.changedRanges<<','
      <<change.changedBounds<<','<<change.insertions<<','
      <<change.removals<<','<<change.activeCells<<','
      <<change.entries<<','<<change.workUpper<<','
      <<(needCompare?std::to_string(plan.projectedPairs):"not_measured")<<','
      <<result.tests<<','<<result.digest.count<<','
      <<result.digest.sum<<','<<result.digest.xors<<','
      <<result.digest.sum32<<','<<result.digest.xor32<<','
      <<int(analyticVerified)<<','<<int(bruteVerified)<<','
      <<rssMiB()<<','<<requestedThreads<<','<<actualWorkers<<'\n';
   if((f+1)%std::max<std::uint64_t>(1,frames/10)==0)
    std::cerr<<"[FLAT PERSISTENT CBP] "<<scene<<" "<<f+1<<"/"<<frames
             <<" mode="<<(sweep?"x-sweep":"3D-grid")
             <<" full_ms="<<fullMs<<" preflight="<<change.preflightMs
             <<" repair="<<change.maintenanceMs
             <<" query="<<queried
             <<" changed_memberships="<<change.changedRanges
             <<" rebuilt="<<int(change.rebuilt)
             <<" workers="<<actualWorkers
             <<" exact_pairs="<<result.digest.count
             <<" complete="<<int(result.complete)<<"\n";
  }
  csv.flush();
  if(!csv)throw std::runtime_error("Incremental evidence flush failed");
  csv.close();
  fs::rename(destination.string()+".part",destination);
  std::cout<<"PASS bounded FLAT persistent spatial occupancy: frames="<<frames
           <<" requested_threads="<<requestedThreads
           <<" rebuilds="<<rebuilds<<" changed_cell_ranges="<<totalRepairs
           <<" grid_queries="<<gridSelected<<" swept_queries="<<sweepSelected
           <<" incomplete="<<incomplete
           <<" (no offline winner, full index maintenance charged,"
              " count+fingerprint output, NOT full pair list or solver)\n";
 }catch(const std::exception& e){
  std::cerr<<"PERSISTENT CBP FAILED: "<<e.what()<<'\n';return 1;
 }
}
