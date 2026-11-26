#pragma once
// Coarse exact AABB tiles prune rectangular jobs before the packed kernel.
// All final keys use the original binary64 boxes; tile size is fixed at 32.
#include "kyoto/gpu_packed_mask.hpp"
#include "kyoto/projected_sap_axis.hpp"
namespace kyoto::contact::gpu_tiles {
namespace f=features;
namespace g=gpu_mask;
struct Job {std::uint32_t left,right,nleft,nright,diagonal;};
static_assert(sizeof(Job)==20);
struct Work {std::uint64_t full_slots{},tile_tests{},jobs{},pair_slots{},mask_words{};};
struct Tile {std::uint32_t start,count;Aabb bounds;};
inline std::vector<Tile> tiles(std::vector<f::Record>& records,int axis){
    if(records.size()>UINT32_MAX)throw std::overflow_error("tile index exceeds uint32");
    std::sort(records.begin(),records.end(),[&](const auto& a,const auto& b){
        const double x=f::component(a.bounds.min,axis),y=f::component(b.bounds.min,axis);
        return x!=y?x<y:a.id<b.id;});
    std::vector<Tile> result;
    for(std::size_t i=0;i<records.size();i+=32){
        const auto n=std::min<std::size_t>(32,records.size()-i);auto box=records[i].bounds;
        for(std::size_t j=1;j<n;++j)box=f::unite(box,records[i+j].bounds);
        result.push_back({std::uint32_t(i),std::uint32_t(n),box});
    }return result;
}
#define KYOTO_TILE_BODY \
    uint jid=word/32u; uint row=word%32u; \
    if(jid>=njobs)return 0u; \
    Job j=jobs[jid]; if(row>=j.nleft)return 0u; \
    uint bits=0u;for(uint k=0u;k<j.nright;++k){ \
        if(j.diagonal!=0u&&row>=k)continue; \
        if(a[j.left+row].lo[0]<=b[j.right+k].hi[0]&&b[j.right+k].lo[0]<=a[j.left+row].hi[0]&& \
           a[j.left+row].lo[1]<=b[j.right+k].hi[1]&&b[j.right+k].lo[1]<=a[j.left+row].hi[1]&& \
           a[j.left+row].lo[2]<=b[j.right+k].hi[2]&&b[j.right+k].lo[2]<=a[j.left+row].hi[2])bits|=1u<<k; \
    }return bits;
#define KYOTO_TILE_STR0(x) #x
#define KYOTO_TILE_STR(x) KYOTO_TILE_STR0(x)
inline std::uint32_t word(const g::Box* a,const g::Box* b,const Job* jobs,std::uint32_t njobs,std::uint32_t word){using uint=std::uint32_t;KYOTO_TILE_BODY}
inline std::string opencl_source(){return R"CLC(
typedef struct {float lo[4];float hi[4];} Box;
typedef struct {uint left,right,nleft,nright,diagonal;} Job;
inline uint tile_word(__global const Box* a,__global const Box* b,__global const Job* jobs,uint njobs,uint word){
)CLC" KYOTO_TILE_STR(KYOTO_TILE_BODY) R"CLC(
}
__kernel void overlap_tiles(__global const Box* a,__global const Box* b,__global const Job* jobs,__global uint* masks,__global const uint* count){
    uint word=(uint)get_global_id(0); if(word/32u<*count)masks[word]=tile_word(a,b,jobs,*count,word);
}
)CLC";}
inline std::string metal_source(){return R"MSL(
#include <metal_stdlib>
using namespace metal;
struct Box {float lo[4];float hi[4];};
struct Job {uint left,right,nleft,nright,diagonal;};
inline uint tile_word(device const Box* a,device const Box* b,device const Job* jobs,uint njobs,uint word){
)MSL" KYOTO_TILE_STR(KYOTO_TILE_BODY) R"MSL(
}
kernel void overlap_tiles(device const Box* a [[buffer(0)]],device const Box* b [[buffer(1)]],device const Job* jobs [[buffer(2)]],device uint* masks [[buffer(3)]],constant uint& count [[buffer(4)]],uint word [[thread_position_in_grid]]){
    if(word/32u<count)masks[word]=tile_word(a,b,jobs,count,word);
}
)MSL";}
#undef KYOTO_TILE_BODY
#undef KYOTO_TILE_STR
#undef KYOTO_TILE_STR0
// Dispatch is synchronous, and returns exactly 32 mask words per job.
// EE cross-tile pairs occur once; a diagonal tile uses a strict upper triangle.
// Each job's output is bounded, including padding lanes of a partial tile.
template<class Dispatch> f::Result query(const f::Mesh& mesh,Dispatch&& dispatch,Work& work){
    auto built=f::build(mesh);const int axis=f::projected_sap_axis(built);f::Result r;work={};
    const auto vt=tiles(built.vertices,axis),ft=tiles(built.faces,axis),et=tiles(built.edges,axis);
    auto scan=[&](const auto& a,const auto& b,const auto& at,const auto& bt,bool self){
        work.full_slots+=std::uint64_t(a.size())*b.size();
        if(a.empty()||b.empty())return;
        const auto aa=g::boxes(a),bb=g::boxes(b);std::vector<Job> jobs;constexpr std::size_t cap=4096;
        auto flush=[&](){if(jobs.empty())return;const auto masks=dispatch(aa,bb,jobs);if(masks.size()!=jobs.size()*32)throw std::runtime_error("tile mask size");
            work.jobs+=jobs.size();work.mask_words+=masks.size();
            for(std::size_t j=0;j<jobs.size();++j){const auto job=jobs[j];work.pair_slots+=std::uint64_t(job.nleft)*job.nright;
                for(unsigned row=0;row<32;++row){auto bits=masks[j*32+row];while(bits){const unsigned col=std::countr_zero(bits);bits&=bits-1;
                    if(row>=job.nleft||col>=job.nright||(job.diagonal&&row>=col))throw std::runtime_error("tile padding or diagonal bit");
                    const auto& x=a[job.left+row];const auto& y=b[job.right+col];if(!f::valid_pair(x,y,r))continue;
                    if(self)r.pairs.push_back({f::Kind::EdgeEdge,std::min(x.id,y.id),std::max(x.id,y.id)});else r.pairs.push_back({f::Kind::VertexFace,x.id,y.id});
                }}
            }jobs.clear();};
        for(std::size_t i=0;i<at.size();++i)for(std::size_t j=self?i:0;j<bt.size();++j){++work.tile_tests;
            if(!overlaps(at[i].bounds,bt[j].bounds))continue;
            jobs.push_back({at[i].start,bt[j].start,at[i].count,bt[j].count,std::uint32_t(self&&i==j)});if(jobs.size()==cap)flush();}
        flush();
    };
    scan(built.vertices,built.faces,vt,ft,false);scan(built.edges,built.edges,et,et,true);f::normalize(r);return r;
}
} // namespace kyoto::contact::gpu_tiles
