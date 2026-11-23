// Pass 20: complete REAL FEM solver opt-in static BVH equality test.
// The comparison runs identical true neo-Hookean 20m/s finite-triangle
// adaptive physical contact, not an isolated synthetic box microbench.
// Sequential rigid face processing must preserve every accepted source
// position, velocity, impulse, leaf decision, and exact elapsed time.
// Both methods pay their own complete solver integration and static tree
// construction. Timings are diagnostic; no speedup asserted here.
#include "kyoto/rigid_mesh_fem_contact.hpp"
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using namespace kyoto::solid;
using Clock=std::chrono::steady_clock;
static void must(bool yes,const std::string& reason){
    if(!yes)throw std::runtime_error(reason);
}
static void close(double a,double b,const std::string& reason){
    if(std::abs(a-b)>1e-11*std::max({1.0,std::abs(a),std::abs(b)}))
        throw std::runtime_error(reason);
}
static void same_body(const Body& a,const Body& b){
    close(a.time_seconds,b.time_seconds,"physical clock diverged");
    must(a.nodes.size()==b.nodes.size(),"FEM node count changed");
    must(a.tets.size()==b.tets.size(),"tet topology changed");
    must(a.contact_count_step==b.contact_count_step,
         "contact impulse update count changed");
    close(a.normal_impulse_step,b.normal_impulse_step,
          "normal impulse total diverged");
    for(std::size_t i=0;i<a.nodes.size();++i){
        close(a.nodes[i].x.x,b.nodes[i].x.x,"node x diverged");
        close(a.nodes[i].x.y,b.nodes[i].x.y,"node y diverged");
        close(a.nodes[i].x.z,b.nodes[i].x.z,"node z diverged");
        close(a.nodes[i].v.x,b.nodes[i].v.x,"velocity x diverged");
        close(a.nodes[i].v.y,b.nodes[i].v.y,"velocity y diverged");
        close(a.nodes[i].v.z,b.nodes[i].v.z,"velocity z diverged");
        close(a.nodes[i].last_normal_impulse,b.nodes[i].last_normal_impulse,
              "node impulse diverged");
    }
}
static void same_receipts(const MeshAdaptiveStats& a,const MeshAdaptiveStats& b,
                          const std::vector<MeshPhysicalLeaf>& ea,
                          const std::vector<MeshPhysicalLeaf>& eb){
    close(a.start_s,b.start_s,"start clock differs");
    close(a.end_s,b.end_s,"end clock differs");
    close(a.integrated_dt_s,b.integrated_dt_s,"actual integrated time differs");
    close(a.min_clearance_m,b.min_clearance_m,"minimum true gap differs");
    close(a.min_tetra_j,b.min_tetra_j,"minimum FEM Jacobian differs");
    close(a.normal_impulse_ns,b.normal_impulse_ns,"external impulse differs");
    must(a.accepted_leaves==b.accepted_leaves,
         "adaptive accepted substep decision changed");
    must(a.rejected_geom_trials==b.rejected_geom_trials,
         "geometric rejection decision changed");
    must(a.rejected_tetra_trials==b.rejected_tetra_trials,
         "tetrahedron rejection decision changed");
    must(a.face_impulse_updates==b.face_impulse_updates,
         "face impulse update count changed");
    must(a.continuous_pair_tests==b.continuous_pair_tests,
         "actual narrowphase CCD pair set changed");
    must(a.broadphase_box_tests>=b.broadphase_box_tests,
         "static BVH expanded per-rigid triangle box evaluations");
    must(b.broadphase_node_tests>0,
         "opt-in hierarchy did not traverse any broadphase nodes");
    must(ea.size()==eb.size(),"accepted adaptive leaf count changed");
    for(std::size_t i=0;i<ea.size();++i){
        close(ea[i].start_time_s,eb[i].start_time_s,
              "accepted leaf start time differs");
        close(ea[i].end_time_s,eb[i].end_time_s,
              "accepted leaf end time differs");
        close(ea[i].closest_face_gap_m,eb[i].closest_face_gap_m,
              "accepted leaf triangle gap differs");
        close(ea[i].minimum_tetra_j,eb[i].minimum_tetra_j,
              "accepted leaf determinant differs");
        must(ea[i].face_impulse_updates==eb[i].face_impulse_updates,
             "accepted leaf physical impulse receipts differ");
    }
}
static std::pair<Body,StaticTriangleMesh> fast_source(){
    Body body=toroidal_solid();
    auto rigid=corrugated_contact_patch();
    const auto gap=audit_deformable_rigid_mesh(body,rigid,0.);
    const double raise=gap.minimum_distance_m-.012;
    for(auto& v:rigid.vertices)v.z+=raise;
    for(auto& node:body.nodes)node.v={0.,0.,-20.};
    return {std::move(body),std::move(rigid)};
}
int main(int argc,char**argv){
 try{
    if(argc>2)throw std::invalid_argument(
        "usage: kyoto_rigid_mesh_bvh_solver_parity_tests [optional folder]");
    std::ofstream trace;
    if(argc==2){
        std::filesystem::create_directories(argv[1]);
        trace.open(std::filesystem::path(argv[1])/
                  "native_solver_static_bvh_paired.csv");
        trace<<std::setprecision(17);
        trace<<"repeat,method,elapsed_wall_ns,accepted_leaves,rejected_geom,"
                "rejected_tetra,actual_integrated_dt_s,face_impulse_updates,"
                "full_mesh_CCD_narrowphase_pairs,triangle_box_tests,"
                "BVH_node_tests,minimum_real_triangle_gap_m,min_tetra_j\n";
    }
    constexpr int repeats=4;
    for(int rep=0;rep<repeats;++rep){
        auto [original,rigid]=fast_source();
        Body accelerated=original;
        std::vector<MeshPhysicalLeaf> old_leaves,new_leaves;
        MeshAdaptiveStats a{},b{};
        std::int64_t old_ns=0,new_ns=0;
        auto original_step=[&](){
            const auto start=Clock::now();
            a=step_time_consistent_rigid_triangle_mesh(
                original,rigid,.00075,{},&old_leaves,nullptr,false);
            old_ns=std::chrono::duration_cast<std::chrono::nanoseconds>(
                Clock::now()-start).count();
        };
        auto accelerated_step=[&](){
            const auto start=Clock::now();
            b=step_time_consistent_rigid_triangle_mesh(
                accelerated,rigid,.00075,{},&new_leaves,nullptr,true);
            new_ns=std::chrono::duration_cast<std::chrono::nanoseconds>(
                Clock::now()-start).count();
        };
        if(rep%2==0){original_step();accelerated_step();}
        else{accelerated_step();original_step();}
        same_body(original,accelerated);
        same_receipts(a,b,old_leaves,new_leaves);
        must(a.accepted_leaves>1&&a.rejected_geom_trials>0,
             "native fast impact source did not trigger real adaptive CCD");
        if(trace){
            auto write=[&](const char* method,std::int64_t nanos,
                           const MeshAdaptiveStats& x){
                trace<<rep<<','<<method<<','<<nanos<<','
                     <<x.accepted_leaves<<','<<x.rejected_geom_trials<<','
                     <<x.rejected_tetra_trials<<','<<x.integrated_dt_s<<','
                     <<x.face_impulse_updates<<','<<x.continuous_pair_tests<<','
                     <<x.broadphase_box_tests<<','<<x.broadphase_node_tests<<','
                     <<x.min_clearance_m<<','<<x.min_tetra_j<<'\n';
            };
            write("original_complete_FEM",old_ns,a);
            write("opt_in_static_BVH_complete_FEM",new_ns,b);
        }
    }
    must(!trace.is_open()||trace.good(),"original full solver audit write failed");
    std::cout<<"PASS20 exact native 20m/s FEM adaptive contact solver parity "
             <<"across four counterbalanced full physics repetitions; "
             <<"accepted leaves/CCD pairs/impulses/clock/positions match.\n";
    return 0;
 }catch(const std::exception& e){
    std::cerr<<"PASS20 FAIL "<<e.what()<<'\n';return 1;
 }
}
