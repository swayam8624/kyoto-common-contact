#pragma once
// Pass 29A: two finite-mass deformable triangles, local normal impulse.
// This is a reusable contact law, NOT a coupled FEM integrator yet.
// For normalized barycentric weights a_i,b_j, relative witness velocity
// u = sum a_i*v_i - sum b_j*w_j, effective inverse mass
// k = sum a_i^2/m_i + sum b_j^2/M_j. For u.n<0, J=-(1+e)u.n/k*n.
// Equal and opposite barycentric impulses preserve total linear momentum.
// Since n is parallel to the witness separation, total angular impulse
// is zero (up to rounding). Kinetic work is -(1-e^2)*(u.n)^2/(2*k).
// Deliberately frictionless: separated friction witnesses can create a
// couple and need a separate angular-momentum model.
#include "kyoto/triangle_mesh_contact.hpp"
#include <array>
#include <cmath>
#include <stdexcept>

namespace kyoto::solid {
struct ContactTriangleState {
    Triangle3 x{};
    std::array<V3,3> v{};
    std::array<double,3> mass{};
};
struct CoupledNormalReceipt {
    double inverse_mass{},before_normal_velocity{},after_normal_velocity{};
    double normal_impulse{},predicted_kinetic_change{};
    V3 normal{},impulse_on_a{};
};
inline CoupledNormalReceipt apply_two_triangle_normal_impulse(
    ContactTriangleState& a,ContactTriangleState& b,
    const std::array<double,3>& wa,const std::array<double,3>& wb,
    double restitution){
    if(!(std::isfinite(restitution)&&restitution>=0&&restitution<=1))
        throw std::invalid_argument("invalid restitution");
    const auto check=[](const ContactTriangleState& t,const std::array<double,3>& w){
        double sum=0;
        for(int i=0;i<3;++i){
            if(!finite(t.x[i])||!finite(t.v[i])||
               !(std::isfinite(t.mass[i])&&t.mass[i]>0)||
               !(std::isfinite(w[i])&&w[i]>=0&&w[i]<=1))
                throw std::invalid_argument("invalid finite-mass contact stencil");
            sum+=w[i];
        }
        if(std::abs(sum-1.)>1e-12)throw std::invalid_argument("unnormalized contact weights");
    };
    check(a,wa);check(b,wb);
    V3 pa{},pb{},u{};
    double k=0;
    for(int i=0;i<3;++i){
        pa+=a.x[i]*wa[i];pb+=b.x[i]*wb[i];
        u+=a.v[i]*wa[i]-b.v[i]*wb[i];
        k+=wa[i]*wa[i]/a.mass[i]+wb[i]*wb[i]/b.mass[i];
    }
    const double gap=norm(pa-pb);
    if(!(std::isfinite(k)&&k>0&&std::isfinite(gap)&&gap>1e-12))
        throw std::runtime_error("indeterminate finite-mass contact normal");
    CoupledNormalReceipt r;
    r.inverse_mass=k;r.normal=(pa-pb)/gap;
    r.before_normal_velocity=dot(u,r.normal);
    if(!std::isfinite(r.before_normal_velocity))
        throw std::runtime_error("nonfinite relative velocity");
    r.after_normal_velocity=r.before_normal_velocity;
    if(r.before_normal_velocity>=0)return r;
    const double un=r.before_normal_velocity;
    r.normal_impulse=-(1.+restitution)*un/k;
    r.impulse_on_a=r.normal*r.normal_impulse;
    r.predicted_kinetic_change=-(1.-restitution*restitution)*un*un/(2.*k);
    auto va=a.v,vb=b.v;V3 relative{};
    for(int i=0;i<3;++i){
        va[i]+=r.impulse_on_a*(wa[i]/a.mass[i]);
        vb[i]+=r.impulse_on_a*(-wb[i]/b.mass[i]);
        if(!finite(va[i])||!finite(vb[i]))throw std::runtime_error("nonfinite contact response");
        relative+=va[i]*wa[i]-vb[i]*wb[i];
    }
    r.after_normal_velocity=dot(relative,r.normal);
    if(!(std::isfinite(r.normal_impulse)&&std::isfinite(r.predicted_kinetic_change)&&
         std::isfinite(r.after_normal_velocity)))
        throw std::runtime_error("nonfinite impulse receipt");
    // All-or-nothing local mutation.
    a.v=va;b.v=vb;
    return r;
}
} // namespace kyoto::solid
