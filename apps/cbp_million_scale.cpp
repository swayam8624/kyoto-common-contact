// Kyoto CBP scale gate: up to ONE MILLION actual moving AABB objects.
// They are synthetically generated 3D AABB bodies, NOT one million mass-
// spring Worlds, triangles or validated coupled collision responses.
//
// Spatial layout: separately indexed 8-body islands with proven nonoverlap
// between islands. Within each island enumerate every local candidate pair.
// The proof makes the local enumeration globally exact for THIS workload,
// and an independent O(N^2) oracle checks ALL pairs at N<=2048.
// No speedup claim against V4, GPU or general external scenes is implied.
//
// --virtual supports 1e9 only for analytical resource planning, not timing.
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
namespace fs=std::filesystem;
using Clock=std::chrono::steady_clock;
struct Box {float lo[3]{},hi[3]{};};
static_assert(sizeof(Box)==24);
struct Digest {
 std::uint64_t count{},hashXor{},hashSum{};
 void add(std::uint32_t a,std::uint32_t b){
  std::uint64_t v=(std::uint64_t(a)<<32)|b;
  v+=UINT64_C(0x9e3779b97f4a7c15);
  v=(v^(v>>30))*UINT64_C(0xbf58476d1ce4e5b9);
  v=(v^(v>>27))*UINT64_C(0x94d049bb133111eb);
  v^=v>>31;
  ++count;hashXor^=v;hashSum+=v;
 }
 bool operator==(const Digest&)const=default;
};
std::uint64_t number(const char* raw,std::uint64_t low,std::uint64_t high){
 std::string s(raw);std::size_t end=0;
 const auto n=std::stoull(s,&end);
 if(end!=s.size()||n<low||n>high)
  throw std::invalid_argument("Bounded input count exceeds allowed range");
 return n;
}
bool overlap(const Box& a,const Box& b){
 for(int k=0;k<3;++k)
  if(a.lo[k]>b.hi[k]||b.lo[k]>a.hi[k])return false;
 return true;
}
double duration(Clock::time_point t){
 return std::chrono::duration<double,std::milli>(Clock::now()-t).count();
}
void moveScene(std::vector<Box>& boxes,int frame,int scene,std::uint32_t seed){
 const std::size_t groups=(boxes.size()+7)/8;
 const std::size_t side=static_cast<std::size_t>(
  std::ceil(std::cbrt(double(groups))));
 for(std::size_t id=0;id<boxes.size();++id){
  const std::size_t group=id/8,member=id%8;
  const std::array<std::size_t,3> grid{
   group%side,(group/side)%side,group/(side*side)};
  const double oscillation=.085*std::sin(frame*.117+
                          double(group%29)*.24+double(seed%43)*.01);
  const double local=scene==0?.55:scene==1?.09:
       (.54-.46*(.5+.5*std::sin(frame*.13+double(group%7))));
  for(int axis=0;axis<3;++axis){
   const int bit=static_cast<int>((member>>axis)&1u);
   const float x=static_cast<float>(4.0*double(grid[axis])+
      double(bit)*local+oscillation+
      .004*std::sin(double(id%23)+axis*1.3+frame*.13));
   auto& box=boxes[id];
   box.lo[axis]=x;
   box.hi[axis]=static_cast<float>(x+.42+.08*double((id+axis)%3)/3.);
   const double anchor=4.0*double(grid[axis]);
   if(box.lo[axis]<anchor-.2||box.hi[axis]>anchor+1.6)
    throw std::runtime_error("Island separation proof invariant violated");
  }
 }
}
Digest localExact(const std::vector<Box>& boxes){
 Digest d;
 for(std::size_t first=0;first<boxes.size();first+=8)
  for(std::size_t a=first;a<std::min(boxes.size(),first+8);++a)
   for(std::size_t b=a+1;b<std::min(boxes.size(),first+8);++b)
    if(overlap(boxes[a],boxes[b]))
     d.add(static_cast<std::uint32_t>(a),static_cast<std::uint32_t>(b));
 return d;
}
Digest allPairsOracle(const std::vector<Box>& boxes){
 Digest d;
 for(std::size_t a=0;a<boxes.size();++a)
  for(std::size_t b=a+1;b<boxes.size();++b)
   if(overlap(boxes[a],boxes[b]))
    d.add(static_cast<std::uint32_t>(a),static_cast<std::uint32_t>(b));
 return d;
}
}
int main(int argc,char** argv){
 try{
  if(argc!=6&&argc!=7)
   throw std::invalid_argument(
    "Usage: ccp_cbp_million_scale OBJECTS FRAMES SEED "
    "SCENE[separated|dense|pulse] OUT.csv [--virtual]");
  const auto objects=number(argv[1],8,1000000000ULL);
  const auto frames=number(argv[2],1,240);
  const auto seed=number(argv[3],1,2147483647);
  const std::string scene=argv[4];
  const int sceneCode=scene=="separated"?0:scene=="dense"?1:
                     scene=="pulse"?2:-1;
  if(sceneCode<0)throw std::invalid_argument("Unknown island scene");
  const fs::path outPath=argv[5];
  const bool onlyEstimate=argc==7&&std::string(argv[6])=="--virtual";
  if(argc==7&&!onlyEstimate)
   throw std::invalid_argument("Only --virtual can follow the output path");
  if(objects>1000000&&!onlyEstimate)
   throw std::invalid_argument(
     "Refusing to materialize >1 million AABBs on this bounded CPU runner");
  if(fs::exists(outPath)||fs::exists(outPath.string()+".part"))
   throw std::runtime_error("Immutable evidence path already exists");
  if(!outPath.parent_path().empty())fs::create_directories(outPath.parent_path());
  std::ofstream out(outPath.string()+".part");
  if(!out)throw std::runtime_error("Could not create scale evidence");
  out<<std::fixed<<std::setprecision(5);
  if(onlyEstimate){
   // This is intentionally NOT a measured benchmark.
   const long double perFrame=static_cast<long double>(objects)*24.;
   const long double allFrames=perFrame*static_cast<long double>(frames);
   out<<"status,logical_AABBs,frames,bytes_one_AABB_frame,bytes_all_frames,verified\n";
   out<<"ANALYTICAL_ONLY,"<<objects<<','<<frames<<','
      <<static_cast<double>(perFrame)<<','
      <<static_cast<double>(allFrames)<<",NO\n";
   std::cout<<"ANALYTICAL ONLY: "<<objects
            <<" logical bounding boxes; 24-byte AABB array is "
            <<double(perFrame/1e9L)<<" GB per frame and "
            <<double(allFrames/1e12L)
            <<" TB if all "<<frames<<" frames are stored. "
            <<"NO objects simulated; NO runtime benchmark.\n";
  }else{
   out<<"frame,objects,islands,scene,generate_ms,query_ms,exact_pairs,"
         "commutative_hash_xor,commutative_hash_sum,"
         "global_oracle_checked,precision,bytes_resident\n";
   std::vector<Box> boxes(static_cast<std::size_t>(objects));
   const std::size_t islands=(boxes.size()+7)/8;
   double queryTotal=0;
   for(std::uint64_t frame=0;frame<frames;++frame){
    const auto start=Clock::now();
    moveScene(boxes,int(frame),sceneCode,static_cast<std::uint32_t>(seed));
    const double generation=duration(start);
    const auto queryBegin=Clock::now();
    const auto actual=localExact(boxes);
    const double queryMs=duration(queryBegin);
    if(objects<=2048 && actual!=allPairsOracle(boxes))
     throw std::runtime_error("Independent all-pairs oracle mismatch");
    const bool checked=objects<=2048;
    if(actual.count>28*islands)
     throw std::runtime_error("Inconsistent isolated-shard candidate count");
    queryTotal+=queryMs;
    out<<frame<<','<<objects<<','<<islands<<','<<scene<<','
       <<generation<<','<<queryMs<<','<<actual.count<<','
       <<actual.hashXor<<','<<actual.hashSum<<','
       <<(checked?1:0)<<",AABB_ISLANDS_EXACT,"
       <<sizeof(Box)*boxes.size()<<'\n';
    if((frame+1)%std::max<std::uint64_t>(1,frames/10)==0)
     std::cerr<<"[MILLION] "<<frame+1<<"/"<<frames
              <<" objects="<<objects
              <<" exact AABB candidates="<<actual.count
              <<" query_ms="<<queryMs
              <<(checked?" independently brute-verified":" proven separated islands")
              <<'\n';
   }
   std::cout<<"PASS million-scale exact synthetic sharded AABBs: N="
            <<objects<<" frames="<<frames
            <<" mean query ms="<<queryTotal/double(frames)
            <<" independent full oracle="<<(objects<=2048?"YES":"NO")
            <<". Not deformable mesh physics or a general broad phase.\n";
  }
  out.flush();
  if(!out)throw std::runtime_error("Could not flush experimental evidence");
  out.close();fs::rename(outPath.string()+".part",outPath);
 }catch(const std::exception& e){
  std::cerr<<"CBP SCALE GATE FAILED: "<<e.what()<<'\n';
  return 1;
 }
}
