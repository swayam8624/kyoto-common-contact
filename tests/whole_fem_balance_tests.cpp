// Kyoto Pass 10: actual WHOLE-FEM per-accepted-substep 3D momentum ledger.
//
// The local Pass-9 test checked one isolated impulse. Here we audit the
// ENTIRE integrated 180-node/480-tet physical step with gravity, damping,
// internally balanced neo-Hookean forces and REAL sphere impulses.
//
// With old positions x, nodal mass m, elastic force f, gravity g,
// alpha=exp(-damping*dt), and recorded external contact vector J:
//
//    P_next = alpha [P_old + dt sum_i(f_i + m_i g)] + J.
//
// Angular momentum about the world origin requires one extra term
// because the Euler step MOVES the nodes while applying impulses:
//
//    L_next = alpha [L_old+dt sum_i(x_old_i x F_i)]
//             + tau_J(at old nodal positions)
//             + sum_i[(x_new_i-x_old_i) x (m_i*v_new_i)].
//
// The last displacement-angular term is a discrete kinematic transport
// term, NOT a physically introduced impulse. The ledger explicitly
// includes it rather than falsely claiming exact angular conservation.
// We also independently measure sum(f_elastic) and sum(x x f_elastic).
//
// This experiment is not an independent proof of physical accuracy,
// energy conservation or general mesh-contact dynamics: it proves
// algebraic accounting of the ACTUAL discrete numerical method.

#include "kyoto/time_consistent_contact.hpp"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

using namespace kyoto::solid;

