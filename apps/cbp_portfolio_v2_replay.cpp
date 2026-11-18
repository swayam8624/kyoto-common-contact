// Phase 10: measure a genuinely causal four-arm portfolio on immutable
// pre-recorded KYOTOM01 current-AABB scenes. This is NOT the offline
// per-frame-best method, not a coupled solver or a GPU implementation.
//
// The manager executes exactly ONE backend per frame. Only AFTER its
// timed query returns do we run the independent CPU oracle and verify
// the complete sorted-pair set. This deliberately prevents the oracle
// from warming or informing the controller's query.
// Experimental timing may still be affected by process/hardware noise;
// compare independent, repeated process runs, not claimed nanosecond gains.
#include "ccp/cbp_portfolio_v2.hpp"
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
#include <limits>
#include <stdexcept>
#include <string>
#include <sstream>
#include <vector>

namespace {
namespace fs=std::filesystem;
using ccp::cbp::Bounds;
using ccp::cbp::Pairs;
using ccp::cbp::CbpPortfolioV2;
using Clock=std::chrono::steady_clock;

struct Replay {
 std::ifstream file;
 std::uint32_t n{},frames{},seed{},material{};
 explicit Replay(const fs::path& path):file(path,std::ios::binary){
  if(!file)throw std::runtime_error("Cannot open snapshot");
  char magic[8]{};
  file.read(magic,8);
  if(!file||std::string(magic,8)!="KYOTOM01")
   throw std::runtime_error("Invalid binary signature");
  for(auto* word:{&n,&frames,&seed,&material})
   file.read(reinterpret_cast<char*>(word),4);
  if(!file||n==0||n>65536||frames<12||frames>240||material!=5)
   throw std::runtime_error("Invalid mixed-material dimensions or format");
  auto offset=file.tellg();
  file.seekg(0,std::ios::end);
  if(file.tellg()-offset!=std::streamoff(n)*std::streamoff(frames)*24)
   throw std::runtime_error("Source binary byte length mismatch");
  file.seekg(offset);
 }
 std::vector<Bounds> next(){
  std::vector<Bounds> boxes;
  boxes.reserve(n);
  for(std::uint32_t i=0;i<n;++i){
   std::array<float,6> f{};
   file.read(reinterpret_cast<char*>(f.data()),sizeof(f));
   if(!file)throw std::runtime_error("Truncated frame");
   for(const float x:f)
    if(!std::isfinite(x))throw std::runtime_error("Nonfinite scene bound");
   if(f[0]>f[3]||f[1]>f[4]||f[2]>f[5])
    throw std::runtime_error("Inverted scene bound");
   boxes.push_back({static_cast<int>(i),{f[0],f[1],f[2]},
                    {f[3],f[4],f[5]},1u,1u,true});
  }
  return boxes;
 }
};
std::vector<int> ownership(const fs::path& filename,std::size_t n){
 std::ifstream file(filename);
 if(!file)throw std::runtime_error("Missing immutable owner manifest");
 std::string row;
 if(!std::getline(file,row)||row!="id,kind,regime,first_primitive,primitive_count")
  throw std::runtime_error("Invalid owner header");
 std::vector<int> labels(n,-1);
 std::size_t next=0;int instances=0;
 while(std::getline(file,row)){
  if(row.empty())continue;
  std::stringstream line(row);
  std::array<std::string,5> fields{};
  for(auto& f:fields)if(!std::getline(line,f,','))
   throw std::runtime_error("Incomplete owner record");
  std::string extra;
  if(std::getline(line,extra,','))
   throw std::runtime_error("Unexpected owner record suffix");
  const int id=std::stoi(fields[0]),start=std::stoi(fields[3]),
            length=std::stoi(fields[4]);
  if(id!=instances||start<0||length<=0||
     std::size_t(start)!=next||std::size_t(length)>n-next)
   throw std::runtime_error("Corrupted ownership partition");
  for(int j=0;j<length;++j)labels[next+std::size_t(j)]=instances;
  ++instances;next+=std::size_t(length);
 }
 if(instances<12||instances>1000||next!=n)
  throw std::runtime_error("Ownership does not cover every stable ID");
 return labels;
}
std::uint64_t pairHash(const Pairs& pairs){
 std::uint64_t hash=UINT64_C(14695981039346656037);
 for(const auto& p:pairs)
  for(std::uint32_t id:{std::uint32_t(p.a),std::uint32_t(p.b)}){
   hash^=id;hash*=UINT64_C(1099511628211);
  }
 return hash;
}
double milliseconds(Clock::time_point start,Clock::time_point end){
 return std::chrono::duration<double,std::milli>(end-start).count();
}
}
int main(int argc,char** argv){
 try{
  if(argc!=4)throw std::invalid_argument(
    "Usage: ccp_cbp_portfolio_v2_replay snapshot.kmcb instances.csv OUT.csv");
  Replay input(argv[1]);
  const auto labels=ownership(argv[2],input.n);
  const fs::path destination=argv[3];
  if(fs::exists(destination)||fs::exists(destination.string()+".part"))
   throw std::runtime_error("Refusing to replace immutable 4-arm evidence");
  if(!destination.parent_path().empty())
   fs::create_directories(destination.parent_path());
  std::ofstream out(destination.string()+".part");
  if(!out)throw std::runtime_error("Cannot open result");
  out<<"frame,seed,n,mode,champion,query_ms,internal_total_ms,backend_ms,"
        "decision_ms,migration_ms,switched,probe,promoted,rejected,returning,"
        "trial_frames,trial_count,promotions,rejections,switch_count,"
        "champion_reference_ms,trial_mean_ms,champion_ewma_ms,"
        "pairs,inter_instance_pairs,pair_hash,oracle_pair_hash,exact_parity\n";
  out<<std::fixed<<std::setprecision(8);
  CbpPortfolioV2 manager;
  manager.setSapThreshold(48);
  std::size_t nontrivial=0;
  for(std::uint32_t frame=0;frame<input.frames;++frame){
   const auto boxes=input.next();
   const auto started=Clock::now();
   const auto actual=manager.query(boxes,labels);
   const auto elapsed=milliseconds(started,Clock::now());
   const auto d=manager.decision();
   // Timing has stopped. Oracle is NOT a side-channel into the manager.
   auto reference=ccp::live::candidates(boxes,true);
   if(actual!=reference)
    throw std::runtime_error("Four-arm full candidate-set PARITY FAILURE at frame "+
      std::to_string(frame)+" using "+CbpPortfolioV2::name(d.mode));
   const auto truthHash=pairHash(reference);
   const auto gotHash=pairHash(actual);
   if(input.n<=512 && frame%7==0 &&
      ccp::live::candidates(boxes,false)!=reference)
    throw std::runtime_error("Independent all-pairs oracle disagreed");
   std::size_t cross=0;
   for(const auto& p:reference)
    if(labels[std::size_t(p.a)]!=labels[std::size_t(p.b)])++cross;
   nontrivial+=cross;
   out<<frame<<','<<input.seed<<','<<input.n<<','
      <<CbpPortfolioV2::name(d.mode)<<','
      <<CbpPortfolioV2::name(d.champion)<<','
      <<elapsed<<','<<d.totalMs<<','<<d.backendMs<<','
      <<d.decisionMs<<','<<d.migrationMs<<','
      <<int(d.switched)<<','<<int(d.probe)<<','
      <<int(d.promoted)<<','<<int(d.rejected)<<','
      <<int(d.returning)<<','<<d.trialFrames<<','<<d.trials<<','
      <<d.promotions<<','<<d.rejections<<','<<d.switches<<','
      <<d.championReferenceMs<<','<<d.trialMeanMs<<','
      <<d.championEwmaMs<<','<<reference.size()<<','<<cross<<','
      <<gotHash<<','<<truthHash<<",verified\n";
   if((frame+1)%std::max(1u,input.frames/12u)==0)
    std::cerr<<"[FOUR ARM] frame "<<frame+1<<"/"<<input.frames
             <<" chosen "<<CbpPortfolioV2::name(d.mode)
             <<" champion "<<CbpPortfolioV2::name(d.champion)
             <<" q_ms "<<elapsed
             <<" cross_candidates "<<cross
             <<" exact_parity verified\n";
  }
  out.flush();
  if(!out)throw std::runtime_error("Incomplete manager evidence write");
  out.close();
  fs::rename(destination.string()+".part",destination);
  const auto& last=manager.decision();
  std::cout<<"PASS four-arm on-policy portfolio exact pairs: frames="
           <<input.frames<<" primitives="<<input.n
           <<" actual_trials="<<last.trials
           <<" promoted="<<last.promotions<<" rejected="<<last.rejections
           <<" switches="<<last.switches
           <<" total cross-asset candidate occurrences="<<nontrivial
           <<" (not resolved contacts)\n";
 }catch(const std::exception& e){
  std::cerr<<"CBP FOUR-ARM REPLAY FAILED: "<<e.what()<<'\n';
  return 1;
 }
}
