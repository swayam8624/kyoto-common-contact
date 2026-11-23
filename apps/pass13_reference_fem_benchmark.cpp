// Pass 13: measured reference broadphase on ORIGINAL solver-generated 3D FEM
// trajectories, not synthetic animation. This benchmarks our independent
// brute-force reference versus rebuilt SAP; it is NOT an SCA vs IPC result.
// Both methods pay feature envelope construction, candidate normalization,
// and their own query work. State generation is excluded and identified.
#include "kyoto/feature_broadphase.hpp"
#include "kyoto/persistent_feature_sap.hpp"
#include "kyoto/refit_feature_hierarchy.hpp"
#include "kyoto/fem_feature_snapshots.hpp"
#include "kyoto/rigid_mesh_fem_contact.hpp"
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <set>
#include <stdexcept>
#include <utility>
#include <vector>

namespace f=kyoto::contact::features;
namespace s=kyoto::solid;
using Clock=std::chrono::steady_clock;

// Both native and upstream IPC trials use one shared source geometry adapter.
static f::Mesh scene(const s::Body& before,const s::Body& after,
                     const s::StaticTriangleMesh& rigid){
    return kyoto::contact::fem::feature_snapshot(before,after,rigid);
}

static std::int64_t nanos(Clock::time_point a,Clock::time_point b){
    return std::chrono::duration_cast<std::chrono::nanoseconds>(b-a).count();
}
int main(int argc,char**argv){
  try{
    if(argc!=2)throw std::invalid_argument(
        "usage: kyoto_pass13_reference_fem_benchmark OUTPUT_FOLDER");
    const std::filesystem::path out=argv[1];
    std::filesystem::create_directories(out);
    std::ofstream csv(out/"native_fem_reference_broadphase.csv");
    kyoto::contact::persistent::OrderedSweep persistent;
    kyoto::contact::atlas::RefitHierarchy hierarchy;
    if(!csv)throw std::runtime_error("cannot write reference experiment CSV");
    csv<<"step,time_s,vertices,edges,faces,exact_pairs,sap_pairs,"
          "reference_candidate_aabb_tests,sap_candidate_aabb_tests,"
          "reference_build_ns,reference_query_ns,sap_build_ns,sap_query_ns,"
          "persistent_total_ns,persistent_warm_frames,persistent_invalidations,"
          "hierarchy_total_ns,hierarchy_rebuilds,hierarchy_refits,"
          "source_face_impulse_updates,source_accepted_leaves\n";
    csv<<std::setprecision(17);
    s::Body body=s::toroidal_solid();
    const auto patch=s::corrugated_contact_patch();
    constexpr int steps=360; // Includes first source face contact near step 293.
    std::size_t contact_steps=0;
    for(int i=1;i<=steps;++i){
        s::Body before=body;
        const auto physical=s::step_time_consistent_rigid_triangle_mesh(
            body,patch,0.00075);
        if(physical.face_impulse_updates)++contact_steps;
        if(i%12!=0 && i!=steps)continue; // independent deterministic snapshot cadence
        const auto mesh=scene(before,body,patch);
        const auto t0=Clock::now();
        const auto reference_built=f::build(mesh);
        const auto t1=Clock::now();
        const auto truth=f::exhaustive(reference_built);
        const auto t2=Clock::now();
        const auto sap_built=f::build(mesh);
        const auto t3=Clock::now();
        const auto sap=f::sweep_and_prune(sap_built);
        const auto t4=Clock::now();
        const auto maintained=persistent.query(mesh);
        const auto t5=Clock::now();
        const auto hierarchical=hierarchy.query(mesh);
        const auto t6=Clock::now();
        if(truth.pairs!=hierarchical.pairs)
            throw std::runtime_error("maintained hierarchy candidate parity failure at physical step "+std::to_string(i));
        if(truth.pairs!=maintained.pairs)
            throw std::runtime_error("persistent sweep candidate parity failure at physical step "+std::to_string(i));
        if(truth.pairs!=sap.pairs)
            throw std::runtime_error("candidate parity failure at physical step "+std::to_string(i));
        if(physical.accepted_leaves==0)
            throw std::runtime_error("unintegrated original FEM physical step");
        csv<<i<<','<<body.time_seconds<<','<<mesh.vertices.size()<<','
           <<mesh.edges.size()<<','<<mesh.faces.size()<<','
           <<truth.pairs.size()<<','<<sap.pairs.size()<<','
           <<truth.candidate_aabb_tests<<','<<sap.candidate_aabb_tests<<','
           <<nanos(t0,t1)<<','<<nanos(t1,t2)<<','
           <<nanos(t2,t3)<<','<<nanos(t3,t4)<<','
           <<nanos(t4,t5)<<','<<persistent.warm_frames()<<','
           <<persistent.invalidations()<<','
           <<nanos(t5,t6)<<','<<hierarchy.rebuilds()<<','
           <<hierarchy.refits()<<','
           <<physical.face_impulse_updates<<','
           <<physical.accepted_leaves<<'\n';
    }
    // Changing the epoch and collision mask must invalidate the warm cache.
    const auto changed_before=body;
    auto changed_mesh=scene(changed_before,body,patch);
    changed_mesh.topology_epoch=2;
    changed_mesh.faces.back().mask=0;
    const auto invalidated=persistent.query(changed_mesh);
    if(invalidated.pairs!=f::exhaustive(changed_mesh).pairs ||
       persistent.invalidations()!=1)
        throw std::runtime_error("topology/mask invalidation failed oracle parity");
    const auto new_hierarchy=hierarchy.query(changed_mesh);
    if(new_hierarchy.pairs!=f::exhaustive(changed_mesh).pairs ||
       hierarchy.rebuilds()!=2)
        throw std::runtime_error("maintained hierarchy invalidation failed");
    std::cout<<"PASS13_REFERENCE_FEM only: "<<steps
             <<" physically integrated macro-steps, "<<contact_steps
             <<" contact-bearing steps; exhaustive/rebuilt SAP/persistent-order SAP parity. "
             <<"No official IPC timings. No maintained SCA timings.\n";
    return 0;
  }catch(const std::exception& e){
    std::cerr<<"PASS13_REFERENCE_FEM_ERROR: "<<e.what()<<'\n';
    return 1;
  }
}
