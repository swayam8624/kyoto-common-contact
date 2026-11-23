#pragma once
// Kyoto Pass 7A: physically integrated 3D tetrahedral FEM with a new
// conservative GLOBAL whole-triangle/sphere motion limiter and
// mass-weighted barycentric contact impulses.
//
// This is an intentionally cautious engineering reference, NOT an
// IPC-style implicit constrained elastodynamic solver. Every mesh vertex
// shares a single accepted fraction of its integration trial; a failing
// triangle cannot be independently frozen while the rest tunnels past.
// The global fraction is obtained from the Pass-6B Lipschitz certificate.
// A contact "skin" > 0 provides numerical clearance; the precise
// guarantee is exact arithmetic, not universally machine-certified FP.
//
// The contact normal impulse uses effective inverse contact mass:
//   k_eff = sum_i barycentric_i^2 / mass_i,
//   J_n = -(1+e)*v_relative_normal / k_eff.
// Each participating vertex receives delta_v_i = (w_i/m_i)*J_n*n.
// A static rigid obstacle receives the opposite impulse implicitly.
// Tangential friction is limited by mu*J_n. The local sequential updates
// are NOT a globally coupled KKT solve or a proven stable friction law.
//
// Limitations: contact impulses occur inside a finite activation shell;
// global temporal clipping can alter physical momentum/energy; self-contact,
// triangle/triangle rigid mesh contact and complete numerical directed
// rounding remain open. ALL consequences are measurable in output.

#include "kyoto/triangle_sphere_sweep.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <vector>

