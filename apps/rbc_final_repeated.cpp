// RBC Final Run 1: measured repeated full-pair replay with flat frontier and Guard A/A.
//
// This is a NEW executable, deliberately separate from the frozen
// apps/material_replay_benchmark.cpp used in the historical research.
// The entire native query, including real cache checks, candidate
// filtering, indexing, fallback and canonical sorting, is timed.
// The current frame's FULL exact sorted pair IDs are compared to a
// separately computed SAP oracle on EVERY method / EVERY frame.
// CSV is written outside timing; simulation and oracle work are NOT
// charged to each contender. No ML model or future frame is observed.
//
// Fairness: Guard at three skin values, same-skinned indexed and flat RBC,
// two independent Guard A/A controls, persistent and fresh SAP and V4.
// Each repeat constructs FRESH stateful policies; the method order
// alternates forward and reverse whole trajectories across repeats.
// This is exploratory AB/BA, NOT independent-clock-tuned ABBA or a
// contact solver end-to-end evaluation. Large scenes can be memory-
// or output-bound; report wall time, source and actual K separately.
//
// Usage: ccp_rbc_material_replay SOURCE.kmcb OUT.csv [REPEATS=4]
//                                     [WARMUP=8]
#include "ccp/rbc/sweep_certificate.hpp"
#include "ccp/rbc/certified_flat_frontier.hpp"
#include "ccp/final_broadphase.hpp"
#include "ccp/live_cloth.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using ccp::live::Bounds;
using ccp::live::Pair;
using Pairs=std::vector<Pair>;
using Clock=std::chrono::steady_clock;

struct Source {
 std::uint32_t n{},frames{},seed{},material{};
 std::vector<std::vector<Bounds>> samples;
};

std::uint32_t positive_number(const char* text,int lo,int hi){
 std::size_t end=0;
 long long value=0;
 try{value=std::stoll(text,&end);}
 catch(...){throw std::runtime_error("invalid integer parameter");}
 if(end!=std::string(text).size()||value<lo||value>hi)
   throw std::runtime_error("integer outside allowed range");
 return std::uint32_t(value);
}

Source read_source(const std::string& path){
 std::ifstream input(path,std::ios::binary);
 if(!input)throw std::runtime_error("cannot open immutable material source");
 char magic[8]{};
 input.read(magic,8);
 if(!input||std::string(magic,8)!="KYOTOM01")
  throw std::runtime_error("KYOTOM01 magic mismatch");
 Source data;
 for(auto* p:{&data.n,&data.frames,&data.seed,&data.material})
  input.read(reinterpret_cast<char*>(p),sizeof(std::uint32_t));
 if(!input||!data.n||data.n>50000||!data.frames||data.frames>10000||
    data.material>5||std::uint64_t(data.n)*data.frames>3000000)
  throw std::runtime_error("source size cap/header invalid");
 const auto begin=input.tellg();
 input.seekg(0,std::ios::end);
 const auto bytes=input.tellg()-begin;
 const auto expected=std::streamoff(data.n)*std::streamoff(data.frames)*
                       std::streamoff(6*sizeof(float));
 if(bytes!=expected)throw std::runtime_error("source byte count mismatch");
 input.seekg(begin);
 data.samples.reserve(data.frames);
 for(std::uint32_t frame=0;frame<data.frames;++frame){
  auto& boxes=data.samples.emplace_back();
  boxes.reserve(data.n);
  for(std::uint32_t i=0;i<data.n;++i){
   float a[6]{};
   input.read(reinterpret_cast<char*>(a),sizeof(a));
   if(!input)throw std::runtime_error("truncated frame");
   for(float x:a)if(!std::isfinite(x))
    throw std::runtime_error("nonfinite binary bounds");
   if(a[0]>a[3]||a[1]>a[4]||a[2]>a[5])
    throw std::runtime_error("source inverted bounds");
   boxes.push_back({int(i),{a[0],a[1],a[2]},
                           {a[3],a[4],a[5]},1u,1u,true});
  }
 }
 return data;
}

std::uint64_t hash_pairs(const Pairs& pairs) {
 std::uint64_t h=UINT64_C(14695981039346656037);
 for(const auto& p:pairs)
  for(std::uint32_t x:{std::uint32_t(p.a),std::uint32_t(p.b)}){
   h ^= std::uint64_t(x);
   h *= UINT64_C(1099511628211);
  }
 return h;
}

Pairs full_filter(const Pairs& potentially,
                  const std::vector<Bounds>& boxes){
 Pairs result;
 result.reserve(potentially.size());
 for(auto p:potentially) {
  const auto& a=boxes.at(std::size_t(p.a));
  const auto& b=boxes.at(std::size_t(p.b));
  if(ccp::rbc::compatible(a,b)&&ccp::rbc::linf_box_gap(a,b)==0.0)
   result.push_back(p);
 }
 return result;
}

