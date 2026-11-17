// CBP Phase 3 diagnostic: three fixed cross-region kernels versus ONE
// causal router on identical *changing* deterministic AABB trajectories.
// Fixed methods run separately for external diagnostics/parity; their
// durations are NEVER passed to the causal controller.
// Each method owns independent persistent state; entire query time is charged.
// No material labels, hindsight decisions, or per-frame inactive probes.
#include "ccp/cbp_cost_manager.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using ccp::cbp::Bounds;
using ccp::cbp::CrossMode;
using ccp::cbp::CausalCostRouter;
using ccp::cbp::RegionRouter;
using ccp::cbp::Pairs;
using Clock=std::chrono::steady_clock;

constexpr std::array<const char*,5> kStage{
 "spaced","clustered","dense","spaced_return","rapid_deformation"};
constexpr std::array<const char*,4> kMethods{
 "exhaustive","shared_bvh","certified_cache","causal_manager"};

struct Statistics {
 std::vector<double> times;
 std::size_t pairs{},switches{},cacheFrames{},bvhFrames{},exhaustiveFrames{};
 double selectorMs{},migrationMs{};
};
double mean(const std::vector<double>& samples){
 return std::accumulate(samples.begin(),samples.end(),0.)/double(samples.size());
}
double p95(std::vector<double> samples){
 std::sort(samples.begin(),samples.end());
 return samples[static_cast<std::size_t>(std::ceil(.95*samples.size()))-1];
}
void generate(std::vector<Bounds>& boxes,std::vector<int>& labels,int stage,int t){
 const int n=static_cast<int>(boxes.size());
 for(int i=0;i<n;++i){
  double x{},y{},z{};
  if(stage==0||stage==3)x=double(i)*3.5;
  else if(stage==1||stage==4){
   x=double(i%16)*.44;
   y=double(i/16)*.44;
   z=double(i%3)*.06;
  }else{
   x=double(i%16)*.002;
   y=double(i/16)*.002;
  }
  x+=(stage==4?.07:.0004)*double((i%5)-2)*t;
  const double extent=stage==2?2.:stage==0||stage==3?.5:.55;
  boxes[static_cast<std::size_t>(i)]={i,{x,y,z},
    {x+extent,y+extent,z+extent},
    i%9==0?2u:1u,i%19==0?0u:3u,true};
  labels[static_cast<std::size_t>(i)]=i%8;
 }
}
void print(const char* stage,const char* method,int n,const Statistics& s){
 if(s.times.empty())return;
 std::cout<<stage<<','<<method<<','<<n<<','<<s.times.size()<<','
          <<std::fixed<<std::setprecision(6)
          <<mean(s.times)<<','<<p95(s.times)<<','<<s.pairs<<','
          <<s.switches<<','<<(s.selectorMs/double(s.times.size()))<<','
          <<s.migrationMs<<','<<s.cacheFrames<<','<<s.bvhFrames<<','
          <<s.exhaustiveFrames<<'\n';
}
}
int main(int argc,char** argv){
 try{
  if(argc>3)throw std::invalid_argument("Usage: cbp_manager_bench [N=256] [frames_per_stage=24]");
  const int n=argc>1?std::stoi(argv[1]):256;
  const int duration=argc>2?std::stoi(argv[2]):24;
  if(n<32||n>1024||duration<12||duration>96)
   throw std::invalid_argument("Bounded experiment: N 32..1024, stage frames 12..96");
  std::vector<Bounds> boxes(static_cast<std::size_t>(n));
  std::vector<int> labels(static_cast<std::size_t>(n));

  std::array<RegionRouter,3> fixed;
  fixed[0].setCrossMode(CrossMode::Exhaustive);
  fixed[1].setCrossMode(CrossMode::SharedBVH);
  fixed[2].setCrossMode(CrossMode::CertifiedCache);
  CausalCostRouter manager;
  for(auto& r:fixed)r.setSapThreshold(48);
  manager.setSapThreshold(48);

  std::array<std::array<Statistics,4>,5> stageStats{};
  std::array<Statistics,4> overall{};
  constexpr int warmup=8;
  for(int stage=0;stage<5;++stage){
   for(int t=0;t<duration;++t){
    const int frame=stage*duration+t;
    generate(boxes,labels,stage,t);
    std::array<Pairs,4> outputs;
    for(int k=0;k<4;++k){
     const int method=(frame+k)%4; // Rotate order, not used as controller input.
     const auto begin=Clock::now();
     if(method<3)outputs[static_cast<std::size_t>(method)]=
       fixed[static_cast<std::size_t>(method)].query(boxes,labels);
     else outputs[3]=manager.query(boxes,labels);
     const double elapsed=std::chrono::duration<double,std::milli>(
                          Clock::now()-begin).count();
     if(frame<warmup)continue;
     auto& ss=stageStats[static_cast<std::size_t>(stage)][static_cast<std::size_t>(method)];
     auto& all=overall[static_cast<std::size_t>(method)];
     for(Statistics* stat:{&ss,&all}){
      stat->times.push_back(elapsed);
      stat->pairs=outputs[static_cast<std::size_t>(method)].size();
      if(method==3){
       const auto d=manager.decision();
       stat->switches+=static_cast<std::size_t>(d.switched);
       stat->selectorMs+=d.decisionMs;
       stat->migrationMs+=d.migrationMs;
       stat->cacheFrames+=static_cast<std::size_t>(d.chosen==CrossMode::CertifiedCache);
       stat->bvhFrames+=static_cast<std::size_t>(d.chosen==CrossMode::SharedBVH);
       stat->exhaustiveFrames+=static_cast<std::size_t>(d.chosen==CrossMode::Exhaustive);
      }
     }
    }
    if(outputs[0]!=outputs[1]||outputs[0]!=outputs[2]||outputs[0]!=outputs[3])
     throw std::runtime_error("Four-way exact pair mismatch at frame "+std::to_string(frame));
   }
  }
  std::cout<<"stage,method,N,frames,mean_ms,p95_ms,last_pairs,"
              "switches,mean_decision_ms,migration_total_ms,"
              "cache_frames,bvh_frames,exhaustive_frames\n";
  for(int stage=0;stage<5;++stage)
   for(int method=0;method<4;++method)
    print(kStage[static_cast<std::size_t>(stage)],
          kMethods[static_cast<std::size_t>(method)],
          n,stageStats[static_cast<std::size_t>(stage)][static_cast<std::size_t>(method)]);
  for(int method=0;method<4;++method)
   print("all",kMethods[static_cast<std::size_t>(method)],n,
         overall[static_cast<std::size_t>(method)]);
  const double bestFixed=std::min({
     mean(overall[0].times),mean(overall[1].times),mean(overall[2].times)});
  std::cerr<<"[CBP phase3 exploratory] manager/best-fixed mean speedup="
           <<(bestFixed/mean(overall[3].times))
           <<"x on staged SYNTHETIC geometry; not solver/SIGGRAPH evidence\n";
 }catch(const std::exception& e){
  std::cerr<<"CBP manager diagnostic failure: "<<e.what()<<'\n';
  return 1;
 }
}
