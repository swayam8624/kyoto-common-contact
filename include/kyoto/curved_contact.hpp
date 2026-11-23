#pragma once
// Pass 6 / curved nonplanar rigid collider for the REAL tetrahedral FEM body.
// Restriction: exact analytic swept VERTEX-vs-SPHERE intersection of the
// semi-implicit Euler linear trial chord. The solver stops a vertex at its
// first contact and changes its velocity with a one-sided impulse.
// No claims of safe triangle-vs-sphere or body self-contact; a triangle can
// intersect a sphere even when all its vertices are outside. That is an
// explicit future acceptance gate, not an implicit correctness assertion.
//
// Do not mistake this for IPC mesh CCD. The sphere is smooth, analytic,
// nonplanar, and there is no prescribed vertex animation.

#include "kyoto/solid_dynamics.hpp"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <vector>

namespace kyoto::solid {
struct RigidSphere {
    V3 center{0,0,-0.45};
    double radius{0.75};
    double restitution{0.1};
    double friction{0.24};
};
inline void validate(const RigidSphere& s) {
    if(!finite(s.center)||!(std::isfinite(s.radius)&&s.radius>0)||
       !std::isfinite(s.restitution)||s.restitution<0||
       s.restitution>1||!std::isfinite(s.friction)||s.friction<0)
        throw std::invalid_argument("invalid rigid sphere");
}
struct SphereSweep {
    bool hit{};
    double time_fraction{1.0};
    V3 surface_point{},normal{};
};
inline SphereSweep swept_vertex_sphere(V3 start,V3 end,
                                      const RigidSphere& sphere) {
    validate(sphere);
    if(!finite(start)||!finite(end))
        throw std::invalid_argument("nonfinite vertex trajectory");
    const V3 from_center=start-sphere.center;
    const V3 delta=end-start;
    const double d0=dot(from_center,from_center);
    const double rr=sphere.radius*sphere.radius;
    if(d0<rr-1e-10*std::max(1.,rr))
        throw std::invalid_argument("initially penetrating sphere");
    const double a=dot(delta,delta);
    if(!(std::isfinite(a)&&std::isfinite(d0)))
        throw std::overflow_error("nonfinite sphere CCD input");
    if(a==0)return {};
    const double b=2*dot(from_center,delta),c=d0-rr;
    // Starting on the boundary and moving inward is a time-zero hit.
    // Moving tangent/outward is not an inbound crossing.
    if(c<=1e-12*std::max(1.,rr)&&b<0) {
        const V3 n=from_center/norm(from_center);
        return {true,0.,sphere.center+n*sphere.radius,n};
    }
    const double discr=b*b-4*a*c;
    if(!std::isfinite(discr))
        throw std::overflow_error("nonfinite sphere CCD discriminant");
    if(discr<0 || b>=0)return {};
    const double sqrt_disc=std::sqrt(discr);
    // Numerically stable quadratic formula for inbound root, b<0:
    // smaller root = 2c/(-b+sqrt(discr)); avoids cancellation.
    const double den=-b+sqrt_disc;
    if(!(den>0))return {};
    const double tau=2*c/den;
    if(!(tau>=0 && tau<=1))return {};
    const V3 hit=start+delta*tau;
    const V3 normal=(hit-sphere.center)/norm(hit-sphere.center);
    if(!finite(normal))throw std::runtime_error("sphere CCD normal invalid");
    return {true,tau,sphere.center+normal*sphere.radius,normal};
}

struct SphereContactMetrics {
    std::size_t swept_hits{},normal_impulses{};
    double normal_impulse_ns{};
    double minimum_clearance_m{std::numeric_limits<double>::infinity()};
    double maximum_sweep_fraction{};
};

// Semi-implicit true FEM solver + exact analytic swept vertex/chord hit.
// A vertex that hits cannot move *through* the sphere in that accepted
// global step: its position stops at the first hit plus outward skin.
// The reflected velocity is retained for the next step.
// This conservative split-at-contact is first-order; it sacrifices the
// remaining fraction of the impacted vertex's timestep. It is not an
// energy-accurate coupled contact-constrained time integrator.
inline SphereContactMetrics step_rigid_sphere(Body& body,
                  const RigidSphere& sphere,double dt) {
    validate(sphere);
    if(!(dt>0&&std::isfinite(dt)))
        throw std::invalid_argument("invalid sphere timestep");
    const auto field=body.evaluate();
    const double damp=std::exp(-body.material.damping*dt);
    const std::size_t count=body.nodes.size();
    std::vector<V3> future_x(count),future_v(count);
    std::vector<double> impulses(count,0);
    SphereContactMetrics result;
    // Save a strictly pre-impact prefix of the original linear chord.
    // Normal-offset positions are NOT guaranteed to preserve a safe chord.
    for(std::size_t i=0;i<count;i++){
        const Node& n=body.nodes[i];
        const V3 acceleration=field.elastic_force[i]/n.mass+
                               V3{0,0,-body.material.gravity};
        const V3 trial_v=(n.v+acceleration*dt)*damp;
        const V3 trial_x=n.x+trial_v*dt;
        const auto hit=swept_vertex_sphere(n.x,trial_x,sphere);
        if(hit.hit) {
            result.swept_hits++;
            result.maximum_sweep_fraction=
                std::max(result.maximum_sweep_fraction,hit.time_fraction);
            // Retain only a certified pre-impact prefix of the trial
            // chord. This also detects outside-to-outside tunnelling.
            const double safe_tau=std::max(0.0,hit.time_fraction-1e-9);
            future_x[i]=n.x+(trial_x-n.x)*safe_tau;
            const double vn=dot(trial_v,hit.normal);
            future_v[i]=trial_v;
            if(vn<0){
                const double j=-(1+sphere.restitution)*n.mass*vn;
                future_v[i]+=hit.normal*(j/n.mass);
                const V3 tangent=future_v[i]-
                                 hit.normal*dot(future_v[i],hit.normal);
                const double tang_speed=norm(tangent);
                if(tang_speed>0) {
                    const double friction_impulse=std::min(
                        n.mass*tang_speed,sphere.friction*j);
                    future_v[i]+=tangent*(-friction_impulse/
                                          (n.mass*tang_speed));
                }
                impulses[i]=j;
                result.normal_impulse_ns+=j;
                result.normal_impulses++;
            }
        } else {
            future_x[i]=trial_x;
            future_v[i]=trial_v;
        }
        const double clearance=norm(future_x[i]-sphere.center)-
                               sphere.radius;
        result.minimum_clearance_m=std::min(result.minimum_clearance_m,
                                            clearance);
        if(!(std::isfinite(clearance)&&clearance>=-2e-9&&
             finite(future_x[i])&&finite(future_v[i])))
            throw std::runtime_error("sphere constraint violated");
    }
    // Numerically validate all oriented tetrahedra BEFORE state commit.
    // This is a transactional update. No silent inversion fix.
    for(const auto& t:body.tets){
        const auto a=future_x[t.n[0]];
        const M3 ds=from_cols(future_x[t.n[1]]-a,
                             future_x[t.n[2]]-a,
                             future_x[t.n[3]]-a);
        const double j=determinant(ds*t.inverse_rest);
        if(!(std::isfinite(j)&&j>1e-6))
            throw std::runtime_error("sphere impact inverts tetra: reduce dt");
    }
    for(std::size_t i=0;i<count;i++){
        body.nodes[i].x=future_x[i];
        body.nodes[i].v=future_v[i];
        body.nodes[i].last_normal_impulse=impulses[i];
    }
    body.time_seconds+=dt;
    body.contact_count_step=result.normal_impulses;
    body.normal_impulse_step=result.normal_impulse_ns;
    return result;
}
} // namespace kyoto::solid
