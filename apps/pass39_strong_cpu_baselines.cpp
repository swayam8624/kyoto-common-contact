// Pass 37A: physical source scaling with matched CPU candidate backends.
// Source geometry comes exclusively from accepted native FEM leaves.
// Candidate timings include envelope extraction, validation, tree
// maintenance, adjacency, and canonical output; source snapshot assembly
// is shared and separately timed. This is not a whole-solver speed duel.
#include "kyoto/physical_scaling_snapshot.hpp"
#include "kyoto/persistent_feature_sap.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
namespace s=kyoto::solid;
namespace f=kyoto::contact::features;
namespace at=kyoto::contact::atlas;
using Clock=std::chrono::steady_clock;
static void need(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
static double ns(Clock::time_point a,Clock::time_point b){
    return std::chrono::duration<double,std::nano>(b-a).count();
}
using kyoto::contact::physical_scaling::snapshot;
struct Sample {f::Mesh mesh;double prep_ns{};double time{},dt{};};
struct Arms{
    at::RefitHierarchy refit;
    kyoto::contact::persistent::OrderedSweep persistent_x, persistent_axis{true};
    // Pair-cut and selective-refit flags are explicit, not tuned per scene.
    at::RefitHierarchy frontier{true,true,false,4,true,true};
    f::Result query(int arm,const f::Mesh& m){
        switch(arm){
            case 0:return f::exhaustive(m);
            case 1:return f::sweep_and_prune(m);
            case 2:{at::RefitHierarchy cold;return cold.query(m);}
            case 3:return refit.query(m);
            case 4:return frontier.query(m);
            case 5:return persistent_x.query(m);
            case 6:return f::adaptive_sweep_and_prune(m);
            case 7:return persistent_axis.query(m);
            default:throw std::logic_error("unknown backend");
        }
    }
};
int main(int argc,char** argv){
    try{
        if(argc<2||argc>3)throw std::invalid_argument("usage: pass37 OUTPUT_DIR [max_width]");
        const unsigned maximum=argc==3?unsigned(std::stoul(argv[2])):16;
        need(maximum==1||maximum==4||maximum==16||maximum==32||maximum==64,
             "max_width must be 1, 4, 16, 32 or 64");
        const std::filesystem::path dir=argv[1];std::filesystem::create_directories(dir);
        std::ofstream samples(dir/"physical_source_scaling.csv"),times(dir/"matched_cpu_candidate_scaling.csv");
        samples<<std::setprecision(17)
               <<"width_cells,regime,nodes,tets,macrostep,step_with_trace_ns,accepted_leaves,self_impulses,rejections,certified_min_j\n";
        times<<std::setprecision(17)
             <<"width_cells,regime,block,leaf,order_slot,arm,vertices,edges,faces,source_snapshot_ns,query_ns,candidates,box_tests,incident_rejections,time_s,dt_s\n";
        constexpr std::array<const char*,8> names{"exhaustive","rebuilt_sap","rebuilt_bvh","retained_refit","retained_selective_frontier","persistent_sap_x","adaptive_sap","persistent_sap_axis"};
        for(unsigned width:{1u,4u,16u,32u,64u}){
            if(width>maximum)continue;
            for(int regime=0;regime<2;++regime){
                const char* label=regime==0?"near_contact":"separated";
                auto a=s::fixtures::u_solid(width,regime==0?.0025:.02,.1);
                auto b=s::fixtures::remote();
                s::CoupledFEMContext context;
                s::CoupledFEMSettings cfg;cfg.hierarchy=cfg.typed_self_contact=true;
                std::vector<Sample> source;
                std::size_t impulses=0;
                for(int step=0;step<4;++step){
                    std::vector<s::CoupledFEMLeaf> leaves;
                    const auto t0=Clock::now();
                    const auto receipt=s::step_coupled_fem(a,b,.00025,cfg,&leaves,&context);
                    const auto t1=Clock::now();
                    need(leaves.size()==receipt.accepted_leaves&&receipt.certified_min_j>1e-6,
                         "source trajectory was not certified");
                    impulses+=receipt.self_a.impulse_updates;
                    samples<<width<<','<<label<<','<<a.nodes.size()<<','<<a.tets.size()<<','<<step<<','
                           <<ns(t0,t1)<<','<<receipt.accepted_leaves<<','<<receipt.self_a.impulse_updates<<','
                           <<receipt.rejected_geometry+receipt.rejected_tets<<','<<receipt.certified_min_j<<'\n';
                    for(const auto& leaf:leaves){
                        const auto t2=Clock::now();
                        auto mesh=snapshot(leaf.before_a,leaf.after_a,cfg.skin);
                        const auto t3=Clock::now();
                        source.push_back({std::move(mesh),ns(t2,t3),leaf.before_a.time_seconds,leaf.dt});
                    }
                }
                need(!source.empty()&&(regime!=0||impulses>0),"near-contact source lacked actual self impulses");
                // One discarded block and eight measured order-balanced blocks.
                // Each backend owns only its own trajectory cache. The
                // independent exhaustive result is also computed outside timers.
                for(int block=-1;block<8;++block){
                    Arms arms;
                    for(std::size_t i=0;i<source.size();++i){
                        const auto& sample=source[i];
                        const auto truth=f::exhaustive(sample.mesh).pairs;
                        for(int slot=0;slot<8;++slot){
                            const int arm=(slot+block+8)%8;
                            const auto t0=Clock::now();
                            const auto result=arms.query(arm,sample.mesh);
                            const auto t1=Clock::now();
                            need(result.pairs==truth,"CPU candidate backend missed or added a pair");
                            if(block>=0)
                                times<<width<<','<<label<<','<<block<<','<<i<<','<<slot<<','<<names[arm]<<','
                                     <<sample.mesh.vertices.size()<<','<<sample.mesh.edges.size()<<','
                                     <<sample.mesh.faces.size()<<','<<sample.prep_ns<<','<<ns(t0,t1)<<','
                                     <<result.pairs.size()<<','<<result.candidate_aabb_tests<<','
                                     <<result.incident_rejections<<','<<sample.time<<','<<sample.dt<<'\n';
                        }
                    }
                    need(arms.refit.rebuilds()==1&&arms.frontier.rebuilds()==1&&
                         arms.refit.refits()+1==source.size()&&arms.frontier.refits()+1==source.size()&&
                         arms.persistent_x.warm_frames()+1==source.size()&&arms.persistent_axis.warm_frames()+1==source.size(),
                         "retained trajectory backend lifetime incorrect");
                }
                std::cout<<"PASS37 width="<<width<<" regime="<<label<<" tets="<<a.tets.size()
                         <<" accepted_source_leaves="<<source.size()<<" self_impulses="<<impulses<<'\n';
            }
        }
        need(samples.good()&&times.good(),"scaling receipt write failed");
        return 0;
    }catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}
}