static void need(bool pass,const char* why){
    if(!pass)throw std::runtime_error(why);
}
static V3 linear_momentum(const Body& b){
    V3 p{};
    for(const auto& n:b.nodes)p+=n.v*n.mass;
    return p;
}
static V3 angular_momentum(const Body& b){
    V3 l{};
    for(const auto& n:b.nodes)
        l+=cross(n.x,n.v*n.mass);
    return l;
}
struct Summary {
    std::size_t leaves{},with_contact{};
    double max_absolute_momentum_residual_ns{};
    double max_absolute_angular_residual_nsm{};
    double max_relative_momentum_residual{};
    double max_relative_angular_residual{};
    double max_internal_force_residual_N{};
    double max_internal_torque_residual_Nm{};
    double max_internal_force_relative{};
    double max_internal_torque_relative{};
    double largest_mechanical_energy_gain_j{};
    double largest_mechanical_energy_loss_j{};
    double total_mechanical_energy_change_j{};
    double min_triangle_gap_m{std::numeric_limits<double>::infinity()};
};
static void audit_integrated_leaf(
    const Body& old,const Body& next,
    const WholeFaceStepMetrics& contact,double dt,
    Summary& stats,std::ostream* csv,
    const char* scenario,std::size_t step,std::size_t leaf) {
    need(old.nodes.size()==next.nodes.size(),
         "source FEM topology changed");
    const auto fields=old.evaluate();
    const double damping=std::exp(-old.material.damping*dt);
    V3 force{},torque{},net_elastic{},elastic_torque{},transport{};
    double elastic_force_total=0,elastic_torque_total=0;
    for(std::size_t i=0;i<old.nodes.size();i++){
        const auto& node=old.nodes[i];
        const auto& after=next.nodes[i];
        const V3 elastic=fields.elastic_force[i];
        net_elastic+=elastic;
        elastic_torque+=cross(node.x,elastic);
        elastic_force_total+=norm(elastic);
        elastic_torque_total+=norm(cross(node.x,elastic));
        const V3 external_gravity{0,0,-old.material.gravity*node.mass};
        const V3 net=elastic+external_gravity;
        force+=net;
        torque+=cross(node.x,net);
        transport+=cross(after.x-node.x,after.v*after.mass);
    }
    const V3 p_old=linear_momentum(old);
    const V3 l_old=angular_momentum(old);
    const V3 p_new=linear_momentum(next);
    const V3 l_new=angular_momentum(next);
    const V3 p_pred=(p_old+force*dt)*damping+
                     contact.net_contact_impulse_vector;
    const V3 l_pred=(l_old+torque*dt)*damping+
                     contact.net_contact_torque_about_step_start+
                     transport;
    const double residual_p=norm(p_new-p_pred);
    const double residual_l=norm(l_new-l_pred);
    const double p_scale=std::max({1.,norm(p_old),norm(p_new),
                                  norm(p_pred),norm(force*dt),
                                  norm(contact.net_contact_impulse_vector)});
    const double l_scale=std::max({1.,norm(l_old),norm(l_new),
                                  norm(l_pred),norm(torque*dt),
                                  norm(transport)});
    const double relative_p=residual_p/p_scale;
    const double relative_l=residual_l/l_scale;
    const double elastic_force_relative=
        norm(net_elastic)/std::max(1.,elastic_force_total);
    const double elastic_torque_relative=
        norm(elastic_torque)/std::max(1.,elastic_torque_total);
    need(std::isfinite(residual_p)&&std::isfinite(residual_l),
         "nonfinite whole-step momentum residual");
    need(relative_p<1e-10,"whole-step external impulse P ledger broken");
    need(relative_l<1e-10,
         "whole-step internal/external angular ledger broken");
    need(elastic_force_relative<1e-10,
         "neo-Hookean internal force fails exact equilibrium test");
    need(elastic_torque_relative<1e-10,
         "neo-Hookean internal torque fails stress symmetry test");
    const auto before=old.diagnostics(0);
    const auto after=next.diagnostics(0);
    const double energy0=before.kinetic_joule+
                         before.elastic_joule+before.gravity_joule;
    const double energy1=after.kinetic_joule+
                         after.elastic_joule+after.gravity_joule;
    const double delta=energy1-energy0;
    need(std::isfinite(delta),"nonfinite entire FEM energy change");
    const auto geometry=audit_surface_sphere(next,RigidSphere{});
    // The rigid sphere for fast contact has modified radius, so
    // independent safety is checked at call-site with its real collider.
    (void)geometry;
    stats.leaves++;
    stats.with_contact+=contact.normal_impulses>0;
    stats.max_absolute_momentum_residual_ns=std::max(
        stats.max_absolute_momentum_residual_ns,residual_p);
    stats.max_absolute_angular_residual_nsm=std::max(
        stats.max_absolute_angular_residual_nsm,residual_l);
    stats.max_relative_momentum_residual=std::max(
        stats.max_relative_momentum_residual,relative_p);
    stats.max_relative_angular_residual=std::max(
        stats.max_relative_angular_residual,relative_l);
    stats.max_internal_force_residual_N=std::max(
        stats.max_internal_force_residual_N,norm(net_elastic));
    stats.max_internal_torque_residual_Nm=std::max(
        stats.max_internal_torque_residual_Nm,norm(elastic_torque));
    stats.max_internal_force_relative=std::max(
        stats.max_internal_force_relative,elastic_force_relative);
    stats.max_internal_torque_relative=std::max(
        stats.max_internal_torque_relative,elastic_torque_relative);
    stats.largest_mechanical_energy_gain_j=std::max(
        stats.largest_mechanical_energy_gain_j,delta);
    stats.largest_mechanical_energy_loss_j=std::max(
        stats.largest_mechanical_energy_loss_j,-delta);
    stats.total_mechanical_energy_change_j+=delta;
    if(csv){
        (*csv)<<scenario<<','<<step<<','<<leaf<<','
              <<next.time_seconds<<','<<dt<<','
              <<residual_p<<','<<residual_l<<','
              <<relative_p<<','<<relative_l<<','
              <<norm(net_elastic)<<','<<norm(elastic_torque)<<','
              <<elastic_force_relative<<','<<elastic_torque_relative<<','
              <<norm(contact.net_contact_impulse_vector)<<','
              <<norm(contact.net_contact_torque_at_application)<<','
              <<norm(transport)<<','
              <<contact.normal_impulses<<','
              <<energy0<<','<<energy1<<','<<delta<<'\n';
    }
}
int main(int argc,char**argv){
 try{
    if(argc>2)throw std::invalid_argument(
       "usage: kyoto_full_fem_balance_tests [optional evidence directory]");
    std::ofstream csv;
    if(argc==2){
        std::filesystem::create_directories(argv[1]);
        csv.open(std::filesystem::path(argv[1])/
                 "whole_FEM_accepted_substep_balance.csv");
        csv<<std::setprecision(17);
        csv<<"scenario,macro_step,leaf,time_s,physical_dt_s,"
             "momentum_balance_ns,angular_balance_nsm,"
             "momentum_relative,angular_relative,"
             "internal_force_balance_N,internal_torque_balance_Nm,"
             "internal_force_relative,internal_torque_relative,"
             "real_external_contact_vector_impulse_norm_ns,"
             "real_external_contact_angular_impulse_norm_nsm,"
             "discrete_angular_transport_norm_nsm,"
             "face_contact_updates,mechanical_energy_before_j,"
             "mechanical_energy_after_j,mechanical_energy_change_j\n";
    }
    Summary routine,fast;
    Body body=toroidal_solid();
    const RigidSphere sphere{};
    for(std::size_t macro=1;macro<=900;macro++){
        Body old=body;
        std::vector<AcceptedPhysicalSubstep> receipts;
        const auto actual=step_time_consistent_whole_face_sphere(
            body,sphere,.00075,{},&receipts);
        need(receipts.size()==actual.accepted_substeps,
             "routine physics receipt mismatch");
        for(std::size_t leaf=0;leaf<receipts.size();leaf++){
            Body next=old;
            const auto contact=step_whole_face_sphere(
                next,sphere,receipts[leaf].dt_s);
            need(!contact.time_clipped,
                 "routine audited physical leaf was clipped");
            const auto audit=audit_surface_sphere(next,sphere);
            need(audit.penetrating_faces==0,
                 "routine full FEM has geometric penetration");
            routine.min_triangle_gap_m=std::min(
                routine.min_triangle_gap_m,
                audit.minimum_surface_clearance_m);
            audit_integrated_leaf(old,next,contact,
                receipts[leaf].dt_s,routine,
                csv.is_open()?&csv:nullptr,"routine",macro,leaf);
            old=std::move(next);
        }
        need(std::abs(old.time_seconds-body.time_seconds)<1e-10,
             "replayed 900-step FEM physical clock differs");
    }
    need(routine.leaves==900&&routine.with_contact>0,
         "whole-step FEM routine noncontact or missing leaves");
    Body fastbody=toroidal_solid();
    RigidSphere tuned{};
    tuned.radius+=audit_surface_sphere(fastbody,tuned)
                      .minimum_surface_clearance_m-.015;
    for(auto& node:fastbody.nodes)node.v={0,0,-50};
    Body fast_previous=fastbody;
    std::vector<AcceptedPhysicalSubstep> fast_receipts;
    const auto faststep=step_time_consistent_whole_face_sphere(
        fastbody,tuned,.00075,{},&fast_receipts);
    need(faststep.rejected_clipped_trials>0&&
         fast_receipts.size()>1,
         "fast replay does not contain physical subdivision");
    for(std::size_t leaf=0;leaf<fast_receipts.size();leaf++){
        Body next=fast_previous;
        const auto r=step_whole_face_sphere(
            next,tuned,fast_receipts[leaf].dt_s);
        need(!r.time_clipped,"fast physical leaf clipped");
        const auto state=audit_surface_sphere(next,tuned);
        need(state.penetrating_faces==0,
             "fast full FEM has triangle penetration");
        fast.min_triangle_gap_m=std::min(
            fast.min_triangle_gap_m,
            state.minimum_surface_clearance_m);
        audit_integrated_leaf(fast_previous,next,r,
            fast_receipts[leaf].dt_s,fast,
            csv.is_open()?&csv:nullptr,"fast",0,leaf);
        fast_previous=std::move(next);
    }
    need(fast.leaves==fast_receipts.size()&&
         std::abs(fast_previous.time_seconds-fastbody.time_seconds)<1e-10,
         "fast full-step actual momentum comparison missing physical time");
    if(argc==2){
        std::ofstream manifest(std::filesystem::path(argv[1])/
                               "whole_fem_balance_manifest.json");
        manifest<<std::setprecision(17)
            <<"{\n"
            <<" \"scope\":\"entire_discrete_neohookean_FEM_with_gravity_damping_and_external_sphere_impulse\",\n"
            <<" \"true_physics_calibration_proven\":false,\n"
            <<" \"global_energy_conservation_required\":false,\n"
            <<" \"angular_transport_term_included\":true,\n"
            <<" \"routine_accepted_physical_substeps\":"<<routine.leaves<<",\n"
            <<" \"routine_leaves_with_contact\":"<<routine.with_contact<<",\n"
            <<" \"fast_accepted_physical_substeps\":"<<fast.leaves<<",\n"
            <<" \"routine_max_momentum_residual_ns\":"
               <<routine.max_absolute_momentum_residual_ns<<",\n"
            <<" \"routine_max_angular_residual_nsm\":"
               <<routine.max_absolute_angular_residual_nsm<<",\n"
            <<" \"fast_max_momentum_residual_ns\":"
               <<fast.max_absolute_momentum_residual_ns<<",\n"
            <<" \"fast_max_angular_residual_nsm\":"
               <<fast.max_absolute_angular_residual_nsm<<",\n"
            <<" \"routine_max_relative_momentum_residual\":"
               <<routine.max_relative_momentum_residual<<",\n"
            <<" \"routine_max_relative_angular_residual\":"
               <<routine.max_relative_angular_residual<<",\n"
            <<" \"fast_max_relative_momentum_residual\":"
               <<fast.max_relative_momentum_residual<<",\n"
            <<" \"fast_max_relative_angular_residual\":"
               <<fast.max_relative_angular_residual<<",\n"
            <<" \"routine_max_internal_force_relative\":"
               <<routine.max_internal_force_relative<<",\n"
            <<" \"routine_max_internal_torque_relative\":"
               <<routine.max_internal_torque_relative<<",\n"
            <<" \"fast_max_internal_force_relative\":"
               <<fast.max_internal_force_relative<<",\n"
            <<" \"fast_max_internal_torque_relative\":"
               <<fast.max_internal_torque_relative<<",\n"
            <<" \"routine_max_observed_mechanical_energy_gain_j\":"
               <<routine.largest_mechanical_energy_gain_j<<",\n"
            <<" \"routine_max_observed_mechanical_energy_loss_j\":"
               <<routine.largest_mechanical_energy_loss_j<<",\n"
            <<" \"routine_total_mechanical_energy_change_j\":"
               <<routine.total_mechanical_energy_change_j<<",\n"
            <<" \"fast_total_mechanical_energy_change_j\":"
               <<fast.total_mechanical_energy_change_j<<"\n"
            <<"}\n";
    }
    std::cout<<"PASS: full 3D FEM discrete momentum ledger"
             <<" routine_leaves="<<routine.leaves
             <<" fast_leaves="<<fast.leaves
             <<" routine_max_relative_P="<<routine.max_relative_momentum_residual
             <<" routine_max_relative_L="<<routine.max_relative_angular_residual
             <<" fast_max_relative_P="<<fast.max_relative_momentum_residual
             <<" fast_max_relative_L="<<fast.max_relative_angular_residual
             <<" energy_is_not_conserved_by_friction_damping_and_time_discretization=true\n";
    return 0;
 }catch(const std::exception& e){
    std::cerr<<"FAIL: "<<e.what()<<'\n';
    return 1;
 }
}
