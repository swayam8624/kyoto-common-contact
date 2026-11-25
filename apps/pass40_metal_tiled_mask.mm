// Real-device benchmark. Every GPU arm includes CPU planning, upload,
// synchronized dispatch, readback and original binary64 candidate filtering.
// No result is emitted unless the Metal command completed and keys match.
#include "kyoto/tiled_gpu_mask.hpp"
#include "kyoto/physical_scaling_snapshot.hpp"
#include "kyoto/cached_axis_sap.hpp"
#include "kyoto/fused_axis_sap.hpp"
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
namespace f=kyoto::contact::features;namespace g=kyoto::contact::gpu_mask;namespace t=kyoto::contact::gpu_tiles;namespace s=kyoto::solid;
static void need(bool b,const char* m){if(!b)throw std::runtime_error(m);}
class MetalDispatch{
    id<MTLDevice> device_;
    id<MTLCommandQueue> queue_;
    id<MTLComputePipelineState> packed_,tiled_;
    id<MTLComputePipelineState> pipeline(const std::string& source,NSString* name){
        NSError* error=nil;auto* opt=[[MTLCompileOptions alloc] init];opt.fastMathEnabled=NO;
        id<MTLLibrary> lib=[device_ newLibraryWithSource:[NSString stringWithUTF8String:source.c_str()] options:opt error:&error];
        if(!lib)throw std::runtime_error(error?error.localizedDescription.UTF8String:"Metal library failed");
        id<MTLFunction> fn=[lib newFunctionWithName:name];need(fn!=nil,"kernel unavailable");
        id<MTLComputePipelineState> result=[device_ newComputePipelineStateWithFunction:fn error:&error];
        if(!result)throw std::runtime_error(error?error.localizedDescription.UTF8String:"Metal pipeline failed");return result;
    }
    template<class T> id<MTLBuffer> upload(const std::vector<T>& data){
        need(!data.empty()&&data.size()<=device_.maxBufferLength/sizeof(T),"invalid input buffer size");
        id<MTLBuffer> b=[device_ newBufferWithBytes:data.data() length:data.size()*sizeof(T) options:MTLResourceStorageModeShared];need(b!=nil,"upload allocation failed");return b;
    }
    template<class Encode> std::vector<std::uint32_t> execute(id<MTLComputePipelineState> p,std::size_t words,Encode encode){
        need(words>0&&words<=device_.maxBufferLength/sizeof(std::uint32_t),"mask size");
        id<MTLBuffer> out=[device_ newBufferWithLength:words*sizeof(std::uint32_t) options:MTLResourceStorageModeShared];need(out!=nil,"mask allocation failed");
        id<MTLCommandBuffer> cmd=[queue_ commandBuffer];need(cmd!=nil,"command buffer failed");
        id<MTLComputeCommandEncoder> enc=[cmd computeCommandEncoder];need(enc!=nil,"encoder failed");
        [enc setComputePipelineState:p];encode(enc,out);
        [enc dispatchThreads:MTLSizeMake(words,1,1) threadsPerThreadgroup:MTLSizeMake(std::min<NSUInteger>(256,p.maxTotalThreadsPerThreadgroup),1,1)];
        [enc endEncoding];[cmd commit];[cmd waitUntilCompleted];need(cmd.status==MTLCommandBufferStatusCompleted,"Metal execution failed");
        const auto* data=static_cast<const std::uint32_t*>(out.contents);need(data!=nullptr,"readback failed");return {data,data+words};
    }
public:
    explicit MetalDispatch(id<MTLDevice> d):device_(d){queue_=[d newCommandQueue];need(queue_!=nil,"queue unavailable");packed_=pipeline(g::metal_source(),@"overlap_packed");tiled_=pipeline(t::metal_source(),@"overlap_tiles");}
    std::vector<std::uint32_t> packed(const std::vector<g::Box>& a,const std::vector<g::Box>& b,g::Params p){
        @autoreleasepool {auto ab=upload(a),bb=upload(b);return execute(packed_,g::words(p.count),[&](id<MTLComputeCommandEncoder> enc,id<MTLBuffer> out){[enc setBuffer:ab offset:0 atIndex:0];[enc setBuffer:bb offset:0 atIndex:1];[enc setBuffer:out offset:0 atIndex:2];[enc setBytes:&p length:sizeof(p) atIndex:3];});}
    }
    std::vector<std::uint32_t> tiled(const std::vector<g::Box>& a,const std::vector<g::Box>& b,const std::vector<t::Job>& jobs){
        @autoreleasepool {need(jobs.size()<=4096,"tile batch limit");auto ab=upload(a),bb=upload(b),jb=upload(jobs);const auto n=std::uint32_t(jobs.size());return execute(tiled_,jobs.size()*32,[&](id<MTLComputeCommandEncoder> enc,id<MTLBuffer> out){[enc setBuffer:ab offset:0 atIndex:0];[enc setBuffer:bb offset:0 atIndex:1];[enc setBuffer:jb offset:0 atIndex:2];[enc setBuffer:out offset:0 atIndex:3];[enc setBytes:&n length:sizeof(n) atIndex:4];});}
    }
};
int main(int argc,char** argv){@autoreleasepool{try{
    need(argc==2||argc==3,"usage: OUTPUT_DIR [maximum width]");const unsigned max=argc==3?unsigned(std::stoul(argv[2])):64;need(max==1||max==4||max==16||max==32||max==64,"invalid width");
    id<MTLDevice> d=MTLCreateSystemDefaultDevice();if(!d){std::cerr<<"no Metal device\n";return 77;}MetalDispatch gpu(d);
    const std::filesystem::path out(argv[1]);need(!std::filesystem::exists(out),"output exists");std::filesystem::create_directories(out);
    std::ofstream meta(out/"device.txt"),csv(out/"tiled_metal.csv");meta<<d.name.UTF8String<<"\nmax_buffer="<<d.maxBufferLength<<'\n';
    csv<<"width,near,block,leaf,slot,arm,ns,candidates,mask_words,tile_slots,tile_jobs\n";
    auto packed=[&](const f::Mesh& m,std::uint64_t& words){return g::query(m,[&](const auto& a,const auto& b,g::Params p){words+=g::words(p.count);return gpu.packed(a,b,p);});};
    auto tiled=[&](const f::Mesh& m,t::Work& w){return t::query(m,[&](const auto& a,const auto& b,const auto& jobs){return gpu.tiled(a,b,jobs);},w);};
    // Same precision controls for both device kernels; exact inclusive keys.
    for(double scale:{1.,16777216.,1e-40}){f::Mesh m;const double dx=scale==16777216.?.25:scale*.125;
        for(const auto p:{kyoto::contact::Vec3{scale,0,0},{scale+dx,0,0},{scale,dx,0},{scale+dx*.5,dx*.5,0},{scale,0,dx},{scale+dx,dx,0}})m.vertices.push_back({m.vertices.size(),{{p,p,p,p}}});
        m.faces={{0,{0,1,2}}};m.edges={{0,{0,2}},{1,{1,3}},{2,{4,5}}};const auto truth=f::exhaustive(m).pairs;std::uint64_t words=0;t::Work w;
        need(!truth.empty()&&packed(m,words).pairs==truth&&tiled(m,w).pairs==truth,"precision control mismatch");}
    using Clock=std::chrono::steady_clock;
    for(unsigned width:{1u,4u,16u,32u,64u})if(width<=max)for(int near:{0,1}){
        auto a=s::fixtures::u_solid(width,near?.0025:.02,.1),b=s::fixtures::remote();s::CoupledFEMSettings cfg;cfg.hierarchy=cfg.typed_self_contact=true;s::CoupledFEMContext ctx;std::vector<f::Mesh> source;std::size_t impulses=0;
        for(int step=0;step<4;++step){std::vector<s::CoupledFEMLeaf> trace;const auto r=s::step_coupled_fem(a,b,.00025,cfg,&trace,&ctx);need(!trace.empty()&&trace.size()==r.accepted_leaves,"physical trace");impulses+=r.self_a.impulse_updates;for(const auto& l:trace)source.push_back(kyoto::contact::physical_scaling::snapshot(l.before_a,l.after_a,cfg.skin));}
        need(near?impulses>0:impulses==0,"nonrepresentative workload");std::vector<std::vector<f::Candidate>> truth;for(const auto& m:source)truth.push_back(f::exhaustive(m).pairs);
        for(int block=-1;block<8;++block){kyoto::contact::persistent::CachedAxisSweep cpu{true};kyoto::contact::persistent::FusedAxisSweep fused;for(std::size_t leaf=0;leaf<source.size();++leaf)for(int slot=0;slot<4;++slot){const int arm=(slot+block+4)%4;t::Work w;std::uint64_t words=0;const auto begin=Clock::now();auto r=arm==0?cpu.query(source[leaf]):arm==1?fused.query(source[leaf]):arm==2?packed(source[leaf],words):tiled(source[leaf],w);const auto ns=std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now()-begin).count();need(r.pairs==truth[leaf],"GPU/CPU candidate mismatch");if(block>=0)csv<<width<<','<<near<<','<<block<<','<<leaf<<','<<slot<<','<<(arm==0?"spatial_first":arm==1?"fused_sap":arm==2?"metal_packed":"metal_tiled")<<','<<ns<<','<<r.pairs.size()<<','<<(arm==3?w.mask_words:words)<<','<<w.pair_slots<<','<<w.jobs<<'\n';}}
    }need(csv.good()&&meta.good(),"output failure");return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}}
