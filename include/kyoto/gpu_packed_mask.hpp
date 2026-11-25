#pragma once
// Shared scalar kernel body: compiled on CPU, emitted to OpenCL and Metal.
// This packs candidate flags, not final candidate keys. Host still filters
// against the original binary64 boxes and canonical adjacency/mask rules.
#include "kyoto/feature_broadphase.hpp"
#include <bit>
#include <cstdint>
#include <string>
namespace kyoto::contact::gpu_mask {
struct alignas(16) Box {float lo[4]{},hi[4]{};};
struct Params {std::uint32_t first,count,right_count,self;};
static_assert(sizeof(Box)==32&&sizeof(Params)==16);
inline float bound(double x,bool upper){
    float y=static_cast<float>(x);
    if(!std::isfinite(x)||!std::isfinite(y))throw std::overflow_error("GPU float box range exceeded");
    if((upper&&double(y)<x)||(!upper&&double(y)>x))y=std::nextafter(y,upper?INFINITY:-INFINITY);
    if(!std::isfinite(y))throw std::overflow_error("GPU expanded float range exceeded");
    const float m=std::numeric_limits<float>::min();
    if(std::abs(y)<m)y=upper?(y>0?m:0.f):(y<0?-m:0.f);
    if(upper?double(y)<x:double(y)>x)throw std::runtime_error("GPU box conversion not conservative");
    return y;
}
inline std::vector<Box> boxes(const std::vector<features::Record>& records){
    std::vector<Box> out;out.reserve(records.size());
    for(const auto& r:records){Box b;
        b.lo[0]=bound(r.bounds.min.x,false);b.lo[1]=bound(r.bounds.min.y,false);b.lo[2]=bound(r.bounds.min.z,false);
        b.hi[0]=bound(r.bounds.max.x,true);b.hi[1]=bound(r.bounds.max.y,true);b.hi[2]=bound(r.bounds.max.z,true);
        out.push_back(b);
    }return out;
}
inline std::uint32_t words(std::uint32_t count){return count/32u+std::uint32_t(count%32u!=0);}
// Identical control flow and predicates in the three compilations. Only
// address spaces, function declaration and work-item index differ.
#define KYOTO_PACKED_MASK_BODY \
    if(p.right_count==0u || word>=p.count/32u+(p.count%32u!=0u))return 0u; \
    uint offset=word*32u; uint left=p.count-offset; uint n=left<32u?left:32u; uint bits=0u; \
    for(uint lane=0u;lane<n;++lane){ \
        uint pair=p.first+offset+lane; uint i=pair/p.right_count; uint j=pair%p.right_count; \
        if(p.self!=0u&&i>=j)continue; \
        if(a[i].lo[0]<=b[j].hi[0]&&b[j].lo[0]<=a[i].hi[0]&& \
           a[i].lo[1]<=b[j].hi[1]&&b[j].lo[1]<=a[i].hi[1]&& \
           a[i].lo[2]<=b[j].hi[2]&&b[j].lo[2]<=a[i].hi[2])bits|=(1u<<lane); \
    } return bits;
#define KYOTO_MASK_STR_IMPL(x) #x
#define KYOTO_MASK_STR(x) KYOTO_MASK_STR_IMPL(x)
inline std::uint32_t packed_word(const Box* a,const Box* b,Params p,std::uint32_t word){using uint=std::uint32_t;KYOTO_PACKED_MASK_BODY}
inline std::string metal_source(){return R"MSL(
#include <metal_stdlib>
using namespace metal;
struct Box {float lo[4];float hi[4];};
struct Params {uint first,count,right_count,self;};
inline uint packed_word(device const Box* a,device const Box* b,Params p,uint word){
)MSL" KYOTO_MASK_STR(KYOTO_PACKED_MASK_BODY) R"MSL(
}
kernel void overlap_packed(device const Box* a [[buffer(0)]],device const Box* b [[buffer(1)]],device uint* mask [[buffer(2)]],constant Params& p [[buffer(3)]],uint word [[thread_position_in_grid]]){
    if(word<p.count/32u+(p.count%32u!=0u))mask[word]=packed_word(a,b,p,word);
}
)MSL";}
inline std::string opencl_source(){return R"CLC(
typedef struct {float lo[4];float hi[4];} Box;
typedef struct {uint first,count,right_count,self;} Params;
inline uint packed_word(__global const Box* a,__global const Box* b,Params p,uint word){
)CLC" KYOTO_MASK_STR(KYOTO_PACKED_MASK_BODY) R"CLC(
}
__kernel void overlap_packed(__global const Box* a,__global const Box* b,__global uint* mask,__global const Params* p){
    uint word=(uint)get_global_id(0);
    if(word<p->count/32u+(p->count%32u!=0u))mask[word]=packed_word(a,b,*p,word);
}
)CLC";}
#undef KYOTO_PACKED_MASK_BODY
#undef KYOTO_MASK_STR_IMPL
#undef KYOTO_MASK_STR
// Bounded host dispatch shape. The caller owns the device dispatch and
// synchronizes before invoking this exact post-filter.
template<class Dispatch> features::Result query(const features::Mesh& mesh,Dispatch&& dispatch,std::uint32_t chunk=262144u){
    if(!chunk)throw std::invalid_argument("GPU chunk must be positive");
    const auto built=features::build(mesh);features::Result r;
    auto scan=[&](const auto& a,const auto& b,bool self){
        if(a.empty()||b.empty())return;
        const auto total=std::uint64_t(a.size())*std::uint64_t(b.size());
        if(a.size()>UINT32_MAX||b.size()>UINT32_MAX||total>UINT32_MAX)throw std::overflow_error("GPU pair index range exceeded");
        const auto aa=boxes(a),bb=boxes(b);
        for(std::uint64_t first=0;first<total;first+=chunk){
            const Params p{std::uint32_t(first),std::uint32_t(std::min<std::uint64_t>(chunk,total-first)),std::uint32_t(b.size()),std::uint32_t(self)};
            auto masks=dispatch(aa,bb,p);
            if(masks.size()!=words(p.count))throw std::runtime_error("GPU mask count mismatch");
            for(std::size_t w=0;w<masks.size();++w){auto bits=masks[w];while(bits){
                const auto lane=std::countr_zero(bits);bits&=bits-1u;
                const auto offset=w*32u+lane;if(offset>=p.count)throw std::runtime_error("GPU mask padding bit set");
                const auto k=first+offset,i=k/b.size(),j=k%b.size();
                if(self&&i>=j)throw std::runtime_error("GPU self pair order mismatch");
                if(!features::valid_pair(a[i],b[j],r))continue;
                if(self)r.pairs.push_back({features::Kind::EdgeEdge,std::min(a[i].id,b[j].id),std::max(a[i].id,b[j].id)});
                else r.pairs.push_back({features::Kind::VertexFace,a[i].id,b[j].id});
            }}
        }
    };
    scan(built.vertices,built.faces,false);scan(built.edges,built.edges,true);features::normalize(r);return r;
}
} // namespace kyoto::contact::gpu_mask
