// Pass 11C: TRUE finite triangle-mesh adaptive impact, no sphere.
//
// This is intentionally separate from the routine 900-step scene:
// at 20 m/s a 0.75 ms trial moves 15 mm towards a true triangulated
// rigid surface whose original closest gap is ~12 mm. The unsafe
// full-dt proposal MUST be rejected, and the actual macro time
// MUST be re-integrated as accepted, complete, surface-safe leaves.
//
// "The old full-step would tunnel" is checked directly, not assumed.
// The source body remains a real 480-tet neo-Hookean material with
// positive Jacobians and a mesh-aware barycentric contact law.
// We do NOT assert that this high-speed numerical impact matches
// independently calibrated physical material/energy.

#include "kyoto/rigid_mesh_fem_contact.hpp"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <vector>

using namespace kyoto::solid;
static void must(bool b,const char* why){
    if(!b)throw std::runtime_error(why);
}
int main(int argc,char**argv) {
 try{
    if(argc>2)throw std::invalid_argument(
       "usage: kyoto_fast_mesh_impact_tests [optional source folder]");
    Body body=toroidal_solid();
    auto rigid=corrugated_contact_patch();
    const auto initial=audit_deformable_rigid_mesh(body,rigid,0.);
    constexpr double target=.012;
    // Translate the original rigid mesh in +z only. The closest pair
    // changes slightly under this translation because the patch is
    // NONPLANAR, so record the true source clearance afterwards.
    const double raise=initial.minimum_distance_m-target;
    for(auto& x:rigid.vertices)x.z+=raise;
    validate(rigid);
    const double actual_initial=audit_deformable_rigid_mesh(
        body,rigid,0.).minimum_distance_m;
    must(actual_initial>.006&&actual_initial<.018,
        "fast obstacle source clearance not the designed near-contact");
    constexpr double speed=20,dt=.00075;
    for(auto& node:body.nodes)node.v={0,0,-speed};
    Body copy=body;
    RigidMeshContactSettings s;
    std::vector<Triangle3> triangles;
    std::vector<TriangleBox> boxes;
    for(std::size_t i=0;i<rigid.faces.size();i++){
        const auto tri=face_positions(rigid,i);
        triangles.push_back(tri);
        boxes.push_back(bounding(tri));
    }
    const auto full=try_rigid_mesh_fem_full_step(
        copy,rigid,triangles,boxes,dt,s);
    must(!full.accepted&&full.was_rejected_by_continuous_geometry,
         "physical 20 m/s full timestep failed to exercise mesh CCD rejection");
    // Its rejection MUST be transactional; no phantom elapsed time.
    must(body.time_seconds==0&&copy.time_seconds==0,
         "rejected mesh full trial advanced public physical time");
    std::vector<MeshPhysicalLeaf> leaves;
    const auto accepted=step_time_consistent_rigid_triangle_mesh(
        body,rigid,dt,s,&leaves);
    must(accepted.rejected_geom_trials>0&&accepted.accepted_leaves>1,
         "true finite-mesh fast impact was not time-subdivided");
    must(std::abs(accepted.integrated_dt_s-dt)<1e-12&&
         std::abs(body.time_seconds-dt)<1e-12,
         "finite mesh adaptive collision lost physical elapsed time");
    must(leaves.size()==accepted.accepted_leaves,
         "source physical substep receipts incomplete");
    double accounted=0,minimum_gap=1e10,minJ=1e10;
    for(std::size_t i=0;i<leaves.size();i++){
        const auto& r=leaves[i];
        must(r.dt_s>0,"accepted zero-dt finite mesh physical leaf");
        must(std::abs(r.start_time_s-accounted)<1e-10,
             "finite mesh real physical substeps not time-contiguous");
        accounted+=r.dt_s;
        must(std::abs(r.end_time_s-accounted)<1e-10,
             "finite mesh physical clock receipt wrong");
        must(r.closest_face_gap_m>s.clearance_skin_m*.5,
             "accepted fast finite mesh substep penetrates rigid face");
        must(r.minimum_tetra_j>1e-6,
             "fast physical FEM tetrahedron inverted");
        minimum_gap=std::min(minimum_gap,r.closest_face_gap_m);
        minJ=std::min(minJ,r.minimum_tetra_j);
    }
    must(std::abs(accounted-dt)<1e-12,
         "sum of fast finite mesh actually integrated substeps incomplete");

    // Check true impulse accounting across ALL accepted adaptive leaves,
    // not just the last accepted leaf's nodal diagnostics.
    double nodal_sum=0;
    for(const auto& node:body.nodes)
        nodal_sum+=node.last_normal_impulse;
    must(std::abs(nodal_sum-accepted.normal_impulse_ns)<
         1e-8*std::max(1.,accepted.normal_impulse_ns),
         "fast macro-step per-node impulse receipt excludes earlier physical leaves");
    const auto final=audit_deformable_rigid_mesh(body,rigid,0.);
    must(final.overlapping_face_pairs==0&&final.minimum_distance_m>0,
         "fully adaptive true mesh impact penetrated finite rigid faces");
    if(argc==2){
        std::filesystem::create_directories(argv[1]);
        const std::filesystem::path p=argv[1];
        std::ofstream csv(p/"fast_finite_mesh_physical_leaves.csv");
        csv<<std::setprecision(17);
        csv<<"leaf_id,start_time_s,end_time_s,actual_dt_s,"
               "min_full_triangle_gap_m,min_tetra_j,"
               "normal_impulse_ns,contact_updates,"
               "continuous_pair_tests,swept_interval_nodes,"
               "bisection_depth\n";
        for(std::size_t i=0;i<leaves.size();i++){
            const auto& r=leaves[i];
            csv<<i<<','<<r.start_time_s<<','<<r.end_time_s<<','
               <<r.dt_s<<','<<r.closest_face_gap_m<<','
               <<r.minimum_tetra_j<<','
               <<r.external_normal_impulse_ns<<','
               <<r.face_impulse_updates<<','
               <<r.continuous_pair_tests<<','
               <<r.interval_nodes<<','<<r.subdivision_depth<<'\n';
        }
        std::ofstream manifest(p/"fast_mesh_manifest.json");
        manifest<<std::setprecision(17)
           <<"{\n"
           <<" \"true_nonplanar_rigid_triangle_collision\":true,\n"
           <<" \"old_sphere_contact_used\":false,\n"
           <<" \"normal_physical_time_not_skipped\":true,\n"
           <<" \"universal_stability_proven\":false,\n"
           <<" \"moving_rigid_obstacle_solved\":false,\n"
           <<" \"self_contact_solved\":false,\n"
           <<" \"incident_speed_m_per_s\":"<<speed<<",\n"
           <<" \"actual_initial_clearance_m\":"<<actual_initial<<",\n"
           <<" \"requested_physical_dt_s\":"<<dt<<",\n"
           <<" \"actual_accepted_physical_time_s\":"
               <<accepted.integrated_dt_s<<",\n"
           <<" \"unsafe_full_step_rejected\":true,\n"
           <<" \"geometrically_rejected_proposals\":"
               <<accepted.rejected_geom_trials<<",\n"
           <<" \"inverted_tetrahedron_trial_rejections\":"
               <<accepted.rejected_tetra_trials<<",\n"
           <<" \"accepted_physical_substeps\":"
               <<accepted.accepted_leaves<<",\n"
           <<" \"minimum_accepted_mesh_triangle_gap_m\":"
               <<minimum_gap<<",\n"
           <<" \"minimum_accepted_tet_jacobian\":"
               <<minJ<<",\n"
           <<" \"contact_impulse_updates\":"
               <<accepted.face_impulse_updates<<"\n"
           <<"}\n";
    }
    std::cout<<"PASS11_TRUE_ADAPTIVE_MESH_IMPACT"
             <<" speed_mps="<<speed
             <<" initial_gap_m="<<actual_initial
             <<" rejected_geom="<<accepted.rejected_geom_trials
             <<" rejected_tet="<<accepted.rejected_tetra_trials
             <<" accepted_leaves="<<accepted.accepted_leaves
             <<" impulse_updates="<<accepted.face_impulse_updates
             <<" closest_mesh_gap_m="<<minimum_gap
             <<" minJ="<<minJ
             <<" actual_integrated_time_s="<<accepted.integrated_dt_s
             <<'\n';
    return 0;
 }catch(const std::exception& e){
    std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;
 }
}