enum class Kind {
 guard_a,guard_b,guard_100,guard_200,
 rbc_045,rbc_100,rbc_200,
 sap_fresh,sap_persistent,frozen_v4,flat_045
};
constexpr std::array<Kind,11> kinds={
 Kind::guard_a,Kind::guard_b,Kind::guard_100,Kind::guard_200,
 Kind::rbc_045,Kind::rbc_100,Kind::rbc_200,
 Kind::sap_fresh,Kind::sap_persistent,Kind::frozen_v4,
 Kind::flat_045
};
const char* name(Kind k) {
 switch(k){
 case Kind::guard_a:return "guard_045_A";
 case Kind::guard_b:return "guard_045_B";
 case Kind::guard_100:return "guard_100";
 case Kind::guard_200:return "guard_200";
 case Kind::rbc_045:return "rbc_indexed_045";
 case Kind::rbc_100:return "rbc_indexed_100";
 case Kind::rbc_200:return "rbc_indexed_200";
 case Kind::sap_fresh:return "sap_rebuild";
 case Kind::sap_persistent:return "sap_persistent";
 case Kind::frozen_v4:return "guard_sap_bounded_trial_frozen";
 case Kind::flat_045:return "rbc_flat_045";
 }
 throw std::runtime_error("invalid policy");
}
double skin(Kind k){
 switch(k){
 case Kind::guard_a:case Kind::guard_b:case Kind::rbc_045:
 case Kind::flat_045:return .045;
 case Kind::guard_100:case Kind::rbc_100:return .1;
 case Kind::guard_200:case Kind::rbc_200:return .2;
 default:return 0.;
 }
}
struct Observation{
 Pairs pairs;
 std::size_t builds{},reuses{},potential{},checks{},x_sweep_tests{};
 bool sap_selected{},trial{},switched{};
 double ms{};
};

class Policy {
 Kind k_;
 bool initialized_{};
 ccp::live::BroadPhaseService guard_;
 ccp::rbc::CertifiedSweepIndex rbc_;
 ccp::rbc::CertifiedFlatFrontier flat_;
 ccp::final_broadphase::PersistentSAP sap_;
 ccp::final_broadphase::GuardSAPBoundedTrial v4_;
 std::vector<ccp::rbc::Reach> radii_;
public:
 explicit Policy(Kind k):k_(k){}
 Observation query(const std::vector<Bounds>& boxes){
  Observation obs;
  const auto start=Clock::now();
  if(k_==Kind::rbc_045||k_==Kind::rbc_100||k_==Kind::rbc_200){
   if(!initialized_) {
    radii_.assign(boxes.size(),{0.,skin(k_)});
    rbc_.initialize(boxes,radii_);
    initialized_=true;
   }
   auto result=rbc_.query(boxes);
   obs.pairs=std::move(result.pairs);
   const auto& m=rbc_.metrics();
   obs.builds=m.reference_builds;obs.reuses=m.reference_reuses;
   obs.checks=m.current_candidate_tests;
   obs.x_sweep_tests=m.x_sweep_tests;
   obs.potential=m.potential_pairs;
  }else if(k_==Kind::flat_045){
   if(!flat_.initialized()){
    radii_.assign(boxes.size(),{0.,skin(k_)});
    flat_.initialize(boxes,radii_);
   }
   auto result=flat_.query(boxes);
   obs.pairs=std::move(result.pairs);
   obs.builds=flat_.global_builds();
   obs.potential=result.potential_pairs;
   obs.checks=result.query_pair_checks;
  }else if(k_==Kind::guard_a||k_==Kind::guard_b||
           k_==Kind::guard_100||k_==Kind::guard_200){
   ccp::live::QueryMetrics metric;
   const auto& candidate=guard_.query(
    boxes,{0.,skin(k_),ccp::live::UpdatePolicy::GuardedReuse},true,metric);
   obs.pairs=full_filter(candidate,boxes);
   obs.builds=metric.builds;obs.reuses=metric.reuses;
   obs.checks=metric.pairTests;
  }else if(k_==Kind::sap_fresh)
   obs.pairs=ccp::live::candidates(boxes,true);
  else if(k_==Kind::sap_persistent)
   obs.pairs=sap_.query(boxes);
  else {
   auto result=v4_.query(boxes);
   obs.pairs=std::move(result.pairs);
   obs.sap_selected=std::string(result.action)=="sap_persistent_coherent";
   obs.trial=result.trialFrame;
   obs.switched=result.switched;
  }
  obs.ms=std::chrono::duration<double,std::milli>(
   Clock::now()-start).count();
  if(!std::isfinite(obs.ms)||obs.ms<0)
   throw std::runtime_error("nonfinite native query duration");
  return obs;
 }
};

