// Apple Metal GPU AABB overlap benchmark. Independent oracle/reference kernel.
// Deliberately O(N^2): this measures brute-force GPU reference throughput,
// NOT the online adaptive algorithm or a GPU-vs-CPU SAP speedup claim.
// Compile with CMake on macOS. Timing reports BOTH host submission/wait
// and Metal GPU execution; pair output is counts only, not materialized pairs.
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include <algorithm>
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
using Clock=std::chrono::steady_clock;
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
)METAL";
}
int main(int argc,char** argv){
 @autoreleasepool {
 try{
  if(argc!=7)throw std::runtime_error("usage: ccp_scaling_metal OUTPUT.csv N FRAMES SCENE SEED MODE[sap|exhaustive]");
  const size_t n=positive(argv[2],16384),frames=positive(argv[3],10000);
  const auto kind=std::string(argv[4]);const auto seed=unsigned(positive(argv[5],std::numeric_limits<unsigned>::max()));\n  const std::string mode=argv[6];\n  if(mode!="sap"&&mode!="exhaustive")throw std::runtime_error("invalid Metal mode");
  id<MTLDevice> device=MTLCreateSystemDefaultDevice();
  if(!device)throw std::runtime_error("no Metal GPU found");
  NSError* err=nil;
  NSString* code=[NSString stringWithUTF8String:metalSource];
  id<MTLLibrary> library=[device newLibraryWithSource:code options:nil error:&err];
  if(!library)throw std::runtime_error([[err localizedDescription] UTF8String]);
  id<MTLFunction> fn=[library newFunctionWithName:(mode=="sap"?@"sap_count":@"overlap_count")];
  id<MTLComputePipelineState> pipeline=[device newComputePipelineStateWithFunction:fn error:&err];
  if(!pipeline)throw std::runtime_error([[err localizedDescription] UTF8String]);
  id<MTLCommandQueue> queue=[device newCommandQueue];
  id<MTLBuffer> input=[device newBufferWithLength:n*sizeof(GPUBox) options:MTLResourceStorageModeShared];
  id<MTLBuffer> counts=[device newBufferWithLength:n*sizeof(uint32_t) options:MTLResourceStorageModeShared];
  if(!queue||!input||!counts)throw std::runtime_error("Metal resource allocation failed");
  std::filesystem::path path(argv[1]);
  if(!path.parent_path().empty())std::filesystem::create_directories(path.parent_path());
  std::ofstream out(path);if(!out)throw std::runtime_error("cannot open output CSV");
  out<<"backend,scene,n,frame,seed,device,host_ms,gpu_ms,overlaps,oracle_checked,misses\n";
  out<<std::fixed<<std::setprecision(8);
  for(size_t frame=0;frame<frames;++frame){
   auto boxes=scene(n,seed,int(frame),kind);
   std::copy(boxes.begin(),boxes.end(),static_cast<GPUBox*>([input contents]));
   const auto begin=Clock::now();
   id<MTLCommandBuffer> cmd=[queue commandBuffer];
   id<MTLComputeCommandEncoder> encoder=[cmd computeCommandEncoder];
   if(!cmd||!encoder)throw std::runtime_error("Metal command creation failed");
   [encoder setComputePipelineState:pipeline];
   [encoder setBuffer:input offset:0 atIndex:0];
   [encoder setBuffer:counts offset:0 atIndex:1];
   const uint32_t count=uint32_t(n);
   [encoder setBytes:&count length:sizeof(count) atIndex:2];
   const NSUInteger group=std::min(NSUInteger(pipeline.maxTotalThreadsPerThreadgroup),NSUInteger(256));
   [encoder dispatchThreads:MTLSizeMake(n,1,1) threadsPerThreadgroup:MTLSizeMake(group,1,1)];
   [encoder endEncoding];[cmd commit];[cmd waitUntilCompleted];
   const double host=std::chrono::duration<double,std::milli>(Clock::now()-begin).count();
   if(cmd.status!=MTLCommandBufferStatusCompleted)throw std::runtime_error("Metal command failed");
   auto* values=static_cast<const uint32_t*>([counts contents]);
   uint64_t total=0;for(size_t i=0;i<n;++i)total+=values[i];
   const bool checked=n<=2048&&(frame%5==0);
   if(checked&&total!=oracle(boxes))throw std::runtime_error("GPU overlap oracle mismatch");
   const double gpu=([cmd GPUEndTime]>=[cmd GPUStartTime])?
    ([cmd GPUEndTime]-[cmd GPUStartTime])*1000.0:-1.;
   out<<(mode=="sap"?"metal_sap_sorted":"metal_exhaustive_reference")<<","<<kind<<','<<n<<','<<frame<<','<<seed<<','
      <<'"'<<[[device name] UTF8String]<<'"'<<','<<host<<','<<gpu<<','<<total<<','
      <<(checked?1:0)<<",0\n";
  }
  if(!out)throw std::runtime_error("CSV write failure");
  std::cout<<"Metal reference run complete. Metal SAP includes host sorting and upload; adaptive GPU partitioning is NOT implemented.\n";
 }catch(const std::exception& e){std::cerr<<"METAL BENCHMARK FAILED: "<<e.what()<<'\n';return 1;}
 }
}