namespace kyoto::solid {

struct WholeFaceContactSettings {
    double clearance_skin_m{0.0005};        // kept outside true rigid sphere
    double activation_width_m{0.0020};      // inelastic contact shell width
    double restitution{0.0};               // stable inelastic reference
    double friction{0.2};
    unsigned temporal_depth{13};
    unsigned impulse_iterations{3};
};

struct WholeFaceStepMetrics {
    double accepted_global_fraction{1.0};
    double minimum_vertex_clearance_m{};
    double minimum_triangle_clearance_m{};
    double total_normal_impulse_ns{};
    double total_tangential_impulse_ns{};
    std::size_t active_face_constraints{};
    std::size_t normal_impulses{};
    std::size_t face_interval_tests{};
    std::size_t whole_face_unresolved{};
    bool time_clipped{};
};

inline void validate(const WholeFaceContactSettings& cfg) {
    if(!(std::isfinite(cfg.clearance_skin_m)&&cfg.clearance_skin_m>0&&
         std::isfinite(cfg.activation_width_m)&&cfg.activation_width_m>=0&&
         std::isfinite(cfg.restitution)&&cfg.restitution>=0&&
         cfg.restitution<=1&&std::isfinite(cfg.friction)&&cfg.friction>=0&&
         cfg.temporal_depth>=4&&cfg.temporal_depth<=22&&
         cfg.impulse_iterations>=1&&cfg.impulse_iterations<=12))
        throw std::invalid_argument("invalid whole-face contact settings");
}

inline double apply_face_velocity_impulses(
    const Body& body,const RigidSphere& sphere,
    const std::vector<V3>& positions,std::vector<V3>& velocities,
    std::vector<double>& vertex_impulses,
    const WholeFaceContactSettings& cfg,
    WholeFaceStepMetrics& metrics) {
    const double activation=sphere.radius+cfg.clearance_skin_m+
                            cfg.activation_width_m;
    double normal_sum=0;
    for(unsigned pass=0;pass<cfg.impulse_iterations;pass++) {
        // Deterministic Gauss-Seidel over original stable face order.
        for(const auto& face:body.surface_faces) {
            const auto close=closest_triangle_point(sphere.center,
                positions[face[0]],positions[face[1]],positions[face[2]]);
            if(close.distance>activation)continue;
            if(!(close.distance>1e-12))throw std::runtime_error(
                "invalid contact normal: sphere center on triangle");
            if(pass==0)metrics.active_face_constraints++;
            const V3 n=(close.nearest-sphere.center)/close.distance;
            V3 vcontact{};
            double invmass=0;
            for(int k=0;k<3;k++){
                const auto id=face[k];
                const double w=close.barycentric[k];
                vcontact+=velocities[id]*w;
                invmass+=w*w/body.nodes[id].mass;
            }
            if(!(std::isfinite(invmass)&&invmass>0))
                throw std::runtime_error("invalid contact effective mass");
            const double vn=dot(vcontact,n);
            if(vn>=-1e-12)continue;
            const double Jn=-(1+cfg.restitution)*vn/invmass;
            const V3 tangent=vcontact-n*vn;
            const double speed=norm(tangent);
            const double Jt=speed>1e-15?
                std::min(cfg.friction*Jn,speed/invmass):0.0;
            const V3 full_impulse=n*Jn-
                (speed>1e-15?tangent*(Jt/speed):V3{});
            for(int k=0;k<3;k++){
                const auto id=face[k];
                const double w=close.barycentric[k];
                velocities[id]+=full_impulse*(w/body.nodes[id].mass);
                vertex_impulses[id]+=Jn*w;
            }
            normal_sum+=Jn;
            metrics.total_normal_impulse_ns+=Jn;
            metrics.total_tangential_impulse_ns+=Jt;
            metrics.normal_impulses++;
        }
    }
    return normal_sum;
}

inline WholeFaceStepMetrics step_whole_face_sphere(
    Body& body,const RigidSphere& sphere,double dt,
    const WholeFaceContactSettings& cfg={}) {
    validate(sphere);validate(cfg);
    if(!(std::isfinite(dt)&&dt>0))
        throw std::invalid_argument("nonfinite/invalid timestep");
    if(body.nodes.empty()||body.surface_faces.empty()||body.tets.empty())
        throw std::invalid_argument("whole-face solver requires volumetric mesh");
    const auto initial=audit_surface_sphere(body,sphere);
    if(initial.minimum_surface_clearance_m<=cfg.clearance_skin_m*0.5)
        throw std::runtime_error("initial whole-face clearance violates required skin");
    const auto field=body.evaluate();
    const double damping=std::exp(-body.material.damping*dt);
    std::vector<V3> old,velocity,trial,accepted;
    old.reserve(body.nodes.size());
    velocity.reserve(body.nodes.size());
    trial.reserve(body.nodes.size());
    accepted.reserve(body.nodes.size());
    for(std::size_t i=0;i<body.nodes.size();i++){
        const auto& node=body.nodes[i];
        old.push_back(node.x);
        const V3 f=field.elastic_force[i]/node.mass+
                   V3{0,0,-body.material.gravity};
        velocity.push_back((node.v+f*dt)*damping);
    }
    WholeFaceStepMetrics metrics;
    std::vector<double> impulse_by_vertex(body.nodes.size(),0.);
    // Explicitly react to near-contact closing faces *before* the
    // first displacement trial. This is a mass/barycentrically coupled
    // normal impulse; not merely a post-hoc coordinate projection.
    apply_face_velocity_impulses(body,sphere,old,velocity,
        impulse_by_vertex,cfg,metrics);

    for(std::size_t i=0;i<old.size();i++)
        trial.push_back(old[i]+velocity[i]*dt);

    RigidSphere effective=sphere;
    effective.radius+=cfg.clearance_skin_m;
    double alpha=1.0;
    for(const auto& f:body.surface_faces){
        LinearTriangleMotion motion;
        for(int k=0;k<3;k++){
            motion.start[k]=old[f[k]];
            motion.end[k]=trial[f[k]];
        }
        const auto certificate=certify_triangle_sphere_prefix(
            motion,effective,cfg.temporal_depth);
        metrics.face_interval_tests+=certificate.interval_tests;
        if(!certificate.entirely_separated) {
            metrics.whole_face_unresolved++;
            alpha=std::min(alpha,certificate.certified_safe_prefix);
        }
    }
    if(!(std::isfinite(alpha)&&alpha>=0&&alpha<=1))
        throw std::runtime_error("invalid whole-face global fraction");
    metrics.accepted_global_fraction=alpha;
    metrics.time_clipped=alpha<1;

    for(std::size_t i=0;i<old.size();i++)
        accepted.push_back(old[i]+(trial[i]-old[i])*alpha);

    // After a synchronized, certified-safe displacement, apply an
    // additional mass-weighted contact response at the accepted face
    // positions. This arrests closing velocities at new face contacts.
    apply_face_velocity_impulses(body,sphere,accepted,velocity,
        impulse_by_vertex,cfg,metrics);

    // This contact integrator runs fixed-size physics steps but may
    // truncate the accepted position displacement. Accordingly its
    // contact constraint does work and is NOT an energy-conserving
    // full remaining-time integration. This discrepancy is explicitly
    // recorded by time_clipped and accepted_global_fraction.
    //
    // Never quietly accept inverted tetrahedra or full-face overlaps.
    for(const auto& t:body.tets){
        const V3 a=accepted[t.n[0]];
        const auto ds=from_cols(accepted[t.n[1]]-a,
                   accepted[t.n[2]]-a,accepted[t.n[3]]-a);
        const double jacobian=determinant(ds*t.inverse_rest);
        if(!(std::isfinite(jacobian)&&jacobian>1e-6))
            throw std::runtime_error("whole-face trial inverts a tetrahedron");
    }
    // We must audit BEFORE commit; use a copy to keep the original
    // physics state transactional if something goes wrong.
    Body proposal=body;
    for(std::size_t i=0;i<proposal.nodes.size();i++)
        proposal.nodes[i].x=accepted[i];
    const auto after=audit_surface_sphere(proposal,sphere);
    metrics.minimum_triangle_clearance_m=
        after.minimum_surface_clearance_m;
    metrics.minimum_vertex_clearance_m=
        after.minimum_vertex_clearance_m;
    if(after.penetrating_faces>0||
       after.minimum_surface_clearance_m<cfg.clearance_skin_m*0.5)
        throw std::runtime_error(
            "synchronized whole-face accepted step violates sphere clearance");
    for(std::size_t i=0;i<body.nodes.size();i++){
        body.nodes[i].x=accepted[i];
        body.nodes[i].v=velocity[i];
        body.nodes[i].last_normal_impulse=impulse_by_vertex[i];
    }
    body.contact_count_step=metrics.normal_impulses;
    body.normal_impulse_step=metrics.total_normal_impulse_ns;
    body.time_seconds+=dt;
    return metrics;
}
} // namespace kyoto::solid
