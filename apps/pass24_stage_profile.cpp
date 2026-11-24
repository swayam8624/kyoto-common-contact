// Pass24: source-backed, stage-instrumented independent 3D FEM BVH study.
// This is not a throughput contest: the optional chrono stamps themselves
// have overhead. All stage timings form a disjoint, contiguous partition
// of each individual query call in the native C++ method.
//
// Independent exhaustive VF/EE oracle is outside all measured stages.
// Physical dynamics run before all queries, not inside query costs.
#include "kyoto/fem_feature_snapshots.hpp"
#include "kyoto/refit_feature_hierarchy.hpp"
#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace f=kyoto::contact::features;
namespace s=kyoto::solid;
namespace fem=kyoto::contact::fem;
using kyoto::contact::atlas::RefitHierarchy;

struct Source {
    int scene{},step{};
    const char* regime{};
    std::size_t source_tetrahedra{},impulse_updates{};
    f::Mesh mesh;
};
static std::vector<Source> original_states(){
    constexpr std::array<unsigned,3> majors{12,20,24};
    constexpr std::array<double,3> stiffness{17500.,22000.,20500.};
    const auto rigid=s::corrugated_contact_patch();
    std::vector<Source> states;
    states.reserve(96);
    for(std::size_t scene=0;scene<majors.size();++scene){
        s::Material m;m.young=stiffness[scene];
        s::Body body=s::toroidal_solid(majors[scene],3,3,m);
        for(auto& node:body.nodes)
            node.v={scene==0?.12:(scene==2?-.11:0.),
                    scene==0?-.05:(scene==2?.09:0.),0.};
        for(int step=1;step<=360;++step){
            const s::Body prior=body;
            const auto metrics=s::step_time_consistent_rigid_triangle_mesh(
                body,rigid,.00075);
            if(metrics.accepted_leaves<1)
                throw std::runtime_error("source FEM physical step not accepted");
            if(step%12)continue;
            auto snapshot=fem::feature_snapshot(prior,body,rigid);
            snapshot.topology_epoch=std::uint64_t(scene+1);
            states.push_back({int(scene),step,"routine",
                prior.tets.size(),metrics.face_impulse_updates,
                std::move(snapshot)});
        }
    }
    s::Body fast=s::toroidal_solid();
    auto obstacle=s::corrugated_contact_patch();
    const auto initial=s::audit_deformable_rigid_mesh(fast,obstacle,0.);
    const auto shift=initial.minimum_distance_m-.012;
    for(auto& vertex:obstacle.vertices)vertex.z+=shift;
    for(auto& node:fast.nodes)node.v={0,0,-20.0};
    std::vector<s::MeshAcceptedTransition> accepted;
    const auto impact=s::step_time_consistent_rigid_triangle_mesh(
        fast,obstacle,.00075,{},nullptr,&accepted);
    if(impact.accepted_leaves<=1||accepted.size()!=impact.accepted_leaves)
        throw std::runtime_error("fast source did not integrate its accepted CCD leaves");
    for(std::size_t k=0;k<accepted.size();++k){
        const auto& leaf=accepted[k];
        auto snapshot=fem::feature_snapshot(leaf.before,leaf.after,obstacle);
        snapshot.topology_epoch=4;
        states.push_back({3,4000+int(k),"fast_adaptive",
            leaf.before.tets.size(),leaf.receipt.face_impulse_updates,
            std::move(snapshot)});
    }
    if(states.size()!=90+accepted.size())
        throw std::runtime_error("source FEM scene count mismatch");
    return states;
}
int main(int argc,char**argv){
 try{
    if(argc!=2)throw std::invalid_argument(
        "usage: kyoto_pass24_stage_profile OUTPUT_DIRECTORY");
    const auto output=std::filesystem::path(argv[1]);
    std::filesystem::create_directories(output);
    const auto states=original_states();
    std::ofstream csv(output/"original_FEM_profiled_BVH_stages.csv");
    if(!csv)throw std::runtime_error("could not save original source profile");
    csv<<std::setprecision(17);
    csv<<"source_kind,trial,scene,regime,step,variant,"
          "source_tetrahedra,source_vertices,source_impulses,"
          "source_candidate_pairs,cold_epoch,dirty_vertices,"
          "topology_signature_ns,swept_feature_boxes_ns,"
          "cold_build_or_warm_refit_ns,vf_ee_traversal_ns,"
          "candidate_normalize_ns,total_ns\n";
    constexpr int trials=4;
    std::size_t accepted_rows=0;
    for(int trial=0;trial<trials;++trial){
        // Each source-scene interval is visited EXACTLY ONCE by each
        // independent variant. Profiling does not warm one method twice.
        RefitHierarchy full(true,false,false,4,false);
        RefitHierarchy selective(true,false,false,4,true);
        for(std::size_t i=0;i<states.size();++i){
            const auto& state=states[i];
            const auto oracle=f::exhaustive(state.mesh).pairs;
            std::array<RefitHierarchy*,2> methods{&full,&selective};
            const bool selective_first=((i+std::size_t(trial))%2==0);
            const std::array<int,2> order=selective_first?
                std::array<int,2>{1,0}:std::array<int,2>{0,1};
            for(const auto index:order){
                RefitHierarchy::QueryStages stage;
                const auto result=methods[std::size_t(index)]->query(
                    state.mesh,&stage);
                if(result.pairs!=oracle||
                   stage.candidate_count!=oracle.size()||
                   !(stage.total_ns>0)||
                   stage.total_ns!=
                        stage.topology_signature_ns+
                        stage.swept_feature_boxes_ns+
                        stage.cold_build_or_warm_refit_ns+
                        stage.vf_ee_traversal_ns+
                        stage.candidate_normalize_ns)
                    throw std::runtime_error(
                        "original C++ profile broke candidate oracle/stage accounting");
                const bool cold=(i==0||states[i-1].scene!=state.scene);
                if(cold!=stage.topology_cold)
                    throw std::runtime_error("source profile cold epoch mismatch");
                csv<<"original_solver_integrated_FEM,"<<trial<<','
                   <<state.scene<<','<<state.regime<<','<<state.step<<','
                   <<(index?"selective":"full")<<','
                   <<state.source_tetrahedra<<','
                   <<state.mesh.vertices.size()<<','
                   <<state.impulse_updates<<','<<oracle.size()<<','
                   <<(cold?1:0)<<','<<stage.dirty_vertices<<','
                   <<stage.topology_signature_ns<<','
                   <<stage.swept_feature_boxes_ns<<','
                   <<stage.cold_build_or_warm_refit_ns<<','
                   <<stage.vf_ee_traversal_ns<<','
                   <<stage.candidate_normalize_ns<<','
                   <<stage.total_ns<<'\n';
                ++accepted_rows;
            }
        }
    }
    if(!csv.good())throw std::runtime_error("incomplete native C++ profile CSV");
    std::cout<<"PASS24: physically integrated 3D FEM feature-only stage profile, "
             <<accepted_rows<<" fully paid per-method stage receipts, "
             <<states.size()<<" distinct native FEM intervals, "
             <<"independent exhaustive candidate parity. "
             <<"Stage tracing has nonzero overhead, not a whole-solver speedup.\n";
    return 0;
 }catch(const std::exception& e){
    std::cerr<<"PASS24 FAILURE: "<<e.what()<<'\n';return 1;
 }
}
