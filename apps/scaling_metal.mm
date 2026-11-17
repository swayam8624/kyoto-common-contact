// Apple Metal GPU AABB overlap benchmark. Independent oracle/reference kernel.
// GPU SAP sorts on host, then performs exact overlap count and deterministic
// GPU pair compaction. The exhaustive reference remains quadratic.
// Compile with CMake on macOS. Timing reports BOTH host submission/wait
// and Metal GPU execution; full pairs are materialized and validated.
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include <algorithm>
#include <array>
#include <memory>
#include <numeric>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>
namespace {
struct alignas(16) GPUBox {float lo[4],hi[4];};
// Match Metal Region's 48-byte layout: Box (32) + four uint32 fields.
struct alignas(16) GPURegion {
 GPUBox envelope;
 uint32_t begin{},end{},reserved0{},reserved1{};
};
static_assert(sizeof(GPURegion)==48);
struct GPUPair {uint32_t a,b; bool operator<(const GPUPair& q)const{return a<q.a||(a==q.a&&b<q.b);} bool operator==(const GPUPair& q)const{return a==q.a&&b==q.b;}};
using Clock=std::chrono::steady_clock;
class MaterialReplay {
 std::ifstream stream_;
public:
 uint32_t count{},frames{},seed{},material{};
 explicit MaterialReplay(const std::string& path):stream_(path,std::ios::binary){
  if(!stream_)throw std::runtime_error("cannot open material snapshot file");
  char magic[8]={};
  stream_.read(magic,8);
  if(std::string(magic,8)!="KYOTOM01")
   throw std::runtime_error("invalid material snapshot header; expected KYOTOM01");
  for(auto* value:{&count,&frames,&seed,&material})
   stream_.read(reinterpret_cast<char*>(value),sizeof(uint32_t));
  if(!stream_||!count||count>16384||!frames||frames>10000||material>5)
   throw std::runtime_error("unsupported material replay header dimensions");
  const auto start=stream_.tellg();
  stream_.seekg(0,std::ios::end);
  const auto bytes=stream_.tellg()-start;
  const auto expected=std::streamoff(count)*std::streamoff(frames)*std::streamoff(6*sizeof(float));
  if(bytes!=expected)throw std::runtime_error("material replay size mismatch");
  stream_.seekg(start);
 }
 std::vector<GPUBox> read(){
  std::vector<GPUBox> result;result.reserve(count);
  for(uint32_t i=0;i<count;++i){
   std::array<float,6> v{};
   stream_.read(reinterpret_cast<char*>(v.data()),sizeof(v));
   if(!stream_)throw std::runtime_error("incomplete material snapshot frame");
   for(auto x:v)if(!std::isfinite(x))throw std::runtime_error("non-finite replay material AABB");
   if(v[0]>v[3]||v[1]>v[4]||v[2]>v[5])
    throw std::runtime_error("inverted material replay box");
   result.push_back({{v[0],v[1],v[2],0},{v[3],v[4],v[5],0}});
  }
  return result;
 }
};

size_t positive(const char* raw,size_t maximum){
 const std::string s(raw);size_t consumed=0;unsigned long long v=0;
 try{v=std::stoull(s,&consumed);}catch(...){throw std::runtime_error("invalid integer argument");}
 if(consumed!=s.size()||!v||v>maximum)throw std::runtime_error("integer argument out of range");
 return size_t(v);
}
std::vector<GPUBox> scene(size_t n,unsigned seed,int frame,const std::string& kind){
 std::mt19937 rng(seed);std::uniform_real_distribution<float> rand(-1.f,1.f);
 std::vector<GPUBox> out;out.reserve(n);
 for(size_t i=0;i<n;++i){
  float x=rand(rng),y=rand(rng),z=rand(rng),half=.008f,scale=10.f;
  if(kind=="cluster"){scale=1.5f;half=.03f;}
  else if(kind=="dense"){scale=.6f;half=.07f;}
  else if(kind=="hotspot"){scale=6.f;half=.025f;if(i%5==0){x*=.05f;y*=.05f;z*=.05f;}}
  else if(kind=="all_overlap"){x=y=z=0;half=1;scale=1;}
  else if(kind!="sparse")throw std::runtime_error("invalid scene");
  float t=frame*.011f;
  if(kind!="all_overlap"){x+=.006f*std::sin(t+float(i%97));y+=.006f*std::cos(t*1.1f+float(i%83));}
  out.push_back({{x*scale-half,y*scale-half,z*scale-half,0},
                 {x*scale+half,y*scale+half,z*scale+half,0}});
 }
 return out;
}
uint64_t oracle(const std::vector<GPUBox>& boxes){
 uint64_t sum=0;
 for(size_t i=0;i<boxes.size();++i)for(size_t j=i+1;j<boxes.size();++j){
  const auto& a=boxes[i];const auto& b=boxes[j];
  bool match=true;
  for(int k=0;k<3;++k)if(a.lo[k]>b.hi[k]||b.lo[k]>a.hi[k]){match=false;break;}
  sum+=match;
 }
 return sum;
}
std::vector<GPUPair> oraclePairs(const std::vector<GPUBox>& boxes){
 std::vector<GPUPair> out;
 for(uint32_t i=0;i<boxes.size();++i)for(uint32_t j=i+1;j<boxes.size();++j){
  const auto& a=boxes[i];const auto& b=boxes[j];
  bool match=true;
  for(int k=0;k<3;++k)if(a.lo[k]>b.hi[k]||b.lo[k]>a.hi[k]){match=false;break;}
  if(match)out.push_back({i,j});
 }
 return out;
}
uint64_t pairHash(const std::vector<GPUPair>& pairs){
 uint64_t h=UINT64_C(14695981039346656037);
 for(const auto& p:pairs)
  for(uint32_t value:{p.a,p.b}){
   h^=uint64_t(value);h*=UINT64_C(1099511628211);
  }
 return h;
}
const char* metalSource=R"METAL(
#include <metal_stdlib>
using namespace metal;
struct Box {float4 lo;float4 hi;};
kernel void overlap_count(
  device const Box* boxes [[buffer(0)]],
  device uint* counts [[buffer(1)]],
  constant uint& n [[buffer(2)]],
  uint i [[thread_position_in_grid]]) {
 if(i>=n)return;
 Box a=boxes[i];uint found=0;
 // Every unordered pair is visited once, avoiding global atomics.
 for(uint j=i+1;j<n;++j) {
  Box b=boxes[j];
  if(all(a.lo.xyz<=b.hi.xyz)&&all(b.lo.xyz<=a.hi.xyz))++found;
 }
 counts[i]=found;
}

kernel void sap_count(
  device const Box* boxes [[buffer(0)]],
  device uint* counts [[buffer(1)]],
  constant uint& n [[buffer(2)]],
  uint i [[thread_position_in_grid]]) {
 if(i>=n)return;
 Box a=boxes[i];uint found=0;
 for(uint j=i+1;j<n;++j) {
  Box b=boxes[j];
  if(b.lo.x>a.hi.x)break;
  if(all(a.lo.xyz<=b.hi.xyz)&&all(b.lo.xyz<=a.hi.xyz))++found;
 }
 counts[i]=found;
}

struct Pair {uint a,b;};
// Input prefix offsets are exclusive scans of counts from the first pass.
// Each thread writes only its own disjoint segment, avoiding atomics.
kernel void sap_pairs(
 device const Box* boxes [[buffer(0)]],
 device Pair* output [[buffer(1)]],
 constant uint& n [[buffer(2)]],
 device const uint* offsets [[buffer(3)]],
 uint i [[thread_position_in_grid]]) {
 if(i>=n)return;
 Box a=boxes[i];uint offset=offsets[i];
 for(uint j=i+1;j<n;++j){
  Box b=boxes[j];
  if(b.lo.x>a.hi.x)break;
  if(all(a.lo.xyz<=b.hi.xyz)&&all(b.lo.xyz<=a.hi.xyz)){
   output[offset]=Pair{i,j};++offset;
  }
 }
}
kernel void exhaustive_pairs(
 device const Box* boxes [[buffer(0)]],
 device Pair* output [[buffer(1)]],
 constant uint& n [[buffer(2)]],
 device const uint* offsets [[buffer(3)]],
 uint i [[thread_position_in_grid]]) {
 if(i>=n)return;
 Box a=boxes[i];uint offset=offsets[i];
 for(uint j=i+1;j<n;++j){
  Box b=boxes[j];
  if(all(a.lo.xyz<=b.hi.xyz)&&all(b.lo.xyz<=a.hi.xyz)){
   output[offset]=Pair{i,j};++offset;
  }
 }
}

struct Region {Box box;uint start,end,pad0,pad1;};
kernel void region_count(
  device const Box* boxes [[buffer(0)]],
  device uint* counts [[buffer(1)]],
  constant uint& n [[buffer(2)]],
  device const Region* regions [[buffer(3)]],
  constant uint& regionCount [[buffer(4)]],
  uint i [[thread_position_in_grid]]) {
 if(i>=n)return;
 const Box a=boxes[i];uint found=0;
 // Regions are sorted by the minimum X of the conservative union envelope.
 // Every actual overlap must occur in an intersecting region envelope.
 for(uint r=0;r<regionCount;++r){
  const Region reg=regions[r];
  if(reg.box.lo.x>a.hi.x)break;
  if(!all(a.lo.xyz<=reg.box.hi.xyz)||!all(reg.box.lo.xyz<=a.hi.xyz))continue;
  for(uint j=reg.start;j<reg.end;++j){
   if(j<=i)continue;
   Box b=boxes[j];
   if(all(a.lo.xyz<=b.hi.xyz)&&all(b.lo.xyz<=a.hi.xyz))++found;
  }
 }
 counts[i]=found;
}
kernel void region_pairs(
  device const Box* boxes [[buffer(0)]],
  device Pair* output [[buffer(1)]],
  constant uint& n [[buffer(2)]],
  device const uint* offsets [[buffer(3)]],
  device const Region* regions [[buffer(4)]],
  constant uint& regionCount [[buffer(5)]],
  uint i [[thread_position_in_grid]]) {
 if(i>=n)return;
 const Box a=boxes[i];uint cursor=offsets[i];
 for(uint r=0;r<regionCount;++r){
  const Region reg=regions[r];
  if(reg.box.lo.x>a.hi.x)break;
  if(!all(a.lo.xyz<=reg.box.hi.xyz)||!all(reg.box.lo.xyz<=a.hi.xyz))continue;
  for(uint j=reg.start;j<reg.end;++j){
   if(j<=i)continue;
   Box b=boxes[j];
   if(all(a.lo.xyz<=b.hi.xyz)&&all(b.lo.xyz<=a.hi.xyz)){
    output[cursor]=Pair{i,j};++cursor;
   }
  }
 }
}
)METAL";
}
int main(int argc,char** argv){
 @autoreleasepool {
 try{
  const bool replay=argc==5&&std::string(argv[1])=="--replay";
  if(!replay&&argc!=7)
   throw std::runtime_error("usage: ccp_scaling_metal OUTPUT.csv N FRAMES SCENE SEED MODE[sap|exhaustive|region|online] OR --replay MATERIAL.kmcb OUTPUT.csv MODE[sap|exhaustive|region|online]");
  std::unique_ptr<MaterialReplay> material;
  if(replay)material=std::make_unique<MaterialReplay>(argv[2]);
  const size_t n=replay?material->count:positive(argv[2],16384);
  const size_t frames=replay?material->frames:positive(argv[3],10000);
  const auto kind=replay?std::string("material_replay_")+std::to_string(material->material):std::string(argv[4]);
  const unsigned seed=replay?material->seed:unsigned(positive(argv[5],std::numeric_limits<unsigned>::max()));
  const std::string mode=replay?argv[4]:argv[6];
  if(mode!="sap"&&mode!="exhaustive"&&mode!="region"&&mode!="online")
   throw std::runtime_error("invalid Metal mode");
  id<MTLDevice> device=MTLCreateSystemDefaultDevice();
  if(!device)throw std::runtime_error("no Metal GPU found");
  NSError* err=nil;
  NSString* code=[NSString stringWithUTF8String:metalSource];
  id<MTLLibrary> library=[device newLibraryWithSource:code options:nil error:&err];
  if(!library)throw std::runtime_error([[err localizedDescription] UTF8String]);
  const bool regional=(mode=="region"||mode=="online");
  id<MTLFunction> fn=[library newFunctionWithName:regional?@"region_count":
                         mode=="sap"?@"sap_count":@"overlap_count"];
  id<MTLComputePipelineState> pipeline=[device newComputePipelineStateWithFunction:fn error:&err];
  id<MTLFunction> pairFn=[library newFunctionWithName:regional?@"region_pairs":
                             mode=="sap"?@"sap_pairs":@"exhaustive_pairs"];
  id<MTLComputePipelineState> pairPipeline=[device newComputePipelineStateWithFunction:pairFn error:&err];
  if(!pipeline||!pairPipeline)throw std::runtime_error([[err localizedDescription] UTF8String]);
  id<MTLCommandQueue> queue=[device newCommandQueue];
  id<MTLBuffer> input=[device newBufferWithLength:n*sizeof(GPUBox) options:MTLResourceStorageModeShared];
  id<MTLBuffer> counts=[device newBufferWithLength:n*sizeof(uint32_t) options:MTLResourceStorageModeShared];
  id<MTLBuffer> offsets=[device newBufferWithLength:n*sizeof(uint32_t) options:MTLResourceStorageModeShared];
  id<MTLBuffer> regionData=[device newBufferWithLength:((n+15)/16)*sizeof(GPURegion) options:MTLResourceStorageModeShared];
  if(!queue||!input||!counts||!offsets||!regionData)throw std::runtime_error("Metal resource allocation failed");
  std::filesystem::path path(replay?argv[3]:argv[1]);
  if(!path.parent_path().empty())std::filesystem::create_directories(path.parent_path());
  if(std::filesystem::exists(path))throw std::runtime_error("refusing to overwrite GPU evidence CSV");
  std::ofstream out(path);if(!out)throw std::runtime_error("cannot open output CSV");
  out<<"backend,scene,n,frame,seed,device,host_ms,gpu_ms,overlaps,oracle_checked,misses,pair_bytes,pair_hash,region_span,region_count,online_changed\n";
  out<<std::fixed<<std::setprecision(8);
  uint32_t currentSpan=64;
  int lastOnlineEdit=-1000;
  for(size_t frame=0;frame<frames;++frame){
   auto boxes=replay?material->read():scene(n,seed,int(frame),kind);
   const auto originalBoxes=boxes;
   std::vector<uint32_t> originalIDs(n);
   std::iota(originalIDs.begin(),originalIDs.end(),0u);
   const auto begin=Clock::now();
   if(mode=="sap"){
    std::sort(originalIDs.begin(),originalIDs.end(),[&](uint32_t a,uint32_t b){
     return boxes[a].lo[0]<boxes[b].lo[0]||
       (boxes[a].lo[0]==boxes[b].lo[0]&&a<b);
    });
    std::vector<GPUBox> ordered;ordered.reserve(n);
    for(const auto index:originalIDs)ordered.push_back(boxes[index]);
    boxes=std::move(ordered);
   }
   std::copy(boxes.begin(),boxes.end(),static_cast<GPUBox*>([input contents]));
   uint32_t regionCount=0;
   if(regional){
    std::vector<GPURegion> unions;
    unions.reserve((n+currentSpan-1)/currentSpan);
    for(size_t first=0;first<n;first+=currentSpan){
     const size_t stop=std::min(n,first+currentSpan);
     GPURegion reg{};
     reg.begin=uint32_t(first);reg.end=uint32_t(stop);
     reg.envelope=boxes[first];
     for(size_t k=first+1;k<stop;++k)
      for(int axis=0;axis<3;++axis){
       reg.envelope.lo[axis]=std::min(reg.envelope.lo[axis],boxes[k].lo[axis]);
       reg.envelope.hi[axis]=std::max(reg.envelope.hi[axis],boxes[k].hi[axis]);
      }
     unions.push_back(reg);
    }
    std::sort(unions.begin(),unions.end(),[](const GPURegion& a,const GPURegion& b){
     return a.envelope.lo[0]<b.envelope.lo[0]||
       (a.envelope.lo[0]==b.envelope.lo[0]&&a.begin<b.begin);
    });
    regionCount=uint32_t(unions.size());
    if(regionCount>(n+15)/16)throw std::runtime_error("region allocation overflow");
    std::copy(unions.begin(),unions.end(),static_cast<GPURegion*>([regionData contents]));
   }
   id<MTLCommandBuffer> cmd=[queue commandBuffer];
   id<MTLComputeCommandEncoder> encoder=[cmd computeCommandEncoder];
   if(!cmd||!encoder)throw std::runtime_error("Metal command creation failed");
   [encoder setComputePipelineState:pipeline];
   [encoder setBuffer:input offset:0 atIndex:0];
   [encoder setBuffer:counts offset:0 atIndex:1];
   const uint32_t count=uint32_t(n);
   [encoder setBytes:&count length:sizeof(count) atIndex:2];
   if(regional){
    [encoder setBuffer:regionData offset:0 atIndex:3];
    [encoder setBytes:&regionCount length:sizeof(regionCount) atIndex:4];
   }
   const NSUInteger group=std::min(NSUInteger(pipeline.maxTotalThreadsPerThreadgroup),NSUInteger(256));
   [encoder dispatchThreads:MTLSizeMake(n,1,1) threadsPerThreadgroup:MTLSizeMake(group,1,1)];
   [encoder endEncoding];[cmd commit];[cmd waitUntilCompleted];
   if(cmd.status!=MTLCommandBufferStatusCompleted)throw std::runtime_error("Metal count pass failed");
   auto* values=static_cast<const uint32_t*>([counts contents]);
   // Prefix sum creates collision-free output ranges for every GPU thread.
   // Refuse huge outputs instead of risking an OOM on all-overlap scenes.
   uint64_t total=0;
   auto* prefix=static_cast<uint32_t*>([offsets contents]);
   for(size_t i=0;i<n;++i){
    if(total>16000000ull)throw std::runtime_error("GPU candidate pair safety cap exceeded");
    prefix[i]=uint32_t(total);
    total+=values[i];
   }
   if(total>16000000ull)throw std::runtime_error("GPU candidate pair safety cap exceeded");
   id<MTLBuffer> pairs=[device newBufferWithLength:std::max(size_t(1),size_t(total))*sizeof(GPUPair)
                                         options:MTLResourceStorageModeShared];
   if(!pairs)throw std::runtime_error("GPU pair output allocation failed");
   id<MTLCommandBuffer> pairCmd=[queue commandBuffer];
   id<MTLComputeCommandEncoder> pairEncoder=[pairCmd computeCommandEncoder];
   if(!pairCmd||!pairEncoder)throw std::runtime_error("GPU pair command creation failed");
   [pairEncoder setComputePipelineState:pairPipeline];
   [pairEncoder setBuffer:input offset:0 atIndex:0];
   [pairEncoder setBuffer:pairs offset:0 atIndex:1];
   [pairEncoder setBytes:&count length:sizeof(count) atIndex:2];
   [pairEncoder setBuffer:offsets offset:0 atIndex:3];
   if(regional){
    [pairEncoder setBuffer:regionData offset:0 atIndex:4];
    [pairEncoder setBytes:&regionCount length:sizeof(regionCount) atIndex:5];
   }
   const NSUInteger pairGroup=std::min(NSUInteger(pairPipeline.maxTotalThreadsPerThreadgroup),NSUInteger(256));
   [pairEncoder dispatchThreads:MTLSizeMake(n,1,1) threadsPerThreadgroup:MTLSizeMake(pairGroup,1,1)];
   [pairEncoder endEncoding];[pairCmd commit];[pairCmd waitUntilCompleted];
   if(pairCmd.status!=MTLCommandBufferStatusCompleted)throw std::runtime_error("GPU pair materialization failed");
   // Read the entire pair set; sorted output has deterministic unique owners.
   const auto* gpuPairs=static_cast<const GPUPair*>([pairs contents]);
   std::vector<GPUPair> actual(gpuPairs,gpuPairs+total);
   // Translate sorted GPU SAP indices back to STABLE material element IDs.
   // This makes hashes comparable across CPU and GPU on the same binary file.
   for(auto& p:actual){
    const auto a=originalIDs[p.a],b=originalIDs[p.b];
    p={std::min(a,b),std::max(a,b)};
   }
   std::sort(actual.begin(),actual.end());
   if(std::adjacent_find(actual.begin(),actual.end())!=actual.end())
    throw std::runtime_error("GPU emitted duplicate candidate IDs");
   const double gpu=([cmd GPUEndTime]>=[cmd GPUStartTime]&&[pairCmd GPUEndTime]>=[pairCmd GPUStartTime])?
    ([cmd GPUEndTime]-[cmd GPUStartTime]+[pairCmd GPUEndTime]-[pairCmd GPUStartTime])*1000.0:-1.;
   const double host=std::chrono::duration<double,std::milli>(Clock::now()-begin).count();
   const auto hash=pairHash(actual);
   const bool checked=n<=2048&&(frame%5==0);
   if(checked&&actual!=oraclePairs(originalBoxes))
    throw std::runtime_error("GPU stable-ID pair set differs from independent CPU exhaustive oracle");
   // GPU online regional control sees ONLY past observed candidate density,
   // and pays future region-build costs. This is a candidate-work proxy,
   // not the original CPU cloth controller's confirmed physical contacts.
   bool changed=false;
   const uint32_t measuredSpan=currentSpan;
   if(mode=="online"&&int(frame)-lastOnlineEdit>=24&&frame%12==11){
    const double density=double(total)/double(n);
    uint32_t desired=currentSpan;
    if(density>20. && currentSpan>16)desired=currentSpan/2;
    else if(density<3.&&currentSpan<256)desired=currentSpan*2;
    if(desired!=currentSpan){
     currentSpan=desired;changed=true;lastOnlineEdit=int(frame);
    }
   }
   out<<(mode=="sap"?"metal_sap_sorted":mode=="exhaustive"?"metal_exhaustive_reference":
          mode=="region"?"metal_region_fixed":"metal_region_online")
      <<","<<kind<<','<<n<<','<<frame<<','<<seed<<','
      <<'"'<<[[device name] UTF8String]<<'"'<<','<<host<<','<<gpu<<','<<total<<','
      <<(checked?1:0)<<",0,"<<total*sizeof(GPUPair)<<","<<hash<<","<<measuredSpan<<","<<regionCount<<","<<(changed?1:0)<<"\n";
  }
  if(!out)throw std::runtime_error("CSV write failure");
  std::cout<<"Metal benchmark materializes exact pairs for SAP, region-pruned and an experimental online span controller. The ORIGINAL cloth-specific online controller is NOT on the GPU.\n";
 }catch(const std::exception& e){std::cerr<<"METAL BENCHMARK FAILED: "<<e.what()<<'\n';return 1;}
 }
}
