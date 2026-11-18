// Phase 13: geometrically informed, EXACT global CPU broad-phase selector.
//
// NEW EXPERIMENT, not a replacement for or modification to frozen baselines.
// The selector uses ONLY current AABB locations and a conservative count of
// pair tests. It NEVER examines the scenario name or a future frame, nor
// does it execute an unselected query to learn which would have been faster.
//
// Global 3D hash-grid search suffers in dense spatial cells, including
// duplicated pairs across several cells. Global X-sweep can suffer in slabs.
// This test checks whether cheap bounding work estimates can distinguish both.
//
// Crucial limitation: grid building ALWAYS occurs before routing so decision
// and index construction must be included in complete CPU query cost. The
// sort/candidate-count estimator is paid only on difficult dense frames.
// This is a baseline ablation, NOT a publishable original collision algorithm.
#include "ccp/cbp_pressure.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <optional>
#if defined(__APPLE__) || defined(__linux__)
#include <sys/resource.h>
#endif
#include <stdexcept>
#include <string>
#include <vector>

namespace {
namespace fs=std::filesystem;
using namespace ccp::pressure;
using Clock=std::chrono::steady_clock;
struct SweepPreflight {
 std::vector<std::uint32_t> ids;
 std::uint64_t projectedPairs{};
 double prepMs{};
};
// The exact number of projected X-axis pairs is computed without testing Y/Z
// overlap, in O(N log N) including a sort. We only build this estimate when
// the grid signals high possible work, not in cheaply handled sparse scenes.
SweepPreflight sweepPreflight(const std::vector<Box>& boxes){
 const auto t=Clock::now();
 SweepPreflight p;
 p.ids.resize(boxes.size());
 std::vector<float> lows;
 lows.reserve(boxes.size());
 for(std::size_t i=0;i<boxes.size();++i)
  p.ids[i]=std::uint32_t(i);
 std::sort(p.ids.begin(),p.ids.end(),[&](auto a,auto b){
  if(boxes[a].lo[0]!=boxes[b].lo[0])
   return boxes[a].lo[0]<boxes[b].lo[0];
  return a<b;
 });
 for(const auto id:p.ids)lows.push_back(boxes[id].lo[0]);
 for(std::size_t i=0;i<p.ids.size();++i){
  const auto end=std::upper_bound(
   lows.begin()+static_cast<std::ptrdiff_t>(i+1),lows.end(),
   boxes[p.ids[i]].hi[0]);
  p.projectedPairs+=std::uint64_t(end-
    (lows.begin()+static_cast<std::ptrdiff_t>(i+1)));
 }
 p.prepMs=since(t);
 return p;
}
Result preparedSweep(const std::vector<Box>& boxes,
                     const std::vector<std::uint32_t>& order,
                     std::uint64_t budget,double msCap){
 Result out;
 const auto t=Clock::now();
 for(std::size_t i=0;i<order.size();++i){
  const auto a=order[i];
  for(std::size_t j=i+1;j<order.size();++j){
   const auto b=order[j];
   if(boxes[b].lo[0]>boxes[a].hi[0])break;
   if(out.tests>=budget||(out.tests%65536==0&&since(t)>msCap)){
    out.complete=false;out.ms=since(t);return out;
   }
   ++out.tests;
   if(overlap(boxes[a],boxes[b]))add(out.digest,a,b);
  }
 }
 out.ms=since(t);
 return out;
}
const char* name(bool chooseSweep){
 return chooseSweep?"projected_x_sweep":"global_3d_hash_grid";
}
double rssMiB(){
#if defined(__APPLE__)
 // Native rusage reports maxrss in BYTES on macOS.
 struct rusage r{};
 return getrusage(RUSAGE_SELF,&r)==0?
    double(r.ru_maxrss)/(1024.*1024.): -1.;
#elif defined(__linux__)
 // Linux reports maxrss in KiB.
 struct rusage r{};
 return getrusage(RUSAGE_SELF,&r)==0?double(r.ru_maxrss)/1024.:-1.;
#else
 return -1.;
#endif
}
}
int main(int argc,char** argv){
 try{
  if(argc!=9)throw std::invalid_argument(
    "Usage: ccp_cbp_pressure_hybrid_cpu OUTPUT.csv SCENE N FRAMES SEED "
    "MAX_TESTS MAX_FRAME_MS MAX_GRID_ENTRIES");
  const fs::path destination=argv[1];
  const std::string scenario=argv[2];
  if(!known(scenario))throw std::invalid_argument("Unknown generated scene");
  const auto n=number(argv[3],16,2000000),
             frames=number(argv[4],1,240),
             seed=number(argv[5],1,2147483647),
             maxTests=number(argv[6],1000,600000000),
             maxFrameMs=number(argv[7],20,120000),
             maxEntries=number(argv[8],10000,20000000);
  if(fs::exists(destination)||fs::exists(destination.string()+".part"))
   throw std::runtime_error("Immutable research output already exists");
  if(!destination.parent_path().empty())
   fs::create_directories(destination.parent_path());
  std::ofstream csv(destination.string()+".part");
  if(!csv)throw std::runtime_error("Cannot create bounded research evidence");
  csv<<std::fixed<<std::setprecision(6);
  csv<<"scene,method,n,frame,seed,status,selected_backend,"
        "generate_ms,grid_build_ms,estimator_ms,decision_ms,"
        "query_ms,complete_query_ms,grid_work_upper_bound,"
        "projected_sweep_pairs,selected_pair_tests,"
        "exact_pairs,hash64_sum,hash64_xor,hash32_sum,hash32_xor,"
        "analytic_count_verified,brute_oracle_checked,estimated_data_bytes,"
        "peak_rss_mib\n";
  std::uint64_t gridFrames=0,sweepFrames=0,incomplete=0;
  for(std::uint64_t f=0;f<frames;++f){
   const auto generated=Clock::now();
   const auto boxes=generate(std::size_t(n),std::uint32_t(f),
     std::uint32_t(frames),std::uint32_t(seed),scenario);
   const double genMs=since(generated);
   const auto queryBegin=Clock::now();
   const auto buildStart=Clock::now();
   const Grid grid=buildGrid(boxes,std::size_t(maxEntries));
   const double buildMs=since(buildStart);
   const auto decisionStart=Clock::now();
   SweepPreflight prep;
   // At low grid pressure the X-sort itself would exceed the benefit.
   // At high grid pressure, compare an exact X-projection pair count to
   // the grid's exact number of attempted cell-pair comparisons.
   const bool needCompare = grid.workUpperBound >
       std::uint64_t(n)*UINT64_C(128);
   bool sweep=false;
   if(needCompare){
    prep=sweepPreflight(boxes);
    // Factor two intentionally favors sweep when each grid comparison
    // includes canonical-cell checks and/or duplicate multi-cell visits.
    // This is a heuristic learned from NOTHING; no reference timings read.
    const std::uint64_t projected=prep.projectedPairs;
    sweep=projected<=maxTests &&
      (projected<=grid.workUpperBound/2 ||
       grid.workUpperBound>maxTests);
   }
   const double decisionMs=since(decisionStart);
   const Result output=sweep?
    preparedSweep(boxes,prep.ids,maxTests,double(maxFrameMs)):
    queryGrid(boxes,grid,maxTests,double(maxFrameMs));
   const double completeQueryMs=since(queryBegin);
   if(sweep)++sweepFrames;else ++gridFrames;
   if(!output.complete)++incomplete;

   bool bruteChecked=false,analyticChecked=false;
   if(output.complete&&n<=1024){
    if(output.digest!=brute(boxes))
     throw std::runtime_error("Independent brute-force oracle rejected hybrid");
    bruteChecked=true;
   }
   const auto knownCount=analyticCandidates(
    scenario,n,std::uint32_t(f),std::uint32_t(frames));
   if(output.complete&&knownCount){
    if(output.digest.count!=*knownCount)
     throw std::runtime_error("Analytic exact pair count contradicts hybrid");
    analyticChecked=true;
   }

   csv<<scenario<<",geometric_work_router,"<<n<<','<<f<<','<<seed<<','
      <<(output.complete?"complete":"budget_stop")<<','<<name(sweep)<<','
      <<genMs<<','<<buildMs<<','
      <<prep.prepMs<<','<<decisionMs<<','<<output.ms<<','
      <<completeQueryMs<<','<<grid.workUpperBound<<','
      <<(needCompare?std::to_string(prep.projectedPairs):"not_measured")
      <<','<<output.tests<<','<<output.digest.count<<','
      <<output.digest.sum<<','<<output.digest.xors<<','
      <<output.digest.sum32<<','<<output.digest.xor32<<','
      <<int(analyticChecked)<<','<<int(bruteChecked)<<','
      <<grid.estimatedBytes<<','<<rssMiB()<<'\n';
   if((f+1)%std::max<std::uint64_t>(1,frames/8)==0)
    std::cerr<<"[GEOMETRIC ROUTER] "<<scenario<<" "<<f+1
             <<'/'<<frames<<" backend "<<name(sweep)
             <<" grid_upper "<<grid.workUpperBound
             <<" projected "<<(needCompare?std::to_string(prep.projectedPairs):
                                    "skipped_small_grid")
             <<" measured_full_ms "<<completeQueryMs
             <<" exact_candidates "<<output.digest.count
             <<" status "<<(output.complete?"complete":"budget_stop")<<"\n";
  }
  csv.flush();
  if(!csv)throw std::runtime_error("Incomplete geometric router evidence");
  csv.close();fs::rename(destination.string()+".part",destination);
  std::cout<<"PASS bounded geometric-work router frames="<<frames
           <<" grid_frames="<<gridFrames
           <<" x_sweep_frames="<<sweepFrames
           <<" incomplete_frames="<<incomplete
           <<" (whole query includes grid construction, cost estimate, and one"
           <<" selected exact backend; no retrospective algorithm oracle)\n";
 }catch(const std::exception& e){
  std::cerr<<"GEOMETRIC ROUTER FAILED: "<<e.what()<<'\n';return 1;
 }
}
