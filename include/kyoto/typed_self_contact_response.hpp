#pragma once
// Pass 32C: actual typed self impulses. A point/segment is represented
// with repeated triangle vertices for distance geometry ONLY. The signed
// barycentric stencil is coalesced by original node ID before computing
// the effective inverse mass or writing velocities. This avoids squaring
// duplicate weights separately, and is safe even for overlapping IDs.
#include "kyoto/typed_self_contact_audit.hpp"
#include <map>
namespace kyoto::solid {
inline CoupledNormalReceipt apply_indexed_normal_impulse(
    Body& b,const TypedSelfStencil& s,const std::array<double,3>& wa,
    const std::array<double,3>& wb,double restitution){
    if(!(std::isfinite(restitution)&&restitution>=0&&restitution<=1))
        throw std::invalid_argument("invalid indexed restitution");
    std::map<std::size_t,double> coeff;
    double sa=0,sb=0;
    for(int i=0;i<3;++i){
        if(!(std::isfinite(wa[i])&&wa[i]>=0&&wa[i]<=1&&
             std::isfinite(wb[i])&&wb[i]>=0&&wb[i]<=1))
            throw std::invalid_argument("invalid indexed contact weights");
        coeff[s.left[i]]+=wa[i];coeff[s.right[i]]-=wb[i];
        sa+=wa[i];sb+=wb[i];
    }
    if(std::abs(sa-1.)>1e-12||std::abs(sb-1.)>1e-12)
        throw std::invalid_argument("unnormalized indexed contact weights");
    V3 delta{},velocity{};double k=0;
    for(const auto& [i,w]:coeff){
        const auto& n=b.nodes.at(i);
        if(!(std::isfinite(n.mass)&&n.mass>0&&finite(n.x)&&finite(n.v)))
            throw std::invalid_argument("invalid indexed contact node");
        delta+=n.x*w;velocity+=n.v*w;k+=w*w/n.mass;
    }
    const double gap=norm(delta);
    if(!(std::isfinite(k)&&k>0&&std::isfinite(gap)&&gap>1e-12))
        throw std::runtime_error("indeterminate indexed contact");
    CoupledNormalReceipt r;
    r.inverse_mass=k;r.normal=delta/gap;
    r.before_normal_velocity=dot(velocity,r.normal);
    if(!std::isfinite(r.before_normal_velocity))throw std::runtime_error("invalid relative velocity");
    r.after_normal_velocity=r.before_normal_velocity;
    if(r.before_normal_velocity>=0)return r;
    const auto u=r.before_normal_velocity;
    r.normal_impulse=-(1.+restitution)*u/k;
    r.impulse_on_a=r.normal*r.normal_impulse;
    r.predicted_kinetic_change=-(1.-restitution*restitution)*u*u/(2.*k);
    std::map<std::size_t,V3> pending;
    V3 relative{};
    for(const auto& [i,w]:coeff){
        const auto& n=b.nodes[i];const V3 next=n.v+r.impulse_on_a*(w/n.mass);
        if(!finite(next))throw std::runtime_error("indexed impulse overflow");
        pending.emplace(i,next);relative+=next*w;
    }
    r.after_normal_velocity=dot(relative,r.normal);
    if(!std::isfinite(r.normal_impulse)||!std::isfinite(r.predicted_kinetic_change)||
       !std::isfinite(r.after_normal_velocity))throw std::runtime_error("invalid indexed receipt");
    for(const auto& [i,v]:pending)b.nodes[i].v=v;
    // Gross side-weight receipts preserve the existing two-sided scalar
    // accounting even where shared nodes cancel vector impulses.
    for(int i=0;i<3;++i){
        b.nodes[s.left[i]].last_normal_impulse+=r.normal_impulse*wa[i];
        b.nodes[s.right[i]].last_normal_impulse+=r.normal_impulse*wb[i];
    }
    return r;
}
inline void apply_typed_self_impulses(Body& b,TypedSelfAuditor& auditor,
        double skin,double activation,double restitution,unsigned iterations,
        bool exhaustive,SelfContactStats& stats){
    if(!(std::isfinite(skin)&&skin>0&&std::isfinite(activation)&&activation>=skin))
        throw std::invalid_argument("invalid typed activation");
    const auto set=auditor.candidates(b,b,activation,exhaustive);
    stats.box_tests+=set.box_tests;stats.incident_exclusions+=set.incident_rejections;
    for(unsigned it=0;it<iterations;++it)for(const auto& s:set.stencils){
        std::array<double,3> wa{},wb{};
        double distance=0;
        const auto a=self_positions(b,s.left),c=self_positions(b,s.right);
        if(s.kind==kyoto::contact::features::Kind::VertexFace){
            const auto n=independent_triangle_distance(a[0],c[0],c[1],c[2]);
            wa={1,0,0};wb=n.barycentric;distance=n.distance;
        }else{
            const auto n=closest_segments(a[0],a[1],c[0],c[1]);
            wa={1.-n.s,n.s,0};wb={1.-n.t,n.t,0};
            distance=norm(n.a-n.b);
        }
        if(distance>activation)continue;
        if(!(distance>skin))throw std::runtime_error("typed self source violates clearance");
        const auto r=apply_indexed_normal_impulse(b,s,wa,wb,restitution);
        if(r.normal_impulse>0){
            ++stats.impulse_updates;stats.normal_impulse+=r.normal_impulse;
            stats.contact_work+=r.predicted_kinetic_change;
        }
    }
}
} // namespace kyoto::solid
