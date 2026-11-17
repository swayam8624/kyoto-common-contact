// Small, honest Phase-1/2 three-way comparison. NO cross-scene claims.
// Three independently stateful routers process IDENTICAL snapshots.
// Alternating query order reduces fixed-order confounding; both pay all
// region routing, BVH maintenance, pair generation and normalization costs.
#include "ccp/cbp_region_router.hpp"
#include <algorithm>
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
using ccp::cbp::RegionRouter;
using Clock=std::chrono::steady_clock;
struct Stats{
 std::vector<double> times;
 std::size_t crossTests{},changedLeaves{},refits{},pairs{};
};
double avg(const std::vector<double>& v){
 return std::accumulate(v.begin(),v.end(),0.)/static_cast<double>(v.size());
}
double p95(std::vector<double> v){
 std::sort(v.begin(),v.end());
 return v[static_cast<std::size_t>(std::ceil(.95*v.size()))-1];
}
void runCase(const std::string& name,int n,int frames){
 std::vector<Bounds> boxes;
 std::vector<int> labels;
 boxes.reserve(static_cast<std::size_t>(n));
 labels.reserve(static_cast<std::size_t>(n));
 for(int i=0;i<n;++i){
  double x{},y{},z{};
  if(name=="spaced"){x=i*4.;y=0.;z=0.;}
  else if(name=="clustered"){x=(i%16)*.46;y=(i/16)*.46;z=(i%3)*.05;}
  else{x=(i%16)*.006;y=(i/16)*.006;z=0.;}
  const double size=name=="dense"?2.:name=="spaced"?.45:.55;
  boxes.push_back({i,{x,y,z},{x+size,y+size,z+size},
                   i%17==0?2u:1u,3u,true});
  labels.push_back(i%8);
 }
 RegionRouter brute,indexed,certified;
 brute.setCrossMode(CrossMode::Exhaustive);
 indexed.setCrossMode(CrossMode::SharedBVH);
 certified.setCrossMode(CrossMode::CertifiedCache);
 certified.setCachePadding(.045);
 brute.setSapThreshold(48);
 indexed.setSapThreshold(48);
 certified.setSapThreshold(48);
 Stats data[3];
 const int warmup=std::max(8,frames/10);
 for(int frame=0;frame<frames;++frame){
  // Dynamic deformation with a repeatable abrupt regime change.
  if(frame>0){
   for(int i=0;i<n;++i){
    auto& a=boxes[static_cast<std::size_t>(i)];
    const double dx=(frame%29==0?.13:.0005)*double((i%5)-2);
    a.min.x+=dx;a.max.x+=dx;
   }
  }
  ccp::cbp::Pairs result[3];
  auto measure=[&](int method){
   RegionRouter& router=method==0?brute:(method==1?indexed:certified);
   const auto start=Clock::now();
   result[method]=router.query(boxes,labels);
   const double elapsed=std::chrono::duration<double,std::milli>(Clock::now()-start).count();
   if(frame>=warmup){
    data[method].times.push_back(elapsed);
    const auto t=router.telemetry();
    data[method].crossTests+=t.crossTests;
    data[method].changedLeaves+=t.indexChangedLeaves;
    data[method].refits+=t.indexAncestorRefits;
    data[method].pairs=result[method].size();
   }
  };
  // All three methods have independent persistent state across ALL frames.
  // Cycle execution order to reduce method-order bias.
  for(int k=0;k<3;++k)measure((k+frame%3)%3);
  if(result[0]!=result[1]||result[0]!=result[2])
   throw std::runtime_error("Three-way exact candidate mismatch: "+name+" frame "+std::to_string(frame));
 }
 for(int mode=0;mode<3;++mode){
  const auto& s=data[mode];
  std::cout<<name<<','<<(mode==0?"exhaustive":mode==1?"shared_bvh":"certified_cache")<<','<<n<<','
           <<(frames-warmup)<<','<<std::fixed<<std::setprecision(6)
           <<avg(s.times)<<','<<p95(s.times)<<','<<s.crossTests<<','
           <<s.changedLeaves<<','<<s.refits<<','<<s.pairs<<'\n';
 }
 std::cerr<<"[CBP exploratory] "<<name
          <<" brute/shared_BVH="<<(avg(data[0].times)/avg(data[1].times))
          <<"x; brute/certified="<<(avg(data[0].times)/avg(data[2].times))
          <<"x; synthetic CI smoke, NOT publication evidence\n";
}
}
int main(int argc,char** argv){
 try{
  if(argc>3)throw std::invalid_argument("Usage: ccp_cbp_cross_bench [N=256] [frames=96]");
  const int n=argc>1?std::stoi(argv[1]):256;
  const int frames=argc>2?std::stoi(argv[2]):96;
  if(n<32||n>2048||frames<20||frames>360)
   throw std::invalid_argument("N must be 32..2048 and frames 20..360");
  std::cout<<"case,mode,N,frames,mean_ms,p95_ms,cross_primitive_tests,changed_leaves,ancestor_refits,last_pairs\n";
  for(const char* scenario:{"spaced","clustered","dense"})runCase(scenario,n,frames);
 }catch(const std::exception& e){
  std::cerr<<"CBP benchmark error: "<<e.what()<<'\n';
  return 1;
 }
}
