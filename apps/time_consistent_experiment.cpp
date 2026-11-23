// Kyoto Pass 8: Native source evidence of actual elapsed-time adaptive
// re-integration for 3D tetrahedral FEM full-face sphere contact.
//
// The original Pass-7 control is deliberately kept unchanged.
// Main controlled specimen: bring the same rigid sphere to precisely
// 15 mm of the undeformed solid and give all 180 vertices a common
// incoming velocity of 50 m/s. Old scheme must CLIP displacement
// while charging dt; new method must REPLAY physical substeps for the
// complete dt. A 256-small-step same-model control is reported, but
// is NOT an independent solver/experimental ground truth.
//
// This C++ exporter generates the raw figures' evidence, never
// Python-generated physics or hand-authored metric values.

#include "kyoto/time_consistent_contact.hpp"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <numeric>
#include <stdexcept>
#include <string>
#include <vector>

using namespace kyoto::solid;
static void req(bool yes,const char* message){
    if(!yes)throw std::runtime_error(message);
}
static double mean_position_separation(const Body& a,const Body& b){
    req(a.nodes.size()==b.nodes.size(),"incomparable FEM topology");
    double accum=0;
    for(std::size_t i=0;i<a.nodes.size();i++)
        accum+=norm(a.nodes[i].x-b.nodes[i].x);
    return accum/a.nodes.size();
}
static double mean_velocity_separation(const Body& a,const Body& b){
    req(a.nodes.size()==b.nodes.size(),"incomparable FEM topology");
    double accum=0;
    for(std::size_t i=0;i<a.nodes.size();i++)
        accum+=norm(a.nodes[i].v-b.nodes[i].v);
    return accum/a.nodes.size();
}
static void source_ok(const Body& b,const RigidSphere& sphere){
    const auto audit=audit_surface_sphere(b,sphere);
    const auto fem=b.evaluate();
    req(audit.penetrating_faces==0&&audit.inside_vertex_count==0,
        "actual physical state contains intersecting sphere geometry");
    req(fem.min_jacobian>1e-6&&std::isfinite(fem.elastic_joule),
        "physical tetra inverted or invalid material energy");
}

