#pragma once
// Pass 23 PROTOTYPE: two physically coupled deformable 3D neo-Hookean
// tetrahedral bodies exchanging equal-and-opposite barycentric contact
// impulses on finite triangle surfaces.
//
// This extends the prior static-rigid solver, but unlike the original
// rigid patch BOTH contact bodies have finite nodal masses and BOTH
// receive the contact reaction. Every explicit candidate physical
// leaf is checked by the moving/moving continuous separation guard
// before public state can be committed. Rejected macro-steps are
// time-bisected and REINTEGRATED, never clipped in fixed-clock time.
//
// This remains an explicit LOCAL impulse FEM reference; not an
// implicit IPC optimization, certified double-precision CCD,
// converged contact manifold, self-contact solver, or paper novelty.
#include "kyoto/moving_triangle_pair_certificate.hpp"
#include "kyoto/solid_dynamics.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <functional>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

namespace kyoto::solid {

struct CoupledDeformableSettings {
    double skin_m{.0005};
    double activation_width_m{.012};
    double restitution{};
    double friction{.08};
    unsigned impulse_iterations{2};
    unsigned interval_depth{12};
    unsigned maximum_bisection_depth{15};
    double minimum_dt_s{1e-10};
    std::size_t maximum_accepted_leaves{8192};
};
inline void validate_coupled(const CoupledDeformableSettings& s) {
    if(!(std::isfinite(s.skin_m)&&s.skin_m>0 &&
         std::isfinite(s.activation_width_m)&&s.activation_width_m>=0 &&
         std::isfinite(s.restitution)&&s.restitution>=0&&
         s.restitution<=1 &&
         std::isfinite(s.friction)&&s.friction>=0 &&
         s.impulse_iterations>=1&&s.impulse_iterations<=8 &&
         s.interval_depth>=4&&s.interval_depth<=20 &&
         s.maximum_bisection_depth<=20 &&
         std::isfinite(s.minimum_dt_s)&&s.minimum_dt_s>0 &&
         s.maximum_accepted_leaves>=1))
        throw std::invalid_argument("invalid coupled deformable FEM settings");
}
struct CoupledFEMReceipt {
    bool accepted{},rejected_by_moving_ccd{},rejected_by_tet{};
    double dt_s{},min_face_gap_m{std::numeric_limits<double>::infinity()};
    double min_tet_j{std::numeric_limits<double>::infinity()};
    std::size_t active_face_pairs{},impulse_updates{},
                moving_pair_box_tests{},continuous_pair_tests{},
                continuous_interval_nodes{};
    double normal_impulse_ns{},friction_impulse_ns{};
    V3 impulse_on_a{},impulse_on_b{};
};
struct CoupledFEMAcceptedLeaf {
    Body a_before{},b_before{},a_after{},b_after{};
    CoupledFEMReceipt receipt{};
};
struct CoupledFEMAdaptiveStats {
    double start_time_s{},end_time_s{},integrated_dt_s{};
    std::size_t accepted_leaves{},rejected_ccd_trials{},
                rejected_tet_trials{},impulse_updates{},
                continuous_pair_tests{},continuous_interval_nodes{};
    double min_accepted_face_gap_m{std::numeric_limits<double>::infinity()};
    double min_accepted_tet_j{std::numeric_limits<double>::infinity()};
    V3 impulse_on_a{},impulse_on_b{};
};
inline Triangle3 moving_face(
    const std::vector<V3>& x,
    const std::array<std::size_t,3>& face){
    return {x.at(face[0]),x.at(face[1]),x.at(face[2])};
}

// Exact finite-face minimum distance (up to the original numerical
// triangle-triangle closest-point implementation). AABB distances are
// lower bounds; skipping only when lower > current best cannot miss
// a closer pair.
inline double coupled_surface_gap(
    const Body& a,const Body& b,
    const std::vector<V3>& xa,const std::vector<V3>& xb){
    if(a.surface_faces.empty()||b.surface_faces.empty())
        throw std::invalid_argument("empty coupled deformable surface");
    double closest=std::numeric_limits<double>::infinity();
    for(const auto& fa:a.surface_faces){
        const auto ta=moving_face(xa,fa);
        const auto ba=bounding(ta);
        for(const auto& fb:b.surface_faces){
            const auto tb=moving_face(xb,fb);
            const auto bb=bounding(tb);
            const double dx=std::max({ba.lo.x-bb.hi.x,
                                      bb.lo.x-ba.hi.x,0.});
            const double dy=std::max({ba.lo.y-bb.hi.y,
                                      bb.lo.y-ba.hi.y,0.});
            const double dz=std::max({ba.lo.z-bb.hi.z,
                                      bb.lo.z-ba.hi.z,0.});
            if(std::sqrt(dx*dx+dy*dy+dz*dz)>closest)
                continue;
            closest=std::min(closest,
                triangle_triangle_closest(ta,tb).distance);
            if(closest<=0)return closest;
        }
    }
    if(!std::isfinite(closest))
        throw std::runtime_error("invalid coupled triangle surface gap");
    return closest;
}

inline std::vector<V3> body_positions(const Body& body) {
    std::vector<V3> result;
    result.reserve(body.nodes.size());
    for(const auto& n:body.nodes){
        if(!finite(n.x))throw std::invalid_argument("invalid coupled FEM position");
        result.push_back(n.x);
    }
    return result;
}
inline V3 coupled_total_linear_momentum(
    const Body& a,const Body& b,
    const std::vector<V3>& va,const std::vector<V3>& vb){
    if(va.size()!=a.nodes.size()||vb.size()!=b.nodes.size())
        throw std::invalid_argument("coupled momentum source size mismatch");
    V3 momentum{};
    for(std::size_t i=0;i<va.size();++i)
        momentum+=va[i]*a.nodes[i].mass;
    for(std::size_t i=0;i<vb.size();++i)
        momentum+=vb[i]*b.nodes[i].mass;
    return momentum;
}

// Isolated symmetric contact impulse for one finite-face pair.
// Positive impulse is applied to A and its exact opposite to B.
// The inverse effective mass is sum(wA^2/mA)+sum(wB^2/mB), so contact
// response cannot silently treat one deformable as infinitely heavy.
inline bool apply_coupled_face_impulse(
    const Body& a,const Body& b,
    const std::array<std::size_t,3>& face_a,
    const std::array<std::size_t,3>& face_b,
    const RigidFaceContact& nearest,
    std::vector<V3>& va,std::vector<V3>& vb,
    const CoupledDeformableSettings& settings,
    CoupledFEMReceipt& receipt){
    if(!(nearest.distance>1e-12&&std::isfinite(nearest.distance)))
        throw std::runtime_error("coupled contact normal is geometrically indeterminate");
    if(va.size()!=a.nodes.size()||vb.size()!=b.nodes.size())
        throw std::invalid_argument("coupled impulse velocity size mismatch");
    const V3 normal=(nearest.on_deformable-nearest.on_rigid)/nearest.distance;
    double inverse_effective_mass=0.;
    V3 relative{};
    for(int k=0;k<3;++k){
        const auto ia=face_a[k],ib=face_b[k];
        const auto ma=a.nodes.at(ia).mass,mb=b.nodes.at(ib).mass;
        if(!(ma>0&&mb>0&&std::isfinite(ma)&&std::isfinite(mb)))
            throw std::runtime_error("invalid physical FEM contact nodal mass");
        const auto wa=nearest.deformable_weights[k];
        const auto wb=nearest.rigid_weights[k];
        if(!(std::isfinite(wa)&&std::isfinite(wb)&&wa>=-1e-10&&wb>=-1e-10))
            throw std::runtime_error("invalid contact barycentric weight");
        inverse_effective_mass+=wa*wa/ma+wb*wb/mb;
        relative+=va.at(ia)*wa-vb.at(ib)*wb;
    }
    if(!(inverse_effective_mass>0&&std::isfinite(inverse_effective_mass)))
        throw std::runtime_error("invalid coupled inverse effective contact mass");
    const double inward=dot(relative,normal);
    if(inward>=-1e-12)return false;
    const double normal_j=-(1.+settings.restitution)*inward/
                           inverse_effective_mass;
    const V3 tangent=relative-normal*inward;
    const double tangential_speed=norm(tangent);
    const double friction_j=tangential_speed>1e-15?
        std::min(settings.friction*normal_j,
                 tangential_speed/inverse_effective_mass):0.;
    const V3 impulse=normal*normal_j-
         (tangential_speed>1e-15?
          tangent*(friction_j/tangential_speed):V3{});
    for(int k=0;k<3;++k){
        const auto ia=face_a[k],ib=face_b[k];
        va[ia]+=impulse*(nearest.deformable_weights[k]/a.nodes[ia].mass);
        vb[ib]=vb[ib]-impulse*(nearest.rigid_weights[k]/b.nodes[ib].mass);
    }
    ++receipt.impulse_updates;
    receipt.normal_impulse_ns+=normal_j;
    receipt.friction_impulse_ns+=friction_j;
    receipt.impulse_on_a+=impulse;
    receipt.impulse_on_b+=-impulse;
    return true;
}

inline void apply_coupled_surface_impulses(
    const Body& a,const Body& b,
    const std::vector<V3>& xa,const std::vector<V3>& xb,
    std::vector<V3>& va,std::vector<V3>& vb,
    const CoupledDeformableSettings& cfg,
    CoupledFEMReceipt& receipt){
    const double activation=cfg.skin_m+cfg.activation_width_m;
    for(unsigned iteration=0;iteration<cfg.impulse_iterations;++iteration){
        for(const auto& fa:a.surface_faces){
            const auto ta=moving_face(xa,fa);
            const auto ab=bounding(ta);
            for(const auto& fb:b.surface_faces){
                const auto tb=moving_face(xb,fb);
                if(!maybe_near(ab,bounding(tb),activation))continue;
                const auto nearest=triangle_triangle_closest(ta,tb);
                if(nearest.distance>activation)continue;
                if(iteration==0)++receipt.active_face_pairs;
                (void)apply_coupled_face_impulse(
                    a,b,fa,fb,nearest,va,vb,cfg,receipt);
            }
        }
    }
}
inline bool coupled_predicted_tets_valid(
    const Body& body,const std::vector<V3>& predicted,double& min_j){
    min_j=std::numeric_limits<double>::infinity();
    for(const auto& tet:body.tets){
        const auto origin=predicted.at(tet.n[0]);
        const auto ds=from_cols(
            predicted.at(tet.n[1])-origin,
            predicted.at(tet.n[2])-origin,
            predicted.at(tet.n[3])-origin);
        const auto j=determinant(ds*tet.inverse_rest);
        if(!(std::isfinite(j)&&j>1e-6))return false;
        min_j=std::min(min_j,j);
    }
    return std::isfinite(min_j);
}
inline bool certify_coupled_pair_motion(
    const Body& a,const Body& b,
    const std::vector<V3>& old_a,const std::vector<V3>& new_a,
    const std::vector<V3>& old_b,const std::vector<V3>& new_b,
    const CoupledDeformableSettings& cfg,
    CoupledFEMReceipt& receipt){
    for(const auto& fa:a.surface_faces){
        const LinearTriangleMotion ma{
            moving_face(old_a,fa),moving_face(new_a,fa)};
        const auto ba=swept_bounding(ma);
        for(const auto& fb:b.surface_faces){
            ++receipt.moving_pair_box_tests;
            const LinearTriangleMotion mb{
                moving_face(old_b,fb),moving_face(new_b,fb)};
            if(!maybe_near(ba,swept_bounding(mb),cfg.skin_m))
                continue;
            ++receipt.continuous_pair_tests;
            const auto certificate=certify_two_moving_triangles(
                ma,mb,cfg.skin_m,cfg.interval_depth);
            receipt.continuous_interval_nodes+=certificate.nodes;
            if(!certificate.entirely_separated)return false;
        }
    }
    return true;
}

inline CoupledFEMReceipt try_coupled_deformable_fem_step(
    Body& a,Body& b,double dt,
    const CoupledDeformableSettings& cfg={}){
    validate_coupled(cfg);
    if(&a==&b)throw std::invalid_argument(
        "source must contain two distinct deformable bodies");
    if(!(std::isfinite(dt)&&dt>0))
        throw std::invalid_argument("invalid coupled physical dt");
    if(std::abs(a.time_seconds-b.time_seconds)>1e-10)
        throw std::invalid_argument("desynchronized coupled body clocks");

    CoupledFEMReceipt result;
    result.dt_s=dt;
    const auto xa=body_positions(a),xb=body_positions(b);
    if(!(coupled_surface_gap(a,b,xa,xb)>cfg.skin_m*.5))
        throw std::runtime_error("coupled bodies start in contact overlap");

    const auto fa=a.evaluate(),fb=b.evaluate();
    std::vector<V3> va,vb;
    va.reserve(a.nodes.size());vb.reserve(b.nodes.size());
    const double da=std::exp(-a.material.damping*dt);
    const double db=std::exp(-b.material.damping*dt);
    for(std::size_t i=0;i<a.nodes.size();++i){
        const auto& node=a.nodes[i];
        const auto acceleration=fa.elastic_force[i]/node.mass+
                                 V3{0,0,-a.material.gravity};
        va.push_back((node.v+acceleration*dt)*da);
    }
    for(std::size_t i=0;i<b.nodes.size();++i){
        const auto& node=b.nodes[i];
        const auto acceleration=fb.elastic_force[i]/node.mass+
                                 V3{0,0,-b.material.gravity};
        vb.push_back((node.v+acceleration*dt)*db);
    }
    // Apply first contact velocity response at source positions.
    apply_coupled_surface_impulses(a,b,xa,xb,va,vb,cfg,result);
    std::vector<V3> predicted_a,predicted_b;
    predicted_a.reserve(xa.size());predicted_b.reserve(xb.size());
    for(std::size_t i=0;i<xa.size();++i)
        predicted_a.push_back(xa[i]+va[i]*dt);
    for(std::size_t i=0;i<xb.size();++i)
        predicted_b.push_back(xb[i]+vb[i]*dt);

    // Entire source physical leaf must remain outside finite triangle
    // skin for both MOVING 3D bodies. Reject and retry at smaller real dt.
    if(!certify_coupled_pair_motion(a,b,xa,predicted_a,xb,predicted_b,
                                   cfg,result)){
        result.rejected_by_moving_ccd=true;
        return result;
    }
    double ja=0.,jb=0.;
    if(!coupled_predicted_tets_valid(a,predicted_a,ja)||
       !coupled_predicted_tets_valid(b,predicted_b,jb)){
        result.rejected_by_tet=true;
        return result;
    }
    result.min_tet_j=std::min(ja,jb);
    const auto proposed_gap=coupled_surface_gap(
        a,b,predicted_a,predicted_b);
    if(!(proposed_gap>cfg.skin_m*.5)){
        result.rejected_by_moving_ccd=true;
        return result;
    }
    // As in the original finite-rigid solver, the post-position
    // reaction changes NEXT-STEP velocities, not accepted positions.
    apply_coupled_surface_impulses(
        a,b,predicted_a,predicted_b,va,vb,cfg,result);
    Body candidate_a=a,candidate_b=b;
    for(std::size_t i=0;i<xa.size();++i){
        candidate_a.nodes[i].x=predicted_a[i];
        candidate_a.nodes[i].v=va[i];
    }
    for(std::size_t i=0;i<xb.size();++i){
        candidate_b.nodes[i].x=predicted_b[i];
        candidate_b.nodes[i].v=vb[i];
    }
    candidate_a.time_seconds+=dt;
    candidate_b.time_seconds+=dt;
    candidate_a.contact_count_step=result.impulse_updates;
    candidate_b.contact_count_step=result.impulse_updates;
    candidate_a.normal_impulse_step=result.normal_impulse_ns;
    candidate_b.normal_impulse_step=result.normal_impulse_ns;
    // Verify both actual volumetric source states before publication.
    const auto accepted_a=candidate_a.evaluate();
    const auto accepted_b=candidate_b.evaluate();
    result.min_tet_j=std::min(accepted_a.min_jacobian,
                              accepted_b.min_jacobian);
    result.min_face_gap_m=proposed_gap;
    if(!(std::isfinite(result.min_tet_j)&&result.min_tet_j>1e-6))
        throw std::runtime_error("invalid accepted coupled FEM Jacobian");
    result.accepted=true;
    a=std::move(candidate_a);
    b=std::move(candidate_b);
    return result;
}

// Time-consistent transaction: both deformables advance or NEITHER does.
// Every rejected proposal halves REAL physical time and re-evaluates
// both elastic force fields; rejected trial velocities are not reused.
inline CoupledFEMAdaptiveStats step_time_consistent_coupled_deformables(
    Body& a,Body& b,double dt,
    const CoupledDeformableSettings& cfg={},
    std::vector<CoupledFEMAcceptedLeaf>* accepted_leaves=nullptr){
    validate_coupled(cfg);
    if(!(std::isfinite(dt)&&dt>0))
        throw std::invalid_argument("invalid coupled macro physical dt");
    if(std::abs(a.time_seconds-b.time_seconds)>1e-10)
        throw std::invalid_argument("coupled macro clocks diverge");
    CoupledFEMAdaptiveStats stats;
    stats.start_time_s=a.time_seconds;
    Body staged_a=a,staged_b=b;
    std::vector<CoupledFEMAcceptedLeaf> staging;
    const auto recurse=[&](auto&& self,double leaf_dt,unsigned depth)->void {
        Body before_a,before_b;
        if(accepted_leaves){before_a=staged_a;before_b=staged_b;}
        const auto trial=try_coupled_deformable_fem_step(
            staged_a,staged_b,leaf_dt,cfg);
        stats.continuous_pair_tests+=trial.continuous_pair_tests;
        stats.continuous_interval_nodes+=trial.continuous_interval_nodes;
        if(trial.accepted){
            ++stats.accepted_leaves;
            if(stats.accepted_leaves>cfg.maximum_accepted_leaves)
                throw std::runtime_error("coupled accepted leaf budget exceeded");
            stats.integrated_dt_s+=leaf_dt;
            stats.impulse_updates+=trial.impulse_updates;
            stats.min_accepted_face_gap_m=std::min(
                stats.min_accepted_face_gap_m,trial.min_face_gap_m);
            stats.min_accepted_tet_j=std::min(
                stats.min_accepted_tet_j,trial.min_tet_j);
            stats.impulse_on_a+=trial.impulse_on_a;
            stats.impulse_on_b+=trial.impulse_on_b;
            if(accepted_leaves)staging.push_back({
                std::move(before_a),std::move(before_b),
                staged_a,staged_b,trial});
            return;
        }
        stats.rejected_ccd_trials+=trial.rejected_by_moving_ccd;
        stats.rejected_tet_trials+=trial.rejected_by_tet;
        if(depth>=cfg.maximum_bisection_depth ||
           leaf_dt*.5<cfg.minimum_dt_s)
            throw std::runtime_error(
                "coupled contact cannot be safely integrated under dt budget");
        self(self,leaf_dt*.5,depth+1);
        self(self,leaf_dt*.5,depth+1);
    };
    recurse(recurse,dt,0);
    const double tolerance=std::max(1e-12,dt*1e-9);
    if(!(std::abs(stats.integrated_dt_s-dt)<=tolerance &&
         std::abs(staged_a.time_seconds-stats.start_time_s-dt)<=tolerance &&
         std::abs(staged_b.time_seconds-stats.start_time_s-dt)<=tolerance))
        throw std::runtime_error("accepted coupled physical time did not sum to dt");
    stats.end_time_s=staged_a.time_seconds;
    // Commit caller receipts before physical states. A failed
    // allocation cannot advance only one user-visible body.
    if(accepted_leaves)accepted_leaves->insert(
        accepted_leaves->end(),
        std::make_move_iterator(staging.begin()),
        std::make_move_iterator(staging.end()));
    a=std::move(staged_a);
    b=std::move(staged_b);
    return stats;
}
} // namespace kyoto::solid
