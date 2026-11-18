// Extract a bounded, deterministic sample of REAL cross-instance AABB
// candidate overlap locations from previously recorded KYOTOM01 snapshots.
//
// The full exact sorted candidate list is re-derived once per saved frame
// using the SAME SAP reference input as the CPU/Metal replay. Every frame
// stores a canonical full-list FNV64 pair hash and its actual inter-instance
// pair count. The visualization verifies these receipts against its existing
// measured CPU SAP CSV *before* admitting spatial markers.
//
// Marker positions are centers of overlap of two ELEMENT AABBs. They are
// NOT triangle intersections, narrow-phase contacts or physics impulses.
// Bottom-K deterministic priority sampling avoids first-ID bias and keeps
// 1000-world visual files bounded, without inventing or dropping counts.
#include "ccp/final_broadphase.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <queue>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
namespace fs=std::filesystem;
using ccp::cbp::Bounds;
using ccp::cbp::Pairs;
struct Input {
 std::ifstream in;
 std::uint32_t n{},frames{},seed{},type{};
 explicit Input(const fs::path& file):in(file,std::ios::binary){
  if(!in)throw std::runtime_error("Cannot open material geometry");
  char magic[8]{};
  in.read(magic,8);
  if(!in||std::string(magic,8)!="KYOTOM01")
   throw std::runtime_error("Invalid KYOTOM01 binary signature");
  for(auto* dest:{&n,&frames,&seed,&type})
   in.read(reinterpret_cast<char*>(dest),4);
  if(!in||n==0||n>65536||frames<12||frames>240||type!=5)
   throw std::runtime_error("Unsupported shared mixed-material frame dimensions");
  const auto start=in.tellg();
  in.seekg(0,std::ios::end);
  if(in.tellg()-start!=std::streamoff(n)*std::streamoff(frames)*24)
   throw std::runtime_error("Invalid snapshot byte length");
  in.seekg(start);
 }
 std::vector<Bounds> read(){
  std::vector<Bounds> boxes;
  boxes.reserve(n);
  for(std::uint32_t i=0;i<n;++i){
   std::array<float,6> v{};
   in.read(reinterpret_cast<char*>(v.data()),sizeof(v));
   if(!in)throw std::runtime_error("Truncated material snapshot");
   for(float x:v)
    if(!std::isfinite(x))throw std::runtime_error("Non-finite material bound");
   if(v[0]>v[3]||v[1]>v[4]||v[2]>v[5])
    throw std::runtime_error("Inverted material bound");
   boxes.push_back({static_cast<int>(i),{v[0],v[1],v[2]},
                    {v[3],v[4],v[5]},1u,1u,true});
  }
  return boxes;
 }
};
std::vector<int> owners(const fs::path& file,std::uint32_t n){
 std::ifstream f(file);
 if(!f)throw std::runtime_error("Cannot open instance-ownership manifest");
 std::string header;
 if(!std::getline(f,header)||
    header!="id,kind,regime,first_primitive,primitive_count")
  throw std::runtime_error("Unexpected ownership CSV schema");
 std::vector<int> ids(n,-1);
 std::string line;
 int next=0,id=0;
 while(std::getline(f,line)){
  if(line.empty())continue;
  std::stringstream text(line);
  std::array<std::string,5> fields{};
  for(auto& field:fields)
   if(!std::getline(text,field,','))
    throw std::runtime_error("Missing ownership field");
  std::string extra;
  if(std::getline(text,extra,','))
   throw std::runtime_error("Unexpected ownership field");
  const int declared=std::stoi(fields[0]);
  const int first=std::stoi(fields[3]),size=std::stoi(fields[4]);
  if(declared!=id||first!=next||size<=0||
     first<0||size>static_cast<int>(n)-first)
   throw std::runtime_error("Instance ownership is not contiguous");
  for(int k=0;k<size;++k)ids[static_cast<std::size_t>(first+k)]=id;
  next+=size;++id;
 }
 if(next!=static_cast<int>(n)||id<12||id>1000)
  throw std::runtime_error("Instance ownership does not cover every primitive");
 return ids;
}
std::uint64_t mix64(std::uint64_t v){
 v+=UINT64_C(0x9e3779b97f4a7c15);
 v=(v^(v>>30))*UINT64_C(0xbf58476d1ce4e5b9);
 v=(v^(v>>27))*UINT64_C(0x94d049bb133111eb);
 return v^(v>>31);
}
struct Marker {
 std::uint64_t priority{};
 int ownerA{},ownerB{},primitiveA{},primitiveB{};
 double x{},y{},z{};
};
struct LessPriority {
 bool operator()(const Marker& a,const Marker& b)const{
  return a.priority<b.priority;
 }
};
std::uint64_t hashPairs(const Pairs& pairs){
 std::uint64_t h=UINT64_C(14695981039346656037);
 for(const auto& p:pairs)
  for(const std::uint32_t id:{static_cast<std::uint32_t>(p.a),
                              static_cast<std::uint32_t>(p.b)}){
   h^=id;
   h*=UINT64_C(1099511628211);
  }
 return h;
}
int safePositive(const char* value,int lo,int hi){
 const std::string text(value);
 std::size_t end=0;
 const auto i=std::stoll(text,&end);
 if(end!=text.size()||i<lo||i>hi)
  throw std::runtime_error("Marker sample limit outside safe range");
 return static_cast<int>(i);
}
}
int main(int argc,char** argv){
 try{
  if(argc<4||argc>5)
   throw std::invalid_argument(
    "Usage: ccp_cbp_contact_hotspots snapshots.kmcb instances.csv "
    "OUT.jsonl [marker_cap_per_frame=160]");
  const int cap=argc==5?safePositive(argv[4],1,512):160;
  const fs::path destination(argv[3]);
  if(fs::exists(destination))
   throw std::runtime_error("Existing spatial evidence must not be overwritten");
  Input source(argv[1]);
  const auto owner=owners(argv[2],source.n);
  if(!destination.parent_path().empty())
   fs::create_directories(destination.parent_path());
  const auto temp=fs::path(destination.string()+".part");
  if(fs::exists(temp))
   throw std::runtime_error("Incomplete evidence already exists");
  std::ofstream out(temp);
  if(!out)throw std::runtime_error("Cannot create spatial evidence");
  out<<std::fixed<<std::setprecision(7);
  std::uint64_t observedCross=0;
  for(std::uint32_t frame=0;frame<source.frames;++frame){
   const auto boxes=source.read();
   const auto candidates=ccp::live::candidates(boxes,true);
   const auto digest=hashPairs(candidates);
   std::priority_queue<Marker,std::vector<Marker>,LessPriority> top;
   std::uint64_t cross=0;
   for(const auto& p:candidates){
    const int wa=owner[static_cast<std::size_t>(p.a)];
    const int wb=owner[static_cast<std::size_t>(p.b)];
    if(wa==wb)continue;
    ++cross;
    const std::uint64_t key=(std::uint64_t(static_cast<std::uint32_t>(p.a))<<32)
                         |static_cast<std::uint32_t>(p.b);
    const auto priority=mix64(key^mix64(frame+UINT64_C(0xc0ffee)));
    if(top.size()>=static_cast<std::size_t>(cap)&&
       priority>=top.top().priority)
      continue;
    const auto& a=boxes[static_cast<std::size_t>(p.a)];
    const auto& b=boxes[static_cast<std::size_t>(p.b)];
    const auto mid=[](double amin,double amax,double bmin,double bmax){
     return .5*(std::max(amin,bmin)+std::min(amax,bmax));
    };
    Marker entry{priority,wa,wb,p.a,p.b,
     mid(a.min.x,a.max.x,b.min.x,b.max.x),
     mid(a.min.y,a.max.y,b.min.y,b.max.y),
     mid(a.min.z,a.max.z,b.min.z,b.max.z)};
    top.push(entry);
    if(top.size()>static_cast<std::size_t>(cap))top.pop();
   }
   observedCross+=cross;
   std::vector<Marker> selected;
   selected.reserve(top.size());
   while(!top.empty()){
    selected.push_back(top.top());
    top.pop();
   }
   std::sort(selected.begin(),selected.end(),[](const Marker& a,const Marker& b){
    if(a.priority!=b.priority)return a.priority<b.priority;
    return a.primitiveA<b.primitiveA;
   });
   // FNV64 is printed as decimal text to preserve 64 bits in JavaScript.
   out<<"{\"frame\":"<<frame
      <<",\"pairs\":"<<candidates.size()
      <<",\"inter_pairs\":"<<cross
      <<",\"pair_hash\":\""<<digest<<"\""
      <<",\"sampled\":"<<selected.size()
      <<",\"hotspots\":[";
   for(std::size_t i=0;i<selected.size();++i){
    const auto& m=selected[i];
    if(i)out<<',';
    out<<'['<<m.ownerA<<','<<m.ownerB<<','
       <<m.x<<','<<m.y<<','<<m.z<<','<<m.primitiveA<<','<<m.primitiveB<<']';
   }
   out<<"]}\n";
   if((frame+1)%std::max(1u,source.frames/12u)==0)
    std::cerr<<"[CBP spatial] frame "<<frame+1<<"/"<<source.frames
             <<" cross-instance exact AABB candidates "<<cross
             <<" drawn sample "<<selected.size()<<"\n";
  }
  out.flush();
  if(!out)throw std::runtime_error("Spatial evidence could not be flushed");
  out.close();
  fs::rename(temp,destination);
  std::cout<<"PASS spatial candidate sampling: frames="<<source.frames
           <<" primitives="<<source.n
           <<" total cross-instance candidate occurrences="<<observedCross
           <<" with complete framewise pair-hash receipts\n";
 }catch(const std::exception& e){
  std::cerr<<"CBP CONTACT HOTSPOTS FAILED: "<<e.what()<<"\n";
  return 1;
 }
}
