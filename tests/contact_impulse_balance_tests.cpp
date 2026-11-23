// Pass 9: independent contact impulse/energy/momentum law audit.
//
// The actual Pass-7 whole-face contact function is evaluated against
// conservation identities computed separately from its returned velocities.
// We audit LOCAL 3D face impulses, not claim that the entire explicit
// FEM/damped/gravitational system conserves total mechanical energy.
//
// For barycentric contact w_i and vertex masses m_i, define:
//     k = sum_i w_i^2/m_i; u = sum_i w_i v_i.
//     J = J_n*n - J_t*(u_t/||u_t||).
// Then, for this SINGLE impulse and frozen contact positions:
//     delta_P = J,
//     delta_L_about_origin = q x J,
//     delta_K = u dot J + (k/2)||J||^2 <= 0 for e<=1 and
//               physically limited friction impulse.
// These independent identities test the actual implementation.

#include "kyoto/independent_triangle_distance.hpp"
#include "kyoto/whole_face_contact_dynamics.hpp"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <numbers>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

using namespace kyoto::solid;
static void must(bool yes,const char* why) {
    if(!yes)throw std::runtime_error(why);
}
static V3 momentum(const Body& b,const std::vector<V3>& velocities){
    V3 P{};
    for(std::size_t i=0;i<b.nodes.size();i++)
        P+=velocities[i]*b.nodes[i].mass;
    return P;
}
static V3 angular(const Body& b,const std::vector<V3>& velocities){
    V3 L{};
    for(std::size_t i=0;i<b.nodes.size();i++)
        L+=cross(b.nodes[i].x,velocities[i]*b.nodes[i].mass);
    return L;
}
static double kinetic(const Body& b,const std::vector<V3>& velocities){
    double K=0;
    for(std::size_t i=0;i<b.nodes.size();i++)
        K+=.5*b.nodes[i].mass*dot(velocities[i],velocities[i]);
    return K;
}
int main(int argc,char**argv){
 try{
    if(argc>2)throw std::invalid_argument(
        "usage: kyoto_impulse_balance_tests [optional output directory]");
    std::ofstream csv;
    if(argc==2){
        std::filesystem::create_directories(argv[1]);
        csv.open(std::filesystem::path(argv[1])/"impulse_balance_cases.csv");
        csv<<std::setprecision(17);
        csv<<"case_id,restitution,friction,k_inverse_mass,"
               "incoming_normal_velocity_mps,incoming_tangent_speed_mps,"
               "normal_impulse_ns,tangential_impulse_ns,"
               "measured_delta_kinetic_j,analytical_delta_kinetic_j,"
               "momentum_balance_residual_ns,angular_balance_residual_nsm,"
               "kinetic_before_j,kinetic_after_j\n";
    }
    std::mt19937_64 rng(761982026);
    std::uniform_real_distribution<double> choose(-1.,1.),
      angle(0.,2*std::numbers::pi),position(-4.,4.),
      mass(0.3,10.),speed(.3,35.),
      restitution(0.,1.),friction(0.,1.);
    double worst_energy_residual=0,worst_linear_residual=0,
           worst_angular_residual=0,max_positive_delta=0,
           max_distance_oracle_disagreement=0;
    std::size_t executed=0;
    for(int run=0;run<1200;run++){
        // Arbitrary rigid world rotation plus translation checks that
        // a local z-normal implementation is NOT being special-cased.
        V3 axis{choose(rng),choose(rng),choose(rng)};
        if(norm(axis)<1e-3)axis={0,1,0};
        axis=axis/norm(axis);
        const double theta=angle(rng),s=std::sin(theta),c=std::cos(theta);
        const auto rot=[&](V3 v){
            return v*c+cross(axis,v)*s+
                   axis*(dot(axis,v)*(1-c));
        };
        const V3 center{position(rng),position(rng),position(rng)};
        const std::array<V3,3> local={
           V3{-1.3,-1,1.002},V3{1.4,-1,1.002},
           V3{0,1.5,1.002}};
        Body b;
        b.surface_faces={{0,1,2}};
        for(const auto& p:local){
            const V3 x=center+rot(p);
            const V3 v=rot({choose(rng)*15,choose(rng)*15,
                            -speed(rng)-1});
            b.nodes.push_back({x,x,v,mass(rng),0.});
        }
        RigidSphere sphere{center,1.,0.,0.};
        WholeFaceContactSettings cfg;
        cfg.impulse_iterations=1;
        cfg.activation_width_m=.006;
        cfg.clearance_skin_m=.0005;
        cfg.restitution=run%50==0?1:restitution(rng);
        cfg.friction=run%50==0?0:friction(rng);
        const auto closest=closest_triangle_point(
          center,b.nodes[0].x,b.nodes[1].x,b.nodes[2].x);
        const auto independent=independent_triangle_distance(
          center,b.nodes[0].x,b.nodes[1].x,b.nodes[2].x);
        max_distance_oracle_disagreement=std::max(
          max_distance_oracle_disagreement,
          std::abs(closest.distance-independent.distance));
        must(std::abs(closest.distance-independent.distance)<1e-9,
             "independent Voronoi oracle finds other contact location");
        const V3 n=(closest.nearest-center)/closest.distance;
        V3 u{};
        double k=0;
        std::vector<V3> before;
        for(int i=0;i<3;i++){
            const auto w=closest.barycentric[i];
            const auto& v=b.nodes[i];
            u+=v.v*w;
            k+=w*w/v.mass;
            before.push_back(v.v);
        }
        const double incoming=dot(u,n);
        must(incoming<-.25,"controlled face impulse must be incoming");
        const V3 tangential=u-n*incoming;
        const double tangent_speed=norm(tangential);
        const double kin_before=kinetic(b,before);
        const V3 p_before=momentum(b,before);
        const V3 l_before=angular(b,before);
        std::vector<V3> after=before;
        std::vector<double> per_vertex(3,0.);
        WholeFaceStepMetrics metrics;
        const auto sum=apply_face_velocity_impulses(
            b,sphere,{b.nodes[0].x,b.nodes[1].x,b.nodes[2].x},
            after,per_vertex,cfg,metrics);
        must(metrics.normal_impulses==1&&metrics.total_normal_impulse_ns>0,
             "exactly one contacting face should receive a local impulse");
        must(std::abs(sum-metrics.total_normal_impulse_ns)<1e-11,
             "actual normal impulse return inconsistent");
        const double Jn=metrics.total_normal_impulse_ns;
        const double Jt=metrics.total_tangential_impulse_ns;
        const V3 J=n*Jn-(tangent_speed>1e-15?
             tangential*(Jt/tangent_speed):V3{});
        const double kafter=kinetic(b,after);
        const double deltaK=kafter-kin_before;
        const double predictedK=dot(u,J)+.5*k*dot(J,J);
        const V3 measuredP=momentum(b,after)-p_before;
        const V3 measuredL=angular(b,after)-l_before;
        const double errP=norm(measuredP-J);
        const double errL=norm(measuredL-cross(closest.nearest,J));
        const double errK=std::abs(deltaK-predictedK);
        const double maxK=1e-8*std::max({1.,kin_before,kafter,
                                           std::abs(predictedK)});
        const double maxP=1e-8*std::max(1.,norm(J));
        const double maxL=1e-8*std::max(1.,norm(cross(closest.nearest,J)));
        must(errK<=maxK,"independent contact work/kinetic identity fails");
        must(errP<=maxP,"linear contact momentum balance fails");
        must(errL<=maxL,"barycentric angular impulse balance fails");
        must(deltaK<=maxK,
             "coefficient-limited contact impulse created kinetic energy");
        must(Jt<=cfg.friction*Jn+1e-9,
             "Coulomb contact tangent cap violated");
        must(Jt<=tangent_speed/k+1e-9,
             "tangential impulse overdamped the local contact");
        double sum_weights=0,impulse_total=0;
        for(int i=0;i<3;i++){
            sum_weights+=closest.barycentric[i];
            impulse_total+=per_vertex[i];
            const V3 expect=J*(closest.barycentric[i]/b.nodes[i].mass);
            must(norm(after[i]-before[i]-expect)<=maxP,
                 "contact momentum not distributed with w_i/m_i");
        }
        must(std::abs(sum_weights-1)<1e-10 &&
             std::abs(impulse_total-Jn)<1e-8*std::max(1.,Jn),
             "barycentric source impulses do not add to net normal");
        worst_energy_residual=std::max(worst_energy_residual,errK);
        worst_linear_residual=std::max(worst_linear_residual,errP);
        worst_angular_residual=std::max(worst_angular_residual,errL);
        max_positive_delta=std::max(max_positive_delta,deltaK);
        executed++;
        if(csv.is_open())
            csv<<run<<','<<cfg.restitution<<','<<cfg.friction<<','
               <<k<<','<<incoming<<','<<tangent_speed<<','<<Jn<<','
               <<Jt<<','<<deltaK<<','<<predictedK<<','
               <<errP<<','<<errL<<','<<kin_before<<','<<kafter<<'\n';
    }
    if(argc==2){
        std::ofstream manifest(std::filesystem::path(argv[1])/
                               "impulse_manifest.json");
        manifest<<std::setprecision(17)
                <<"{\n"
                <<" \"scope\":\"local_frozen_geometry_barycentric_face_impulses_only\",\n"
                <<" \"full_FEM_mechanical_energy_conservation_claim\":false,\n"
                <<" \"sample_count\":"<<executed<<",\n"
                <<" \"max_positive_kinetic_change_j\":"<<max_positive_delta<<",\n"
                <<" \"max_energy_work_residual_j\":"<<worst_energy_residual<<",\n"
                <<" \"max_linear_momentum_residual_ns\":"<<worst_linear_residual<<",\n"
                <<" \"max_angular_momentum_residual_nsm\":"<<worst_angular_residual<<",\n"
                <<" \"max_independent_contact_distance_disagreement_m\":"
                   <<max_distance_oracle_disagreement<<"\n}\n";
    }
    std::cout<<"PASS: 1200 randomized rotated/transformed contact impulse"
                " conservation and work identities"
             <<" worst_delta_K_residual_j="<<worst_energy_residual
             <<" momentum_residual_ns="<<worst_linear_residual
             <<" angular_residual_nsm="<<worst_angular_residual
             <<" largest_measured_kinetic_increase_j="<<max_positive_delta
             <<" (local contact-only, not entire damped FEM)\n";
    return 0;
 }catch(const std::exception& e){
    std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;
 }
}
