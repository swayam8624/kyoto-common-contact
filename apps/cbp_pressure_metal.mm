// Phase 11: REAL Apple Metal GPU global AABB pressure, COUNT-ONLY.
//
// Distinct GPU algorithms:
//  - grid: exact 3D spatial-hash cell canonical-pair ownership.
//  - exhaustive: O(N^2) unordered pairs, parallelized per source object.
//
// Both use the exact SAME deterministic Float32 geometry source as the CPU
// cross-method stress harness. No dense pair-list allocation: we explicitly
// measure search/count effort; a separate output-capacity stage must measure
// pair materialization. CPU/GPU count+two commutative 32-bit pair fingerprints
// are checked. This is not a mathematical full-set proof: small N separately
// receives a brute CPU oracle, and all-overlap has known combinatorial count.
// Explicit budget SKIP/FAIL never receives a "success" performance value.
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include "ccp/cbp_pressure.hpp"
#include <algorithm>
#include <chrono>
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
namespace fs=std::filesystem;
using namespace ccp::pressure;
using Clock=std::chrono::steady_clock;
struct alignas(16) GPUCell {
 std::int32_t x{},y{},z{};
 std::uint32_t begin{},count{},pad0{},pad1{},pad2{};
};
struct GPUDigest {
 std::uint32_t count{},sum32{},xor32{},tests{};
};
static_assert(sizeof(Box)==32);
static_assert(sizeof(GPUCell)==32);
static_assert(sizeof(GPUDigest)==16);
constexpr char source[]=R"METAL(
#include <metal_stdlib>
using namespace metal;
struct Box {float4 lo,hi;};
struct Cell {
 int x,y,z;
 uint begin,count,pad0,pad1,pad2;
};
struct Result {uint count,sum32,xor32,tests;};
inline bool crosses(Box a,Box b){
 return all(a.lo.xyz<=b.hi.xyz)&&all(b.lo.xyz<=a.hi.xyz);
}
inline uint fingerprint(uint a,uint b){
 uint x=a*0x9e3779b9u+b*0x85ebca6bu+0x165667b1u;
 x^=x>>16;x*=0x7feb352du;
 x^=x>>15;x*=0x846ca68bu;
 return x^(x>>16);
}
kernel void grid_count(
 device const Box* boxes [[buffer(0)]],
 device const uint* owners [[buffer(1)]],
 device const Cell* cells [[buffer(2)]],
 device Result* results [[buffer(3)]],
 constant uint& cellN [[buffer(4)]],
 uint ci [[thread_position_in_grid]]){
 if(ci>=cellN)return;
 const Cell c=cells[ci];uint found=0,sum=0,xorHash=0,tested=0;
 for(uint i=0;i<c.count;++i){
  const uint aID=owners[c.begin+i];const Box a=boxes[aID];
  for(uint j=i+1;j<c.count;++j){
   const uint bID=owners[c.begin+j];const Box b=boxes[bID];
   ++tested;
   if(!crosses(a,b))continue;
   const float3 common=max(a.lo.xyz,b.lo.xyz);
   const int3 home=int3(floor(common));
   if(home.x!=c.x||home.y!=c.y||home.z!=c.z)continue;
   const uint first=min(aID,bID),second=max(aID,bID);
   const uint h=fingerprint(first,second);
   ++found;sum+=h;xorHash^=h;
  }
 }
 results[ci]={found,sum,xorHash,tested};
}
kernel void brute_count(
 device const Box* boxes [[buffer(0)]],
 device Result* results [[buffer(1)]],
 constant uint& n [[buffer(2)]],
 uint i [[thread_position_in_grid]]){
 if(i>=n)return;
 const Box a=boxes[i];uint found=0,sum=0,xorHash=0,tested=0;
 for(uint j=i+1;j<n;++j){
  ++tested;
  if(crosses(a,boxes[j])){
   const uint h=fingerprint(i,j);
   ++found;sum+=h;xorHash^=h;
  }
 }
 results[i]={found,sum,xorHash,tested};
}
)METAL";

