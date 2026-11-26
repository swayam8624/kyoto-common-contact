#pragma once
// Objective-C++ only. Shared synchronous dispatch implementation from the
// physical Metal query benchmark; extracted without algorithm changes.
#include "kyoto/tiled_gpu_mask.hpp"
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
namespace kyoto::contact::metal_dispatch {
namespace g=kyoto::contact::gpu_mask;
namespace t=kyoto::contact::gpu_tiles;
inline void need(bool b,const char* m){if(!b)throw std::runtime_error(m);}
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
} // namespace kyoto::contact::metal_dispatch
