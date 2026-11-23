#include "kyoto/whole_face_contact_dynamics.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

using namespace kyoto::solid;
static void must(bool yes,const std::string& why){
    if(!yes)throw std::runtime_error(why);
}
int main(){
 try {
    {
        // Analytical effective-mass/impulse oracle: three independent
        // movable points on the SAME triangle, near a sphere.
        // Distinct masses prove inverse-mass barycentric distribution,
        // rather than an ad hoc per-vertex position clamp.
        Body b;
        b.nodes={
           {{-1,-1,1.003},{-1,-1,1.003},{0,0,-1.2},2,0},
           {{ 1,-1,1.003},{ 1,-1,1.003},{0,0,-1.2},4,0},
           {{ 0, 1,1.003},{ 0, 1,1.003},{0,0,-1.2},8,0}};
        b.surface_faces={{0,1,2}};
        RigidSphere s{{0,0,0},1,0,0.3};
        WholeFaceContactSettings cfg;
        cfg.activation_width_m=.005;
        cfg.friction=0;
        cfg.restitution=0;
        cfg.impulse_iterations=1;
        auto pts=std::vector<V3>{b.nodes[0].x,b.nodes[1].x,b.nodes[2].x};
        auto vel=std::vector<V3>{b.nodes[0].v,b.nodes[1].v,b.nodes[2].v};
        std::vector<double> J(3,0);
        WholeFaceStepMetrics metrics;
        const auto witness=closest_triangle_point(
            s.center,pts[0],pts[1],pts[2]);
        const auto weights=witness.barycentric;
        double denom=0;
        for(int i=0;i<3;i++)
            denom+=weights[i]*weights[i]/b.nodes[i].mass;
        const double expected=1.2/denom;
        apply_face_velocity_impulses(b,s,pts,vel,J,cfg,metrics);
        V3 contact_v{};
        for(int i=0;i<3;i++)contact_v+=vel[i]*weights[i];
        const V3 n=(witness.nearest-s.center)/witness.distance;
        must(std::abs(dot(contact_v,n))<1e-10,
             "barycentric effective-mass impulse did not cancel closing velocity");
        must(std::abs(metrics.total_normal_impulse_ns-expected)<1e-9,
             "analytical normal impulse mismatch");
        for(int i=0;i<3;i++)
            must(std::abs(J[i]-expected*weights[i])<1e-9,
                 "barycentric impulse distribution not correct");
    }
    {
        auto body=toroidal_solid();
        const RigidSphere sphere{};
        const auto initial=body.diagnostics(-100);
        WholeFaceContactSettings cfg;
        std::size_t active_steps=0,clipped_steps=0;
        double min_gap=std::numeric_limits<double>::infinity();
        double minimum_J=std::numeric_limits<double>::infinity();
        double maxstress=0,normal_total=0;
        double min_fraction=1;
        for(int n=1;n<=900;n++){
            const auto r=step_whole_face_sphere(body,sphere,.00075,cfg);
            const auto geometry=audit_surface_sphere(body,sphere);
            const auto physical=body.diagnostics(-100);
            must(geometry.penetrating_faces==0,
                 "whole-face solver left full-triangle overlap");
            must(geometry.inside_vertex_count==0,
                 "whole-face solver left vertex penetration");
            must(r.minimum_triangle_clearance_m>=cfg.clearance_skin_m*.5,
                 "whole-face sphere skin violated");
            must(physical.min_jacobian>1e-6,
                 "whole-face scheme inverted a tetrahedral element");
            must(std::isfinite(physical.elastic_joule)&&
                 std::isfinite(physical.kinetic_joule)&&
                 std::isfinite(physical.gravity_joule),
                 "nonfinite FEM energy during whole-face contact");
            min_gap=std::min(min_gap,geometry.minimum_surface_clearance_m);
            minimum_J=std::min(minimum_J,physical.min_jacobian);
            maxstress=std::max(maxstress,physical.max_von_mises_pa);
            normal_total+=r.total_normal_impulse_ns;
            clipped_steps+=r.time_clipped;
            active_steps+=r.normal_impulses>0;
            min_fraction=std::min(min_fraction,r.accepted_global_fraction);
        }
        const auto end=body.diagnostics(-100);
        must(std::abs(end.time_seconds-.675)<1e-9,
             "integrated wall-time label must remain correct");
        must(std::abs(end.total_mass_kg-initial.total_mass_kg)<1e-8,
             "mass changed in whole-face response");
        must(end.max_displacement>.1,
             "volumetric solid did not actually move");
        must(active_steps>0 && normal_total>0,
             "whole-face solver lacks real mass-weighted contact reaction");
        must(maxstress>1 && minimum_J<.999,
             "no physically computed elastic material deformation");
        std::cout<<"WHOLE_FACE_FEM_900_PASS"
                 <<" active_contact_steps="<<active_steps
                 <<" clipped_steps="<<clipped_steps
                 <<" minimum_global_fraction="<<min_fraction
                 <<" min_full_triangle_gap_m="<<min_gap
                 <<" max_von_mises_pa="<<maxstress
                 <<" min_tet_J="<<minimum_J
                 <<" summed_normal_impulse_ns="<<normal_total
                 <<" end_COM_z="<<end.center_of_mass.z<<'\n';
    }
    std::cout<<"PASS: mass-weighted barycentric impulses + transactional "
                "900-step synced whole-face FEM with independent triangle "
                "audit at every accepted step\n";
    return 0;
 }catch(const std::exception& e){
    std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;
 }
}