double milliseconds(Clock::time_point begin){
 return std::chrono::duration<double,std::milli>(Clock::now()-begin).count();
}
void require(bool ok,const char* msg){
 if(!ok)throw std::runtime_error(msg);
}
}
int main(int argc,char** argv){
 @autoreleasepool {
 try{
  if(argc!=11)throw std::invalid_argument(
    "Usage: ccp_cbp_pressure_metal OUT.csv SCENE N FRAMES SEED "
    "MODE[grid|exhaustive] MAX_PAIRS_TESTED MAX_FRAME_MS "
    "MAX_GRID_ENTRIES MAX_GPU_MIB");
  const fs::path output=argv[1];
  const std::string scenario=argv[2],mode=argv[6];
  const auto n=number(argv[3],16,1000000),frames=number(argv[4],1,240),
             seed=number(argv[5],1,2147483647);
  const auto maxTests=number(argv[7],1000,600000000);
  const auto maxFrameMs=number(argv[8],20,120000);
  const auto maxEntries=number(argv[9],10000,20000000);
  const auto maxMiB=number(argv[10],32,4096);
  if(!known(scenario)||!(mode=="grid"||mode=="exhaustive"))
   throw std::invalid_argument("Unknown scene or GPU algorithm");
  if(mode=="exhaustive"&&n>32768)
   throw std::invalid_argument("Full GPU O(N^2) reference capped at 32768 AABBs");
  id<MTLDevice> device=MTLCreateSystemDefaultDevice();
  require(device!=nil,"Apple Metal device unavailable");
  NSError* error=nil;
  NSString* program=[NSString stringWithUTF8String:source];
  id<MTLLibrary> library=[device newLibraryWithSource:program
    options:nil error:&error];
  if(!library)throw std::runtime_error([[error localizedDescription] UTF8String]);
  id<MTLFunction> fn=[library newFunctionWithName:
                         mode=="grid"?@"grid_count":@"brute_count"];
  id<MTLComputePipelineState> pipeline=[device newComputePipelineStateWithFunction:fn error:&error];
  if(!pipeline)throw std::runtime_error([[error localizedDescription] UTF8String]);
  id<MTLCommandQueue> queue=[device newCommandQueue];
  require(queue!=nil,"Metal command queue unavailable");
  if(fs::exists(output)||fs::exists(output.string()+".part"))
   throw std::runtime_error("Refusing to overwrite GPU research evidence");
  if(!output.parent_path().empty())fs::create_directories(output.parent_path());
  std::ofstream csv(output.string()+".part");
  require(bool(csv),"Cannot create GPU evidence file");
  csv<<std::fixed<<std::setprecision(6);
  csv<<"scene,mode,n,frame,seed,gpu_name,status,generate_ms,grid_build_ms,"
       "cpu_oracle_ms,gpu_prepare_ms,host_submit_wait_ms,gpu_execution_ms,"
       "GPU_tested_pairs,CPU_3D_grid_test_upper,GPU_candidate_count,"
       "CPU_candidate_count,GPU_sum32,CPU_sum32,GPU_xor32,CPU_xor32,"
       "validation,grid_cells,grid_entries,output_bytes,estimated_input_bytes\n";
  for(std::uint64_t frame=0;frame<frames;++frame){
   const auto started=Clock::now();
   const auto boxes=generate(std::size_t(n),std::uint32_t(frame),
                   std::uint32_t(frames),std::uint32_t(seed),scenario);
   const double generation=milliseconds(started);
   const auto gStart=Clock::now();
   const auto grid=buildGrid(boxes,std::size_t(maxEntries));
   const double build=milliseconds(gStart);
   const auto cpuStart=Clock::now();
   const auto cpu=queryGrid(boxes,grid,maxTests,double(maxFrameMs));
   const double cpuMs=milliseconds(cpuStart);
   if(n<=1024&&cpu.complete&&cpu.digest!=brute(boxes))
    throw std::runtime_error("Independent brute oracle failed for CPU grid");
   const std::uint64_t bruteTests=n*(n-1)/2;
   const std::uint64_t upper=mode=="grid"?grid.workUpperBound:bruteTests;
   if(upper>maxTests){
    csv<<scenario<<','<<mode<<','<<n<<','<<frame<<','<<seed<<','
       <<'"'<<[[device name] UTF8String]<<'"'
       <<",BUDGET_SKIP,"<<generation<<','<<build<<','<<cpuMs
       <<",0,0,0,0,"<<upper<<",0,"<<cpu.digest.count
       <<",0,"<<cpu.digest.sum32<<",0,"<<cpu.digest.xor32
       <<",not_executed,"<<grid.bins.size()<<','<<grid.entries
       <<",0,0\n";
    std::cerr<<"[METAL PRESSURE] BUDGET_SKIP "<<scenario<<" frame "<<frame+1
             <<" required upper bound "<<upper<<", configured "<<maxTests<<"\n";
    continue;
   }
   // Protect allocations before creating any GPU buffers. This is not a
   // physical RAM allocation estimate for the CPU hash table or OS.
   const auto cells=mode=="grid"?grid.bins.size():0;
   const std::uint64_t gpuBytes=n*sizeof(Box)+grid.entries*4+
       cells*sizeof(GPUCell)+
       (mode=="grid"?cells:n)*sizeof(GPUDigest);
   if(gpuBytes>(maxMiB<<20))
    throw std::runtime_error("GPU input/output buffer allocation budget exceeded");
   const auto staging=Clock::now();
   id<MTLBuffer> input=[device newBufferWithLength:n*sizeof(Box)
     options:MTLResourceStorageModeShared];
   require(input!=nil,"Metal input allocation failure");
   std::copy(boxes.begin(),boxes.end(),static_cast<Box*>([input contents]));
   id<MTLBuffer> ownerBuffer=nil,cellBuffer=nil;
   if(mode=="grid"){
    std::vector<std::uint32_t> ids;
    ids.reserve(std::size_t(grid.entries));
    std::vector<GPUCell> cellData;
    cellData.reserve(cells);
    for(const auto& bin:grid.bins){
     if(bin.ids.size()>65535)
      throw std::runtime_error("Single-cell occupancy >65535: use GPU exhaustive or subdivide");
     const std::uint32_t at=std::uint32_t(ids.size());
     ids.insert(ids.end(),bin.ids.begin(),bin.ids.end());
     cellData.push_back({bin.cell.x,bin.cell.y,bin.cell.z,at,
                         std::uint32_t(bin.ids.size()),0,0,0});
    }
    ownerBuffer=[device newBufferWithLength:std::max(std::size_t(1),ids.size())
                    *sizeof(std::uint32_t) options:MTLResourceStorageModeShared];
    cellBuffer=[device newBufferWithLength:std::max(std::size_t(1),cells)
                    *sizeof(GPUCell) options:MTLResourceStorageModeShared];
    require(ownerBuffer!=nil&&cellBuffer!=nil,"Metal hashed-cell input allocation failure");
    std::copy(ids.begin(),ids.end(),static_cast<std::uint32_t*>([ownerBuffer contents]));
    std::copy(cellData.begin(),cellData.end(),static_cast<GPUCell*>([cellBuffer contents]));
   }
   const std::size_t resultsN=mode=="grid"?cells:std::size_t(n);
   id<MTLBuffer> results=[device newBufferWithLength:
     std::max(std::size_t(1),resultsN)*sizeof(GPUDigest)
     options:MTLResourceStorageModeShared];
   require(results!=nil,"Metal output allocation failure");
   const double prepared=milliseconds(staging);
   id<MTLCommandBuffer> cmd=[queue commandBuffer];
   id<MTLComputeCommandEncoder> encoder=[cmd computeCommandEncoder];
   require(cmd!=nil&&encoder!=nil,"Metal command encoder creation failure");
   [encoder setComputePipelineState:pipeline];
   [encoder setBuffer:input offset:0 atIndex:0];
   if(mode=="grid"){
    [encoder setBuffer:ownerBuffer offset:0 atIndex:1];
    [encoder setBuffer:cellBuffer offset:0 atIndex:2];
    [encoder setBuffer:results offset:0 atIndex:3];
    const auto length=std::uint32_t(resultsN);
    [encoder setBytes:&length length:sizeof(length) atIndex:4];
   }else{
    [encoder setBuffer:results offset:0 atIndex:1];
    const auto length=std::uint32_t(n);
    [encoder setBytes:&length length:sizeof(length) atIndex:2];
   }
   const NSUInteger group=std::min(NSUInteger(256),
                                     NSUInteger(pipeline.maxTotalThreadsPerThreadgroup));
   const auto dispatch=Clock::now();
   [encoder dispatchThreads:MTLSizeMake(resultsN,1,1)
        threadsPerThreadgroup:MTLSizeMake(group,1,1)];
   [encoder endEncoding];
   [cmd commit];[cmd waitUntilCompleted];
   require(cmd.status==MTLCommandBufferStatusCompleted,"Metal kernel failed");
   const double hostMs=milliseconds(dispatch);
   const double gpuMs=([cmd GPUEndTime]>=[cmd GPUStartTime])?
     1000.*([cmd GPUEndTime]-[cmd GPUStartTime]):-1.;
   Digest measured;
   std::uint64_t tested=0;
   const auto* data=static_cast<const GPUDigest*>([results contents]);
   for(std::size_t i=0;i<resultsN;++i){
    measured.count+=data[i].count;
    measured.sum32+=data[i].sum32;
    measured.xor32^=data[i].xor32;
    tested+=data[i].tests;
   }
   if(tested!=upper)
    throw std::runtime_error("GPU kernel did not execute every assigned pair test");
   const char* label="UNVERIFIED";
   if(cpu.complete){
    if(measured.count!=cpu.digest.count||
       measured.sum32!=cpu.digest.sum32||
       measured.xor32!=cpu.digest.xor32)
      throw std::runtime_error("Actual Metal/CPU full-count plus pair fingerprints mismatch");
    label=n<=1024?"GPU_CPU_SMALL_BRUTE":"GPU_CPU_GRID_DIGEST";
   }else if(scenario=="all_overlap"){
    if(measured.count!=bruteTests)
     throw std::runtime_error("Known O(N^2) exact all-overlap cardinality mismatch");
    label="ANALYTIC_ALL_OVERLAP_COUNT_ONLY";
   }
   csv<<scenario<<','<<mode<<','<<n<<','<<frame<<','<<seed<<','
      <<'"'<<[[device name] UTF8String]<<'"'<<",COMPLETE,"
      <<generation<<','<<build<<','<<cpuMs<<','<<prepared<<','<<hostMs
      <<','<<gpuMs<<','<<tested<<','<<grid.workUpperBound<<','
      <<measured.count<<','<<(cpu.complete?cpu.digest.count:0)
      <<','<<measured.sum32<<','<<(cpu.complete?cpu.digest.sum32:0)
      <<','<<measured.xor32<<','<<(cpu.complete?cpu.digest.xor32:0)
      <<','<<label<<','<<cells<<','<<grid.entries
      <<','<<resultsN*sizeof(GPUDigest)<<','<<gpuBytes<<"\n";
   std::cerr<<"[METAL PRESSURE] "<<scenario<<" "<<mode<<" frame "
            <<frame+1<<"/"<<frames<<" N="<<n
            <<" searched GPU AABB comparisons="<<tested
            <<" exact candidate COUNT="<<measured.count
            <<" host_ms="<<hostMs<<" gpu_ms="<<gpuMs
            <<" validation="<<label<<"\n";
  }
  csv.flush();require(bool(csv),"Partial GPU evidence write");
  csv.close();fs::rename(output.string()+".part",output);
  std::cout<<"PASS bounded REAL Metal pressure run; inspect per-frame status "
           <<"and validation columns. All-overlap COUNT-ONLY results are NOT "
           <<"full pair-materializing collision solutions.\n";
 }catch(const std::exception& e){
  std::cerr<<"METAL PRESSURE FAILED: "<<e.what()<<"\n";
  return 1;
 }
 }
}