void benchmark(const Source& source,const std::string& output,
               std::uint32_t repeats,std::uint32_t warmup){
 std::vector<Pairs> oracle;
 oracle.reserve(source.frames);
 for(const auto& sample:source.samples) {
  auto actual=ccp::live::candidates(sample,true);
  if(!std::is_sorted(actual.begin(),actual.end())||
     std::adjacent_find(actual.begin(),actual.end())!=actual.end())
   throw std::runtime_error("independent SAP oracle returned noncanonical pairs");
  // For smaller inputs, cross-check SAP itself against independently
  // implemented exhaustive current reach-AABB (radius=0).
  if(source.n<=384) {
   const std::vector<ccp::rbc::Reach> zero(sample.size(),ccp::rbc::Reach{});
   if(actual!=ccp::rbc::exhaustive(sample,zero))
    throw std::runtime_error("SAP and brute-force oracle mismatch");
  }
  oracle.push_back(std::move(actual));
 }
 const std::filesystem::path path(output);
 if(std::filesystem::exists(path))
  throw std::runtime_error("refusing to overwrite existing research CSV");
 if(!path.parent_path().empty())
  std::filesystem::create_directories(path.parent_path());
 std::ofstream csv(path,std::ios::out|std::ios::trunc);
 if(!csv)throw std::runtime_error("cannot create native CSV");
 csv<<"repeat,slot,method,frame,n,source_seed,material_code,"
       "pairs,pair_hash,ms,cumulative_builds,cumulative_reuses,"
       "potential_pairs,cumulative_current_checks,cumulative_x_sweep_tests,"
       "v4_sap,v4_trial,v4_switched,oracle_equal\n";
 csv<<std::setprecision(17);
 std::uint64_t rows=0;
 for(std::uint32_t repeat=0;repeat<repeats;++repeat){
  std::vector<Kind> order(kinds.begin(),kinds.end());
  if(repeat%2==1)std::reverse(order.begin(),order.end());
  for(std::size_t slot=0;slot<order.size();++slot) {
   const Kind kind=order[slot];
   Policy policy(kind);
   for(std::uint32_t frame=0;frame<source.frames;++frame) {
    auto result=policy.query(source.samples[frame]);
    if(result.pairs!=oracle[frame])
     throw std::runtime_error("NATIVE EXACT PAIR FAILURE: method "+
                              std::string(name(kind))+" frame "+
                              std::to_string(frame));
    if(result.pairs.size()!=oracle[frame].size()||
       hash_pairs(result.pairs)!=hash_pairs(oracle[frame]))
     throw std::runtime_error("NATIVE HASH/PAIR mismatch");
    if(frame<warmup)continue;
    csv<<repeat<<','<<slot<<','<<name(kind)<<','<<frame<<','
       <<source.n<<','<<source.seed<<','<<source.material<<','
       <<result.pairs.size()<<','<<hash_pairs(result.pairs)<<','
       <<result.ms<<','<<result.builds<<','<<result.reuses<<','
       <<result.potential<<','<<result.checks<<','
       <<result.x_sweep_tests<<','
       <<int(result.sap_selected)<<','<<int(result.trial)<<','
       <<int(result.switched)<<",1\n";
    ++rows;
   }
  }
 }
 csv.flush();
 if(!csv)throw std::runtime_error("failed to write native method timing CSV");
 const std::uint64_t expected=std::uint64_t(repeats)*kinds.size()*
                               (source.frames-warmup);
 if(rows!=expected)throw std::runtime_error("incomplete native observations");
 std::cout<<"PASS RBC full-pair replay: N="<<source.n
          <<" source_frames="<<source.frames
          <<" warmup="<<warmup
          <<" repeats="<<repeats
          <<" methods="<<kinds.size()
          <<" verified_timed_rows="<<rows
          <<" output="<<output<<"\n";
}

} // namespace

int main(int argc,char** argv) {
 try{
  if(argc<3||argc>5)
   throw std::runtime_error(
    "usage: ccp_rbc_material_replay SOURCE.kmcb OUT.csv [REPEATS=4] [WARMUP=8]");
  const auto repeats=argc>=4?positive_number(argv[3],1,32):4u;
  const auto warmup=argc>=5?positive_number(argv[4],0,10000):8u;
  const Source source=read_source(argv[1]);
  if(warmup>=source.frames)
   throw std::runtime_error("warmup >= source frames");
  benchmark(source,argv[2],repeats,warmup);
 } catch(const std::exception& e) {
  std::cerr<<"RBC MATERIAL REPLAY FAILED: "<<e.what()<<"\n";
  return 1;
 }
}