int main(int argc,char**argv){
 try{
    if(argc!=2)
        throw std::invalid_argument(
           "usage: kyoto_time_consistent_export OUTPUT_DIR");
    const std::filesystem::path dir=argv[1];
    std::filesystem::create_directories(dir);
    constexpr double dt=.00075;
    const RigidSphere sphere{};
    Body regular=toroidal_solid();

    std::ofstream regular_csv(dir/"routine_900_physics.csv");
    regular_csv<<std::setprecision(17);
    regular_csv<<"step,time_s,requested_dt_s,actually_integrated_dt_s,"
                 "accepted_substeps,rejected_clipped_trials,"
                 "rejected_inverted_trials,full_face_interval_tests,"
                 "accepted_face_impulse_updates,normal_impulse_ns,"
                 "minimum_triangle_clearance_m,minimum_tetra_jacobian,"
                 "kinetic_j,elastic_j,gravity_j,com_x,com_y,com_z,"
                 "total_mass_kg\n";
    std::size_t routine_accept=0,routine_clips=0,routine_invalid=0;
    double routine_min_J=std::numeric_limits<double>::infinity();
    double routine_min_gap=std::numeric_limits<double>::infinity();
    for(int step=0;step<=900;step++){
        TimeConsistentMetrics r;
        if(step>0){
            r=step_time_consistent_whole_face_sphere(regular,sphere,dt);
            routine_accept+=r.accepted_substeps;
            routine_clips+=r.rejected_clipped_trials;
            routine_invalid+=r.rejected_inverted_trials;
        }
        source_ok(regular,sphere);
        const auto d=regular.diagnostics(-100);
        const auto audit=audit_surface_sphere(regular,sphere);
        routine_min_J=std::min(routine_min_J,d.min_jacobian);
        routine_min_gap=std::min(routine_min_gap,
                                 audit.minimum_surface_clearance_m);
        regular_csv<<step<<','<<regular.time_seconds<<','
                   <<(step==0?0:dt)<<','<<r.actually_integrated_dt_s<<','
                   <<r.accepted_substeps<<','
                   <<r.rejected_clipped_trials<<','
                   <<r.rejected_inverted_trials<<','
                   <<r.face_interval_tests<<','
                   <<r.accepted_face_impulse_updates<<','
                   <<r.normal_impulse_ns<<','
                   <<audit.minimum_surface_clearance_m<<','
                   <<d.min_jacobian<<','<<d.kinetic_joule<<','
                   <<d.elastic_joule<<','<<d.gravity_joule<<','
                   <<d.center_of_mass.x<<','<<d.center_of_mass.y<<','
                   <<d.center_of_mass.z<<','
                   <<d.total_mass_kg<<'\n';
    }
    regular_csv.close();

    // Ensure the 50 m/s real-motion experiment begins a known
    // positive 15 mm distance from the WHOLE starting triangle mesh.
    Body initial=toroidal_solid();
    RigidSphere near_sphere=sphere;
    const double original_gap=audit_surface_sphere(initial,near_sphere)
                                  .minimum_surface_clearance_m;
    near_sphere.radius+=original_gap-.015;
    req(std::abs(audit_surface_sphere(initial,near_sphere)
                 .minimum_surface_clearance_m-.015)<1e-10,
        "incorrect nonplanar close-collision fixture");
    for(auto& n:initial.nodes)n.v={0,0,-50};

    Body old=initial,adaptive=initial,fine=initial;
    const auto old_result=step_whole_face_sphere(old,near_sphere,dt);
    req(old_result.time_clipped&&old_result.accepted_global_fraction<1,
        "negative control did not use the old time-skipping shortcut");
    std::vector<AcceptedPhysicalSubstep> receipts;
    const auto result=step_time_consistent_whole_face_sphere(
        adaptive,near_sphere,dt,{},&receipts);
    req(result.rejected_clipped_trials>0&&result.accepted_substeps>1,
        "adaptive replay did not resolve a real potential contact");
    req(std::abs(result.actually_integrated_dt_s-dt)<1e-12,
        "adaptive solver failed to integrate the COMPLETE physical time");
    req(std::abs(adaptive.time_seconds-dt)<1e-12,
        "adaptive solver macro clock wrong");
    source_ok(old,near_sphere);source_ok(adaptive,near_sphere);

    constexpr int ref_steps=256;
    double fine_normal_impulse=0;
    std::size_t fine_substeps=0;
    for(int k=0;k<ref_steps;k++){
        const auto r=step_time_consistent_whole_face_sphere(
            fine,near_sphere,dt/ref_steps);
        fine_normal_impulse+=r.normal_impulse_ns;
        fine_substeps+=r.accepted_substeps;
        source_ok(fine,near_sphere);
    }
    req(std::abs(fine.time_seconds-dt)<1e-12,
        "fine numerical reference missing physical time");
    const auto d_old=old.diagnostics(-100);
    const auto d_adaptive=adaptive.diagnostics(-100);
    const auto d_fine=fine.diagnostics(-100);

    std::ofstream receipt_csv(dir/"fast_adaptive_accepted_substeps.csv");
    receipt_csv<<std::setprecision(17);
    receipt_csv<<"substep,start_time_s,dt_s,end_time_s,"
                 "minimum_full_triangle_clearance_m,"
                 "minimum_tetra_jacobian,normal_impulse_ns,"
                 "tangential_impulse_ns,face_interval_tests,"
                 "face_contact_updates,bisection_depth\n";
    double measured_dt=0;
    for(std::size_t i=0;i<receipts.size();i++){
        const auto& r=receipts[i];
        receipt_csv<<i<<','<<r.start_time_s<<','
                   <<r.dt_s<<','<<r.end_time_s<<','
                   <<r.minimum_full_triangle_clearance_m<<','
                   <<r.minimum_tetra_jacobian<<','
                   <<r.normal_impulse_ns<<','
                   <<r.tangential_impulse_ns<<','
                   <<r.face_interval_tests<<','
                   <<r.face_contact_updates<<','
                   <<r.bisection_depth<<'\n';
        measured_dt+=r.dt_s;
    }
    req(std::abs(measured_dt-dt)<1e-12,
        "physical subdivision receipt does not cover full time");
    receipt_csv.close();

    const auto comparison_file=dir/"fast_original_vs_adaptive_vs_fine.csv";
    std::ofstream comp(comparison_file);
    comp<<std::setprecision(17);
    comp<<"algorithm,end_time_s,verified_integrated_elapsed_time_s_or_nan,"
           "accepted_full_substeps,clipped_fraction_of_old_trial,"
           "mean_position_difference_from_fine_m,"
           "mean_velocity_difference_from_fine_m_per_s,"
           "minimum_triangle_clearance_m,min_tetra_jacobian,"
           "kinetic_j,elastic_j,gravity_j,normal_impulse_ns\n";
    const auto report=[&](const char* name,const Body& state,
                         double elapsed,int leaves,double old_fraction,
                         double impulse){
        const auto geom=audit_surface_sphere(state,near_sphere);
        const auto d=state.diagnostics(-100);
        comp<<name<<','<<state.time_seconds<<','<<elapsed<<','
            <<leaves<<','<<old_fraction<<','
            <<mean_position_separation(state,fine)<<','
            <<mean_velocity_separation(state,fine)<<','
            <<geom.minimum_surface_clearance_m<<','
            <<d.min_jacobian<<','<<d.kinetic_joule<<','
            <<d.elastic_joule<<','<<d.gravity_joule<<','
            <<impulse<<'\n';
    };
    // The first row preserves the physical shortcut, rather than
    // falsely calling clipped displacement a complete integrated dt.
    report("legacy_global_clip",old,std::numeric_limits<double>::quiet_NaN(),
           1,old_result.accepted_global_fraction,
           old_result.total_normal_impulse_ns);
    report("adaptive_full_elapsed_time",adaptive,
           result.actually_integrated_dt_s,
           int(result.accepted_substeps),1.,
           result.normal_impulse_ns);
    report("fine_256_same_model",fine,dt,int(fine_substeps),1.,
           fine_normal_impulse);
    comp.close();


    // Preserve the ORIGINAL physical 3D vertices and material stress
    // at each solver outcome. A renderer can show the actual
    // divergent contact trajectories without inventing positions.
    std::ofstream state_csv(dir/"fast_event_true_3d_states.csv");
    state_csv<<std::setprecision(17);
    state_csv<<"algorithm,vertex_id,rest_x,rest_y,rest_z,"
               "x,y,z,vx,vy,vz,von_mises_pa,displacement_m\n";
    const auto store_state=[&](const char* name,const Body& b){
        const auto fields=b.evaluate();
        for(std::size_t i=0;i<b.nodes.size();i++){
            const auto& n=b.nodes[i];
            state_csv<<name<<','<<i<<','<<n.rest.x<<','<<n.rest.y
                     <<','<<n.rest.z<<','<<n.x.x<<','<<n.x.y<<','
                     <<n.x.z<<','<<n.v.x<<','<<n.v.y<<','<<n.v.z
                     <<','<<fields.nodal_von_mises_pa[i]<<','
                     <<norm(n.x-n.rest)<<'\n';
        }
    };
    store_state("initial",initial);
    store_state("legacy_global_clip",old);
    store_state("adaptive_full_elapsed_time",adaptive);
    store_state("fine_256_same_model",fine);
    state_csv.close();

    std::ofstream topology_csv(dir/"fast_event_surface_faces.csv");
    topology_csv<<"face_id,a,b,c\n";
    for(std::size_t i=0;i<initial.surface_faces.size();i++){
        const auto f=initial.surface_faces[i];
        topology_csv<<i<<','<<f[0]<<','<<f[1]<<','<<f[2]<<'\n';
    }
    topology_csv.close();

    std::ofstream manifest(dir/"manifest.json");
    manifest<<std::setprecision(17);
    manifest<<"{\n"
            <<" \"scene\":\"time_consistent_replay_on_real_3D_hyperelastic_torus\",\n"
            <<" \"actual_solver_physics\":true,\n"
            <<" \"full_face_interval_certificate\":\"Lipschitz_binary64_not_formally_rounded\",\n"
            <<" \"time_integrator\":\"adaptive_halfstep_replay_of_semi_implicit_euler\",\n"
            <<" \"arbitrary_mesh_contact\":false,\n"
            <<" \"implicit_coupled_contact\":false,\n"
            <<" \"self_contact_response\":false,\n"
            <<" \"same_model_fine_reference_is_independent_solver\":false,\n"
            <<" \"ordinary_steps\":900,\n"
            <<" \"ordinary_elapsed_time_s\":"<<regular.time_seconds<<",\n"
            <<" \"ordinary_accepted_substeps\":"<<routine_accept<<",\n"
            <<" \"ordinary_rejected_clipped_trials\":"<<routine_clips<<",\n"
            <<" \"ordinary_rejected_inverted_trials\":"<<routine_invalid<<",\n"
            <<" \"ordinary_minimum_triangle_clearance_m\":"<<routine_min_gap<<",\n"
            <<" \"ordinary_minimum_tetra_jacobian\":"<<routine_min_J<<",\n"
            <<" \"fast_trial_dt_s\":"<<dt<<",\n"
            <<" \"fast_initial_whole_surface_clearance_m\":0.015,\n"
            <<" \"fast_uniform_initial_velocity_mps\":50,\n"
            <<" \"old_global_accepted_fraction\":"<<old_result.accepted_global_fraction<<",\n"
            <<" \"old_displacement_time_s\":"<<old_result.accepted_global_fraction*dt<<",\n"
            <<" \"old_charged_clock_time_s\":"<<old.time_seconds<<",\n"
            <<" \"adaptive_rejected_clipped_proposals\":"<<result.rejected_clipped_trials<<",\n"
            <<" \"adaptive_rejected_inverted_proposals\":"<<result.rejected_inverted_trials<<",\n"
            <<" \"adaptive_accepted_physical_substeps\":"<<result.accepted_substeps<<",\n"
            <<" \"adaptive_deepest_bisection\":"<<result.deepest_bisection<<",\n"
            <<" \"adaptive_total_integrated_time_s\":"<<result.actually_integrated_dt_s<<",\n"
            <<" \"fine_same_model_steps\":"<<ref_steps<<",\n"
            <<" \"fine_actual_substeps\":"<<fine_substeps<<",\n"
            <<" \"old_mean_position_error_against_fine_m\":"<<mean_position_separation(old,fine)<<",\n"
            <<" \"adaptive_mean_position_error_against_fine_m\":"<<mean_position_separation(adaptive,fine)<<",\n"
            <<" \"old_mean_velocity_error_against_fine_mps\":"<<mean_velocity_separation(old,fine)<<",\n"
            <<" \"adaptive_mean_velocity_error_against_fine_mps\":"<<mean_velocity_separation(adaptive,fine)<<"\n"
            <<"}\n";
    if(!manifest.good()||!regular_csv.good()||!comp.good()||
       !receipt_csv.good()||!state_csv.good()||!topology_csv.good())
        throw std::runtime_error("failed writing original C++ timestep evidence");
    std::cout<<"PASS: real FEM adaptive elapsed-time integration"
             <<" regular_steps=900 regular_time="<<regular.time_seconds
             <<" fast_rejected_clipped="<<result.rejected_clipped_trials
             <<" fast_rejected_inverted="<<result.rejected_inverted_trials
             <<" fast_accepted_substeps="<<result.accepted_substeps
             <<" old_nominal_motion_fraction_times_dt_s="
             <<old_result.accepted_global_fraction*dt
             <<" adaptive_integrated_time_s="<<result.actually_integrated_dt_s
             <<" old_pos_difference_from_fine_m="<<mean_position_separation(old,fine)
             <<" adaptive_pos_difference_from_fine_m="<<mean_position_separation(adaptive,fine)
             <<" (fine is same model, NOT independent solver)\n";
    return 0;
 }catch(const std::exception& e){
    std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;
 }
}
