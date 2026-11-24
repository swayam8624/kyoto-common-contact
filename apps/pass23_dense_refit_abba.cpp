// Pass 23: genuinely physically integrated FEM, same-run ABBA comparison
// of EXACT SAME maintained BVH with and without the optional dense-motion
// edge/face envelope fast path. No official IPC cost is inferred here.
// A1/A2 and B1/B2 are independently allocated, advance every frame once,
// and must each match an independent exhaustive VF/EE candidate oracle.
// ALL FOUR paid clocks include topology signature, conservative boxes,
// node refit, traversal, normalization, and the selected optimization.
#include "kyoto/fem_feature_snapshots.hpp"
#include "kyoto/refit_feature_hierarchy.hpp"
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace f=kyoto::contact::features;
namespace s=kyoto::solid;
namespace fem=kyoto::contact::fem;
using kyoto::contact::atlas::RefitHierarchy;
using Clock=std::chrono::steady_clock;
static std::int64_t nanos(Clock::time_point a,Clock::time_point b){
    return std::chrono::duration_cast<std::chrono::nanoseconds>(b-a).count();
}
struct Source {
    int scene{},step{};
    std::size_t tetrahedra{},contact_impulses{},accepted_leaves{};
    double physical_time{};
    f::Mesh mesh;
};
static std::vector<Source> generate(){
    constexpr std::array<unsigned,3> majors{12,20,24};
    constexpr std::array<double,3> stiffness{17500.,22000.,20500.};
    const auto rigid=s::corrugated_contact_patch();
    std::vector<Source> out;
    out.reserve(90);
    for(std::size_t scenario=0;scenario<majors.size();++scenario){
        s::Material mat;mat.young=stiffness[scenario];
        s::Body body=s::toroidal_solid(majors[scenario],3,3,mat);
        for(auto& n:body.nodes){
            n.v={scenario==0?.12:(scenario==2?-.11:0.),
                 scenario==0?-.05:(scenario==2?.09:0.),0.};
        }
        for(int step=1;step<=360;++step){
            const s::Body previous=body;
            const auto physics=s::step_time_consistent_rigid_triangle_mesh(
                body,rigid,.00075);
            if(physics.accepted_leaves<1 ||
               !std::isfinite(body.time_seconds))
                throw std::runtime_error("original physical FEM source step was not accepted");
            if(step%12)continue;
            auto mesh=fem::feature_snapshot(previous,body,rigid);
            mesh.topology_epoch=std::uint64_t(scenario+1);
            out.push_back({int(scenario),step,previous.tets.size(),
                physics.face_impulse_updates,physics.accepted_leaves,
                body.time_seconds,std::move(mesh)});
        }
    }
    if(out.size()!=90)throw std::runtime_error("native 3D FEM source missing");
    return out;
}
int main(int argc,char**argv){
 try{
    if(argc!=2)throw std::invalid_argument(
        "usage: kyoto_pass23_dense_refit_abba OUTPUT_DIRECTORY");
    const auto dir=std::filesystem::path(argv[1]);
    std::filesystem::create_directories(dir);
    const auto states=generate(); // physical simulation outside paid methods
    std::ofstream csv(dir/"pass23_dense_motion_original_fem_abba.csv");
    if(!csv)throw std::runtime_error("cannot save original paid source receipt");
    csv<<std::setprecision(17);
    csv<<"source_kind,scene,step,trial,order_code,cold_epoch,"
          "source_tetrahedra,source_vertices,physical_time_s,"
          "source_contact_impulses,accepted_physical_leaves,"
          "oracle_candidates,dirty_vertices,"
          "dense_warm_frame_count_A1,dense_warm_frame_count_A2,"
          "A1_dense_paid_ns,A2_dense_paid_ns,"
          "B1_incumbent_paid_ns,B2_incumbent_paid_ns,"
          "A1_pairs,A2_pairs,B1_pairs,B2_pairs\n";
    constexpr int repeats=8;
    std::size_t recorded=0,nonzero_contact=0;
    for(int trial=0;trial<repeats;++trial){
        // Four independent mutable BVH instances; each advances
        // through ALL real source states exactly once per repetition.
        RefitHierarchy a1(true,false,false,4,true,true);
        RefitHierarchy a2(true,false,false,4,true,true);
        RefitHierarchy b1(true,false,false,4,true,false);
        RefitHierarchy b2(true,false,false,4,true,false);
        for(std::size_t i=0;i<states.size();++i){
            const auto& source=states[i];
            const auto truth=f::exhaustive(source.mesh).pairs;
            const bool cold=(i==0||states[i-1].scene!=source.scene);
            // Each same-input scene/trial has a rotating ABBA order:
            // A1-B1-B2-A2; A2-B2-B1-A1;
            // B1-A1-A2-B2; B2-A2-A1-B1.
            const int order_index=int((i+std::size_t(trial))%4);
            constexpr std::array<std::array<int,4>,4> patterns{{
                {{0,2,3,1}},{{1,3,2,0}},{{2,0,1,3}},{{3,1,0,2}}
            }};
            constexpr std::array<const char*,4> labels{
                "A1-B1-B2-A2","A2-B2-B1-A1",
                "B1-A1-A2-B2","B2-A2-A1-B1"};
            const auto order=patterns[std::size_t(order_index)];
            std::array<f::Result,4> outputs;
            std::array<std::int64_t,4> elapsed{};
            std::array<RefitHierarchy*,4> methods{&a1,&a2,&b1,&b2};
            for(const int which:order){
                const auto begin=Clock::now();
                outputs[std::size_t(which)]=
                    methods[std::size_t(which)]->query(source.mesh);
                elapsed[std::size_t(which)]=nanos(begin,Clock::now());
                if(elapsed[std::size_t(which)]<=0)
                    throw std::runtime_error("nonpositive native paid monotonic clock");
            }
            for(int k=0;k<4;++k){
                if(outputs[std::size_t(k)].pairs!=truth)
                    throw std::runtime_error("new or original method missed oracle at source step "+
                        std::to_string(source.step)+", scene "+std::to_string(source.scene));
            }
            if(a1.dirty_vertex_count()!=a2.dirty_vertex_count()||
               a1.dirty_vertex_count()!=b1.dirty_vertex_count()||
               a1.dirty_vertex_count()!=b2.dirty_vertex_count())
                throw std::runtime_error("optimized motion detection disagreed with original");
            csv<<"original_physically_integrated_FEM,"<<source.scene<<','
               <<source.step<<','<<trial<<','<<labels[std::size_t(order_index)]<<','
               <<(cold?1:0)<<','<<source.tetrahedra<<','
               <<source.mesh.vertices.size()<<','<<source.physical_time<<','
               <<source.contact_impulses<<','<<source.accepted_leaves<<','
               <<truth.size()<<','<<a1.dirty_vertex_count()<<','
               <<a1.dense_motion_warm_frames()<<','
               <<a2.dense_motion_warm_frames()<<','
               <<elapsed[0]<<','<<elapsed[1]<<','<<elapsed[2]<<','<<elapsed[3]<<','
               <<outputs[0].pairs.size()<<','<<outputs[1].pairs.size()<<','
               <<outputs[2].pairs.size()<<','<<outputs[3].pairs.size()<<'\n';
            ++recorded;
            nonzero_contact+=source.contact_impulses>0;
        }
    }
    if(!csv.good())throw std::runtime_error("incomplete paid native source output");
    std::cout<<"PASS23: native physically integrated 3D neo-Hookean FEM,"
             <<" 3 true mesh/material source scenes, "<<recorded
             <<" 4-arm ABBA paid rows, "<<nonzero_contact
             <<" measured contact-bearing rows, 4 exact exhaustive candidate matches"
             <<" per source, 8 independent method-instance repetitions.\n";
    return 0;
 }catch(const std::exception& e){
    std::cerr<<"PASS23 FAIL: "<<e.what()<<'\n';
    return 1;
 }
}
