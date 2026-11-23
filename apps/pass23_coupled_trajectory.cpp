// Pass23B: sustained REAL coupled two-deformable volumetric FEM
// trajectory. Both bodies are integrated from original state, both
// react to mutual finite-triangle contact, and unsafe proposed leaves
// are actually rejected and reintegrated at smaller physical dt.
//
// Original CSV is the ONLY source for subsequent figures. A printed
// momentum residual is an audit, not proof of calibrated continuum
// material physics, general IPC event correctness, or solver convergence.
#include "kyoto/coupled_deformable_fem_contact.hpp"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

namespace s=kyoto::solid;

static s::Body placed(double offset,double vx) {
    auto body=s::toroidal_solid(8);
    for(auto& n:body.nodes){
        n.x.x+=offset;n.rest.x+=offset;
        n.v={vx,0,0};
    }
    return body;
}
static double total_mass(const s::Body& body){
    double mass=0.;
    for(const auto& n:body.nodes)mass+=n.mass;
    return mass;
}
static s::V3 momentum(const s::Body& a,const s::Body& b){
    s::V3 result{};
    for(const auto& n:a.nodes)result+=n.v*n.mass;
    for(const auto& n:b.nodes)result+=n.v*n.mass;
    return result;
}
static double kinetic(const s::Body& a,const s::Body& b){
    double value=0.;
    for(const auto& n:a.nodes)value+=.5*n.mass*s::dot(n.v,n.v);
    for(const auto& n:b.nodes)value+=.5*n.mass*s::dot(n.v,n.v);
    return value;
}
static double grav_potential(const s::Body& a,const s::Body& b){
    double value=0.;
    for(const auto& n:a.nodes)value+=n.mass*a.material.gravity*n.x.z;
    for(const auto& n:b.nodes)value+=n.mass*b.material.gravity*n.x.z;
    return value;
}
int main(int argc,char**argv){
 try{
    if(argc!=2)throw std::invalid_argument(
        "usage: kyoto_pass23_coupled_trajectory OUTPUT_DIRECTORY");
    const auto output=std::filesystem::path(argv[1]);
    std::filesystem::create_directories(output);
    std::ofstream csv(output/"real_coupled_deformable_contact.csv");
    if(!csv)throw std::runtime_error("failed to write original two-body physics");
    csv<<std::setprecision(17);
    csv<<"macro_step,time_start_s,time_end_s,integrated_dt_s,"
          "accepted_physical_leaves,rejected_moving_ccd_trials,"
          "rejected_inverted_tet_trials,mutual_contact_impulse_updates,"
          "minimum_accepted_face_gap_m,minimum_tetra_j,"
          "impulse_A_x_ns,impulse_A_y_ns,impulse_A_z_ns,"
          "impulse_B_x_ns,impulse_B_y_ns,impulse_B_z_ns,"
          "maximum_whole_system_momentum_residual_ns,"
          "kinetic_energy_j,volumetric_elastic_energy_j,"
          "gravity_potential_j,combined_mechanical_energy_j\n";

    auto a=placed(-.91,+.5);
    auto b=placed(+.91,-.5);
    s::CoupledDeformableSettings cfg;
    cfg.activation_width_m=.015;
    cfg.friction=.0;
    cfg.restitution=0.;
    cfg.impulse_iterations=2;
    constexpr int STEPS=120;
    constexpr double DT=.0005;
    std::size_t total_contacts=0,total_rejected=0,total_leaves=0;
    double min_gap=std::numeric_limits<double>::infinity();
    double min_j=std::numeric_limits<double>::infinity();
    double worst_momentum=0.;
    for(int step=1;step<=STEPS;++step){
        const double t0=a.time_seconds;
        std::vector<s::CoupledFEMAcceptedLeaf> accepted;
        const auto stats=s::step_time_consistent_coupled_deformables(
            a,b,DT,cfg,&accepted);
        if(accepted.empty()||accepted.size()!=stats.accepted_leaves)
            throw std::runtime_error("original coupled physical leaf audit missing");
        if(!(std::abs(stats.integrated_dt_s-DT)<1e-12 &&
             std::abs(a.time_seconds-b.time_seconds)<1e-12 &&
             std::abs(a.time_seconds-t0-DT)<1e-12))
            throw std::runtime_error("source coupled physical clock drift");
        double maximum_residual=0.;
        for(const auto& leaf:accepted){
            const auto before=momentum(leaf.a_before,leaf.b_before);
            const auto after=momentum(leaf.a_after,leaf.b_after);
            const double dt=leaf.receipt.dt_s;
            const double damping=a.material.damping;
            if(std::abs(b.material.damping-damping)>1e-12 ||
               std::abs(a.material.gravity-b.material.gravity)>1e-12)
                throw std::runtime_error(
                    "momentum audit assumes matched damping and gravity");
            const double mass=total_mass(leaf.a_before)+
                              total_mass(leaf.b_before);
            const auto expected=(before+s::V3{
                0,0,-mass*a.material.gravity*dt})*
                std::exp(-damping*dt);
            const auto difference=s::norm(after-expected);
            maximum_residual=std::max(maximum_residual,difference);
            const auto scale=std::max(1.,s::norm(expected));
            if(difference>1e-7*scale)
                throw std::runtime_error(
                    "real coupled physical leaf lost whole-body momentum balance");
            if(s::norm(leaf.receipt.impulse_on_a+
                       leaf.receipt.impulse_on_b)>1e-9*scale)
                throw std::runtime_error(
                    "real coupled physical leaf lost opposite contact reaction");
            if(!(leaf.receipt.min_face_gap_m>cfg.skin_m*.5 &&
                 leaf.receipt.min_tet_j>1e-6))
                throw std::runtime_error(
                    "source accepted physical leaf intersects or inverted");
        }
        const auto e_a=a.evaluate(),e_b=b.evaluate();
        const double e_kin=kinetic(a,b);
        const double e_el=e_a.elastic_joule+e_b.elastic_joule;
        const double e_gravity=grav_potential(a,b);
        csv<<step<<','<<t0<<','<<a.time_seconds<<','
           <<stats.integrated_dt_s<<','<<stats.accepted_leaves<<','
           <<stats.rejected_ccd_trials<<','
           <<stats.rejected_tet_trials<<','<<stats.impulse_updates<<','
           <<stats.min_accepted_face_gap_m<<','
           <<stats.min_accepted_tet_j<<','
           <<stats.impulse_on_a.x<<','<<stats.impulse_on_a.y<<','
           <<stats.impulse_on_a.z<<','<<stats.impulse_on_b.x<<','
           <<stats.impulse_on_b.y<<','<<stats.impulse_on_b.z<<','
           <<maximum_residual<<','<<e_kin<<','<<e_el<<','
           <<e_gravity<<','<<e_kin+e_el+e_gravity<<'\n';
        total_contacts+=stats.impulse_updates;
        total_rejected+=stats.rejected_ccd_trials+stats.rejected_tet_trials;
        total_leaves+=stats.accepted_leaves;
        min_gap=std::min(min_gap,stats.min_accepted_face_gap_m);
        min_j=std::min(min_j,stats.min_accepted_tet_j);
        worst_momentum=std::max(worst_momentum,maximum_residual);
    }
    if(total_contacts==0)
        throw std::runtime_error(
            "actual two-deformable trajectory never had real mutual contact");
    if(!csv.good())throw std::runtime_error("incomplete coupled original CSV");
    std::cout<<"PASS23B actual coupled 3D FEM macro-steps="<<STEPS
             <<" physical_end_time="<<a.time_seconds
             <<" accepted_leaves="<<total_leaves
             <<" rejected_trials="<<total_rejected
             <<" mutual_finite_face_impulses="<<total_contacts
             <<" minimum_face_gap_m="<<min_gap
             <<" minimum_tetra_J="<<min_j
             <<" maximum_original_momentum_residual="<<worst_momentum
             <<"; no general IPC or end-to-end speedup claim.\n";
    return 0;
 }catch(const std::exception& e){
    std::cerr<<"PASS23B sustained coupled FEM FAILED: "
             <<e.what()<<'\n';
    return 1;
 }
}
