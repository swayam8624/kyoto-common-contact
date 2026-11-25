// Execute the compiled OpenCL kernel as CPU code; this is not GPU timing.
#include "kyoto/tiled_gpu_mask.hpp"
#include "kyoto/physical_scaling_snapshot.hpp"
#include "kyoto/cached_axis_sap.hpp"
#include "kyoto/persistent_feature_sap.hpp"
#include <chrono>
#include <iostream>
#include <random>
namespace f=kyoto::contact::features;namespace g=kyoto::contact::gpu_mask;namespace t=kyoto::contact::gpu_tiles;namespace s=kyoto::solid;
static thread_local std::size_t wi;
std::size_t get_global_id(unsigned axis){if(axis!=0)throw std::runtime_error("dimension");return wi;}
extern "C" void overlap_tiles(const g::Box*,const g::Box*,const t::Job*,std::uint32_t*,const std::uint32_t*);
static void need(bool b,const char* m){if(!b)throw std::runtime_error(m);}
static auto dispatch(const std::vector<g::Box>& a,const std::vector<g::Box>& b,const std::vector<t::Job>& jobs){
    const auto n=std::uint32_t(jobs.size());std::vector<std::uint32_t> out(n*32+1,0xdeadbeefu);
    for(wi=0;wi<out.size();++wi)overlap_tiles(a.data(),b.data(),jobs.data(),out.data(),&n);
    need(out.back()==0xdeadbeefu,"kernel wrote beyond work");out.pop_back();return out;
}
int main(){
    std::mt19937_64 rng(408526);std::uniform_int_distribution<int> d(-20,20);
    for(int trial=0;trial<1000;++trial){f::Mesh m;const int n=3+3*int(rng()%40);const double scale=trial%3==0?1e-40:trial%3==1?1.:.125,base=trial%3==2?16777216.:0.;
        auto point=[&](){return kyoto::contact::Vec3{base+scale*d(rng),scale*d(rng),scale*d(rng)};};
        for(int i=0;i<n;++i){const auto p=point(),q=point();m.vertices.push_back({std::uint64_t(10+i),{{p,p,q,q}}});}
        for(int i=0;i+1<n;i+=2)m.edges.push_back({std::uint64_t(50+i),{std::uint64_t(10+i),std::uint64_t(11+i)}});
        for(int i=0;i+2<n;i+=3)m.faces.push_back({std::uint64_t(100+i),{std::uint64_t(10+i),std::uint64_t(11+i),std::uint64_t(12+i)}});
        if(trial%4==0)m.vertices[0].mask=0;
        t::Work w;need(t::query(m,dispatch,w).pairs==f::exhaustive(m).pairs,"random tile parity");
    }
    std::cerr<<"PASS 1000 randomized scenes\n";
    std::cout<<"width,near,block,leaf,arm,ns,full_slots,tile_tests,jobs,pair_slots,mask_words,candidates\n";
    using Clock=std::chrono::steady_clock;
    for(unsigned width:{1u,4u,16u,32u,64u})for(int near:{0,1}){
        auto a=s::fixtures::u_solid(width,near?.0025:.02,.1),b=s::fixtures::remote();s::CoupledFEMSettings cfg;cfg.hierarchy=cfg.typed_self_contact=true;s::CoupledFEMContext warm;std::size_t impulses=0;std::vector<f::Mesh> source;
        for(int step=0;step<4;++step){std::vector<s::CoupledFEMLeaf> leaves;const auto r=s::step_coupled_fem(a,b,.00025,cfg,&leaves,&warm);need(!leaves.empty()&&leaves.size()==r.accepted_leaves,"missing source leaf");impulses+=r.self_a.impulse_updates;
            for(const auto& l:leaves)source.push_back(kyoto::contact::physical_scaling::snapshot(l.before_a,l.after_a,cfg.skin));}
        need(near?impulses>0:impulses==0,"vacuous source regime");
        for(int block=-1;block<3;++block){kyoto::contact::persistent::OrderedSweep sap{true};kyoto::contact::persistent::CachedAxisSweep cached;
            for(std::size_t leaf=0;leaf<source.size();++leaf){const auto& m=source[leaf];const auto truth=f::exhaustive(m).pairs;
                for(int slot=0;slot<3;++slot){const int arm=(slot+block+3)%3;t::Work w;const auto begin=Clock::now();auto r=arm==0?sap.query(m):arm==1?cached.query(m):t::query(m,dispatch,w);const auto elapsed=std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now()-begin).count();need(r.pairs==truth,"physical tile parity");
                    if(block>=0)std::cout<<width<<','<<near<<','<<block<<','<<leaf<<','<<(arm==0?"persistent_sap":arm==1?"cached_sap":"tiled_cpu_kernel")<<','<<elapsed<<','<<w.full_slots<<','<<w.tile_tests<<','<<w.jobs<<','<<w.pair_slots<<','<<w.mask_words<<','<<r.pairs.size()<<'\n';}
            }
        }
        std::cerr<<"PASS width="<<width<<" near="<<near<<'\n';
    }
}
