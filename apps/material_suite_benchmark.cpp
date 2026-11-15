// Kyoto material benchmark: the SAME evolving deformable snapshots are
// processed by rebuild SAP, global guarded reuse, certified regional variants,
// and an experimental online cost-aware regional/guard hybrid controller.
// Geometry generation and independent audit are OUTSIDE the timed query.
// Distinct physical parameters are exploratory mass-spring fixtures, not FEM.
// No GPU timing claims are made by this executable.
#include "ccp/material_scenes.hpp"
#include "ccp/selective_guard.hpp"
#include "ccp/generic_online.hpp"
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <numeric>
#include <stdexcept>
#include <string>
#include <vector>
namespace {
using Clock=std::chrono::steady_clock;
using namespace ccp::research;
using ccp::materials::World;
std::vector<Bounds> rawBounds(const std::vector<Primitive>& items){
 std::vector<Bounds> b;b.reserve(items.size());
 for(const auto& item:items)b.push_back(item.box);
 return b;
}
bool overlap(const Bounds& a,const Bounds& b){
 return a.min.x<=b.max.x&&b.min.x<=a.max.x&&
 a.min.y<=b.max.y&&b.min.y<=a.max.y&&a.min.z<=b.max.z&&b.min.z<=a.max.z;
}
std::vector<Bounds> padded(std::vector<Bounds> b,double margin){
 const double h=margin*.5;
 for(auto& v:b){
  v.min.x-=h;v.min.y-=h;v.min.z-=h;
  v.max.x+=h;v.max.y+=h;v.max.z+=h;
 }
 return b;
}
std::vector<Pair> exactGuard(const std::vector<Pair>& cached,const std::vector<Bounds>& current){
 // Guarded reuse guarantees a superset. Filtering is REQUIRED to make
 // candidate pair semantics directly comparable against exact current SAP.
 std::vector<Pair> out;out.reserve(cached.size());
 for(const auto p:cached){
  if(p.a<0||p.b<0||size_t(p.a)>=current.size()||size_t(p.b)>=current.size())
   throw std::runtime_error("invalid global guard candidate IDs");
  if(overlap(current[size_t(p.a)],current[size_t(p.b)]))out.push_back(p);
 }
 return out;
}
void validatePairVector(const std::vector<Pair>& pairs,size_t n){
 for(size_t i=0;i<pairs.size();++i){
  const auto p=pairs[i];
  if(p.a<0||p.b<=p.a||size_t(p.b)>=n)throw std::runtime_error("invalid candidate pair");
  if(i>0&&!(pairs[i-1]<p))throw std::runtime_error("unsorted or duplicate candidate pair");
 }
}
enum class Policy {SAP,Guard,SelectiveGuard,RegionFixed,RegionAdaptive,RegionHierarchy,Online};
const char* name(Policy p){
 switch(p){
 case Policy::SAP:return "sap_rebuild";
 case Policy::Guard:return "global_guarded_exact";
 case Policy::SelectiveGuard:return "selective_guard_exact";
 case Policy::RegionFixed:return "region_fixed";
 case Policy::RegionAdaptive:return "region_adaptive";
 case Policy::RegionHierarchy:return "region_hierarchical";
 case Policy::Online:return "online_cost_aware";
 }
 return "unknown";
}
// Shared with exact same serialized AABB replay experiments.
using ccp::bench::Result;
using ccp::bench::GenericOnline;
class BenchmarkPolicy {
 Policy policy_;
 RegionalCandidates regions_;
 ccp::live::BroadPhaseService guard_;
 ccp::research::SelectiveGuard selective_;
 GenericOnline online_;
 static constexpr double margin=.025;
public:
 explicit BenchmarkPolicy(Policy p):policy_(p){}
 Result query(const std::vector<Primitive>& input){
  if(policy_==Policy::Online)return online_.query(input);
  Result r;
  if(policy_==Policy::SAP){
   auto bounds=padded(rawBounds(input),margin);
   const auto begin=Clock::now();
   r.pairs=ccp::live::candidates(bounds,true);
   r.queryMs=std::chrono::duration<double,std::milli>(Clock::now()-begin).count();
  }else if(policy_==Policy::SelectiveGuard){
   auto bounds=rawBounds(input);
   ccp::research::SelectiveGuard::Metrics m;
   const auto begin=Clock::now();
   const auto& pairs=selective_.query(bounds,m);
   r.pairs.assign(pairs.begin(),pairs.end());
   r.queryMs=std::chrono::duration<double,std::milli>(Clock::now()-begin).count();
   r.guardBuilds=m.fullBuilds;r.guardReuses=m.cacheReuses;
   r.selectiveRepairs=m.partialRepairs;r.dirty=m.dirtyObjects;
   r.workers=m.parallelWorkers;
   r.aabbTests=m.pairChecks;
  }else if(policy_==Policy::Guard){
   auto bounds=rawBounds(input);
   ccp::live::QueryMetrics m;
   const auto begin=Clock::now();
   const auto& cached=guard_.query(bounds,{margin,.045,ccp::live::UpdatePolicy::GuardedReuse},true,m);
   r.pairs=exactGuard(cached,padded(bounds,margin));
   r.queryMs=std::chrono::duration<double,std::milli>(Clock::now()-begin).count();
   r.guardBuilds=m.builds;r.guardReuses=m.reuses;r.aabbTests=m.pairTests;
  }else{
   Metrics m;
   const bool adaptive=policy_!=Policy::RegionFixed;
   const bool hierarchy=policy_==Policy::RegionHierarchy;
   const auto begin=Clock::now();
   const auto& list=regions_.query(input,margin,adaptive,m,hierarchy);
   r.pairs.assign(list.begin(),list.end());
   r.queryMs=std::chrono::duration<double,std::milli>(Clock::now()-begin).count();
   r.builds=m.regionalPairRebuilds;r.reuses=m.regionalPairReuses;
   r.dirty=m.dirtyRegions;r.aabbTests=m.broadAabbTests;
  }
  return r;
 }
};
int positive(const char* s,int minimum,int maximum){
 size_t consumed=0;long value=0;
 try{value=std::stol(s,&consumed);}catch(...){throw std::invalid_argument("non-integer CLI option");}
 if(consumed!=std::string(s).size()||value<minimum||value>maximum)
  throw std::invalid_argument("CLI integer outside supported range");
 return int(value);
}
void run(const std::string& material,int requested,int frames,unsigned seed,
         int auditEvery,int verbose,std::ofstream& csv,const std::string& regime){
 // All competing policies restart from identical deterministic initial states
 // with the same random seed and evolve independent, equal-time trajectories.
 for(Policy policy:{Policy::SAP,Policy::Guard,Policy::SelectiveGuard,Policy::RegionFixed,
                    Policy::RegionAdaptive,Policy::RegionHierarchy,Policy::Online}){
  World world(material,requested,seed,regime);
  BenchmarkPolicy method(policy);
  if(verbose)std::cerr<<"[BEGIN] material="<<material<<" policy="<<name(policy)
    <<" primitives="<<world.elements()<<" nodes="<<world.nodes()
    <<" bonds="<<world.bonds()<<" frames="<<frames<<"\n";
  double sum=0;std::size_t maxPairs=0,checks=0,observedBuilds=0,observedReuses=0;
  for(int frame=0;frame<frames;++frame){
   world.step();
   auto items=world.snapshot(64);
   const auto output=method.query(items);
   validatePairVector(output.pairs,items.size());
   const bool audit=frame%auditEvery==0;
   std::size_t misses=0,extras=0,reference=0;
   if(audit){
    auto boxes=padded(rawBounds(items),.025);
    const auto oracle=ccp::live::candidates(boxes,true);
    if(items.size()<=512){
     const auto brute=ccp::live::candidates(boxes,false);
     if(oracle!=brute)throw std::runtime_error("reference SAP disagrees with independent exhaustive oracle");
    }
    reference=oracle.size();
    misses=std::size_t(std::count_if(oracle.begin(),oracle.end(),
      [&](const Pair& p){return !std::binary_search(output.pairs.begin(),output.pairs.end(),p);}));
    extras=std::size_t(std::count_if(output.pairs.begin(),output.pairs.end(),
      [&](const Pair& p){return !std::binary_search(oracle.begin(),oracle.end(),p);}));
    if(misses||extras)throw std::runtime_error("FAIL: missing or excess current AABB candidates in "+
      material+" policy="+name(policy)+" frame="+std::to_string(frame)+
      " misses="+std::to_string(misses)+" extra="+std::to_string(extras));
    ++checks;
   }
   csv<<material<<','<<regime<<','<<requested<<','<<items.size()<<','<<world.nodes()<<','
      <<world.bonds()<<','<<seed<<','<<frame<<','<<name(policy)<<','
      <<std::fixed<<std::setprecision(9)<<output.queryMs<<','
      <<output.pairs.size()<<','<<(audit?1:0)<<','<<reference<<','
      <<misses<<','<<extras<<','<<output.builds<<','<<output.reuses<<','
      <<output.dirty<<','<<output.guardBuilds<<','<<output.guardReuses<<','
      <<output.aabbTests<<','<<output.selectiveRepairs<<','<<output.workers<<','<<output.span<<','<<(output.usedGlobal?"global":"regional")
      <<','<<(output.changedSpan?1:0)<<','<<(output.switched?1:0)<<'\n';
   if(!csv)throw std::runtime_error("CSV output error");
   sum+=output.queryMs;maxPairs=std::max(maxPairs,output.pairs.size());
   observedBuilds+=output.builds+output.guardBuilds;
   observedReuses+=output.reuses+output.guardReuses;
   if(verbose>=2||(verbose&&((frame+1)%std::max(1,frames/8)==0||frame==0)))
    std::cerr<<"[FRAME] material="<<material<<" algorithm="<<name(policy)
     <<" "<<frame+1<<"/"<<frames<<" query_ms="<<output.queryMs
     <<" candidates="<<output.pairs.size()<<" audit="<<(audit?"PASS":"skip")
     <<" region_span="<<output.span<<" path="<<(output.usedGlobal?"guard":"region")
     <<" repairs="<<output.selectiveRepairs
     <<" builds="<<output.builds+output.guardBuilds
     <<" reuses="<<output.reuses+output.guardReuses<<"\n";
  }
  std::cerr<<"[RESULT] material="<<material<<" method="<<name(policy)
    <<" mean_query_ms="<<sum/frames<<" max_candidate_pairs="<<maxPairs
    <<" audits="<<checks<<" cache_builds="<<observedBuilds
    <<" cache_reuses="<<observedReuses<<"\n";
 }
}
}
int main(int argc,char** argv){
 try{
  if(argc<5||argc>9)throw std::runtime_error(
    "usage: ccp_material_benchmark OUTPUT.csv MATERIAL[cloth|sponge|cushion|soft_ball|spring|mixed|all] ELEMENT_BUDGET FRAMES [SEED=1234] [AUDIT_EVERY=5] [VERBOSE=1] [REGIME=baseline]");
  const std::filesystem::path path(argv[1]);
  const std::string kind=argv[2];
  const int n=positive(argv[3],16,50000),frames=positive(argv[4],1,10000);
  const unsigned seed=argc>=6?unsigned(positive(argv[5],1,2000000000)):1234;
  const int audit=argc>=7?positive(argv[6],1,10000):5;
  const int verbose=argc>=8?positive(argv[7],0,2):1;
  const std::string regime=argc>=9?argv[8]:"baseline";
  if(kind!="all"&&kind!="mixed")ccp::materials::materialCode(kind);
  if(!path.parent_path().empty())std::filesystem::create_directories(path.parent_path());
  if(std::filesystem::exists(path)&&std::getenv("CCP_CTEST_SMOKE")==nullptr)
   throw std::runtime_error("refusing to overwrite existing evidence CSV");
  std::ofstream csv(path);if(!csv)throw std::runtime_error("could not open evidence CSV");
  csv<<"material,regime,requested_elements,primitives,nodes,bonds,seed,frame,policy,query_ms,"
        "pairs,oracle_checked,oracle_pairs,misses,extras,region_builds,region_reuses,"
        "dirty_regions,guard_builds,guard_reuses,aabb_tests,selective_repairs,parallel_workers,online_span,online_path,"
        "online_span_changed,online_strategy_switch\n";
  const auto kinds=std::array<std::string,6>{"cloth","sponge","cushion","soft_ball","spring","mixed"};
  if(kind=="all"){
   for(const auto& k:kinds)run(k,n,frames,seed,audit,verbose,csv,regime);
  }else run(kind,n,frames,seed,audit,verbose,csv,regime);
  csv.flush();if(!csv)throw std::runtime_error("final evidence flush failed");
  std::cout<<"[PASS] complete material suite, raw CSV: "<<path<<"\n";
  return 0;
 }catch(const std::exception& e){
  std::cerr<<"[FAIL] MATERIAL BENCHMARK: "<<e.what()<<'\n';return 1;
 }
}
