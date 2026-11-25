#pragma once
// Pass 29B: actual synchronized two-body neo-Hookean FEM integration.
// Separate volumes, initially disjoint, fixed topology, frictionless.
// Pass 31A defaults to the outward-enclosed relative-hull separation
// certificate; the guarded Pass 22A remains an explicit comparison arm.
// Unknown -> retry smaller dt. Self contact, containment, exact event CCD and general
// contact manifolds remain outside this explicit reference solver.
#include "kyoto/coupled_triangle_normal_impulse.hpp"
#include "kyoto/moving_triangle_pair_certificate.hpp"
#include "kyoto/static_face_hierarchy.hpp"
#include "kyoto/tet_orientation_certificate.hpp"
#include "kyoto/rounded_relative_triangle_certificate.hpp"
#include "kyoto/nonincident_self_contact.hpp"
#include "kyoto/typed_self_contact_response.hpp"
#include <optional>
#include <vector>

namespace kyoto::solid {
struct CoupledFEMSettings {
    double skin{.0005},activation_width{.003},restitution{};
    unsigned impulse_iterations{3},interval_depth{12},max_bisection{16};
    unsigned orientation_depth{12};
    std::size_t max_leaves{10000};
    double min_dt{1e-10};
    bool hierarchy{false};
    kyoto::contact::atlas::ExternalCandidateProducer external_self_candidates{}; // audited only
    bool typed_self_contact{false}; // opt-in VF/EE response and safety
    bool nonincident_self_contact{false}; // restricted opt-in; no adjacent-feature guarantee
    bool rounded_surface{true}; // conditional IEEE safety gate; false is legacy control
};
struct CoupledFEMStats {
    double integrated_dt{},normal_impulse{},contact_kinetic_change{};
    std::size_t accepted_leaves{},rejected_geometry{},rejected_tets{},
        impulse_updates{},box_tests{},certificate_tests{},interval_nodes{},orientation_nodes{},
        rounded_directions{},rounded_projections{};
    SelfContactStats self_a{},self_b{}; // separate internal impulses, not cross-body transfer
    V3 impulse_on_a{};
    double min_j{std::numeric_limits<double>::infinity()};
    double certified_min_j{std::numeric_limits<double>::infinity()};
};
// Optional evidence records contain actual accepted leaf endpoints for
// BOTH bodies. Rejected trials never appear in the public trace.
struct CoupledFEMLeaf {
    Body before_a,before_b,after_a,after_b;
    double dt{};
    unsigned depth{};
    CoupledFEMStats receipt;
};
// Caller-owned optional warm state. The context caches only candidate
// topology and hierarchy data; it never owns committed physical bodies.
// A failed macrostep clears both auditors, so no rejected geometry can
// be mistaken for a previously accepted source. Not thread-safe.
class CoupledFEMContext {
    std::optional<TypedSelfAuditor> a_,b_;
    kyoto::contact::atlas::ExternalCandidateProducer external_{};
public:
    void reset() noexcept {a_.reset();b_.reset();external_=nullptr;}
    void bind(kyoto::contact::atlas::ExternalCandidateProducer p){
        if(!a_||!b_||p!=external_){reset();a_.emplace(p);b_.emplace(p);external_=p;}
    }
    TypedSelfAuditor& first(){return a_.value();}
    TypedSelfAuditor& second(){return b_.value();}
    std::size_t mapping_rebuilds()const{
        return (a_?a_->mapping_rebuilds():0)+(b_?b_->mapping_rebuilds():0);
    }
    std::size_t hierarchy_rebuilds()const{
        return (a_?a_->hierarchy_rebuilds():0)+(b_?b_->hierarchy_rebuilds():0);
    }
    std::size_t hierarchy_refits()const{
        return (a_?a_->hierarchy_refits():0)+(b_?b_->hierarchy_refits():0);
    }
};
namespace coupled_detail {
using Face = std::array<std::size_t,3>;
inline void validate_body(const Body& b){
    validate(b.material);
    if(b.nodes.empty()||b.tets.empty()||b.surface_faces.empty()||
       !std::isfinite(b.time_seconds))
        throw std::invalid_argument("invalid coupled FEM body");
    for(const auto& n:b.nodes)
        if(!finite(n.x)||!finite(n.v)||!std::isfinite(n.mass)||n.mass<=0)
            throw std::invalid_argument("invalid coupled FEM node");
    for(const auto& t:b.tets)for(auto i:t.n)
        if(i>=b.nodes.size())throw std::invalid_argument("invalid coupled FEM tet");
    for(const auto& f:b.surface_faces){
        if(f[0]==f[1]||f[1]==f[2]||f[0]==f[2])
            throw std::invalid_argument("degenerate coupled topology");
        for(auto i:f)if(i>=b.nodes.size())
            throw std::invalid_argument("invalid coupled FEM face");
    }
    (void)b.evaluate();
}
inline Triangle3 positions(const Body& b,const Face& f){
    return {b.nodes[f[0]].x,b.nodes[f[1]].x,b.nodes[f[2]].x};
}
inline ContactTriangleState state(const Body& b,const Face& f){
    ContactTriangleState t;
    t.x=positions(b,f);
    for(int i=0;i<3;++i){t.v[i]=b.nodes[f[i]].v;t.mass[i]=b.nodes[f[i]].mass;}
    return t;
}
inline void kick(Body& b,double dt){
    const auto field=b.evaluate();
    const double damp=std::exp(-b.material.damping*dt);
    for(std::size_t i=0;i<b.nodes.size();++i){
        auto& n=b.nodes[i];
        n.v=(n.v+(field.elastic_force[i]/n.mass+V3{0,0,-b.material.gravity})*dt)*damp;
        if(!finite(n.v))throw std::runtime_error("nonfinite coupled FEM kick");
        n.last_normal_impulse=0.;
    }
}
inline bool positive_tets(const Body& b){
    for(const auto& t:b.tets){
        const auto x=b.nodes[t.n[0]].x;
        const auto F=from_cols(b.nodes[t.n[1]].x-x,b.nodes[t.n[2]].x-x,
                               b.nodes[t.n[3]].x-x)*t.inverse_rest;
        const double j=determinant(F);
        if(!(std::isfinite(j)&&j>1e-6))return false;
    }
    return true;
}
// The exact real path through the *stored* binary64 inverse rest matrix
// is certified, not just the two rounded endpoint determinant samples.
inline bool continuous_tets(const Body& before,const Body& after,
                            const CoupledFEMSettings& c,CoupledFEMStats& stats){
    for(const auto& t:before.tets){
        TetPositions p,q;
        for(int k=0;k<4;++k){p[k]=before.nodes[t.n[k]].x;q[k]=after.nodes[t.n[k]].x;}
        const auto cert=certify_linear_tet_orientation(p,q,t.inverse_rest,1e-6,c.orientation_depth);
        stats.orientation_nodes+=cert.nodes;
        if(!cert.certified)return false;
        stats.certified_min_j=std::min(stats.certified_min_j,cert.lower_j);
    }
    return true;
}
inline void impulses(Body& a,Body& b,const CoupledFEMSettings& c,
                     CoupledFEMStats& stats){
    const double activation=c.skin+c.activation_width;
    std::vector<TriangleBox> boxes;
    for(const auto& f:b.surface_faces)boxes.push_back(bounding(positions(b,f)));
    std::optional<StaticFaceHierarchy> tree;
    if(c.hierarchy)tree.emplace(boxes);
    std::vector<std::size_t> rows;
    for(unsigned it=0;it<c.impulse_iterations;++it)
        for(const auto& f:a.surface_faces){
            const auto ta=positions(a,f);
            collect_static_face_rows(bounding(ta),activation,boxes,
                tree?&*tree:nullptr,rows,stats.box_tests);
            for(auto j:rows){
                const auto& g=b.surface_faces[j];
                const auto near=triangle_triangle_closest(ta,positions(b,g));
                if(near.distance>activation)continue;
                if(!(near.distance>c.skin))
                    throw std::runtime_error("coupled FEM source violates skin");
                auto sa=state(a,f),sb=state(b,g);
                const auto r=apply_two_triangle_normal_impulse(sa,sb,
                    near.deformable_weights,near.rigid_weights,c.restitution);
                if(r.normal_impulse==0.)continue;
                for(int k=0;k<3;++k){
                    a.nodes[f[k]].v=sa.v[k];b.nodes[g[k]].v=sb.v[k];
                    a.nodes[f[k]].last_normal_impulse+=r.normal_impulse*near.deformable_weights[k];
                    b.nodes[g[k]].last_normal_impulse+=r.normal_impulse*near.rigid_weights[k];
                }
                ++stats.impulse_updates;
                stats.normal_impulse+=r.normal_impulse;
                stats.contact_kinetic_change+=r.predicted_kinetic_change;
                stats.impulse_on_a+=r.impulse_on_a;
            }
        }
}
inline bool swept_safe(const Body& a0,const Body& a1,const Body& b0,const Body& b1,
                      const CoupledFEMSettings& c,CoupledFEMStats& stats){
    std::vector<LinearTriangleMotion> paths;
    std::vector<TriangleBox> boxes;
    for(const auto& g:b0.surface_faces){
        paths.push_back({positions(b0,g),positions(b1,g)});
        boxes.push_back(swept_bounding(paths.back()));
    }
    std::optional<StaticFaceHierarchy> tree;
    if(c.hierarchy)tree.emplace(boxes);
    std::vector<std::size_t> rows;
    for(const auto& f:a0.surface_faces){
        const LinearTriangleMotion path{positions(a0,f),positions(a1,f)};
        // The full endpoint AABB bounds the linear path. With finite
        // binary64 endpoints, a rounded-to-nearest upper sum cannot
        // fall below a representable coordinate that is <= the exact sum.
        // The upward-expanded skin additionally preserves the legacy guard.
        const double broad_gap=rounded::up(c.skin+1e-12);
        collect_static_face_rows(swept_bounding(path),broad_gap,boxes,
            tree?&*tree:nullptr,rows,stats.box_tests);
        for(auto j:rows){
            ++stats.certificate_tests;
            if(c.rounded_surface){
                const auto cert=certify_rounded_relative_triangle_pair(
                    path,paths[j],c.skin,c.interval_depth);
                stats.interval_nodes+=cert.nodes;
                stats.rounded_directions+=cert.directions;
                stats.rounded_projections+=cert.interval_projections;
                if(!cert.entirely_separated)return false;
            }else{
                const auto cert=certify_two_moving_triangles(
                    path,paths[j],c.skin,c.interval_depth);
                stats.interval_nodes+=cert.nodes;
                if(!cert.entirely_separated)return false;
            }
        }
    }
    return true;
}
inline void self_work(SelfContactStats& a,const SelfContactStats& b){
    a.box_tests+=b.box_tests;a.incident_exclusions+=b.incident_exclusions;
    a.certificate_tests+=b.certificate_tests;a.interval_nodes+=b.interval_nodes;
    a.projections+=b.projections;
}
inline void self_accepted(SelfContactStats& a,const SelfContactStats& b){
    a.impulse_updates+=b.impulse_updates;a.normal_impulse+=b.normal_impulse;
    a.contact_work+=b.contact_work;
}
inline void self_impulses(Body& a,Body& b,const CoupledFEMSettings& c,CoupledFEMStats& s,
                         TypedSelfAuditor& auditor_a,TypedSelfAuditor& auditor_b){
    if(c.typed_self_contact){
        apply_typed_self_impulses(a,auditor_a,c.skin,c.skin+c.activation_width,
                                  c.restitution,c.impulse_iterations,!c.hierarchy,s.self_a);
        apply_typed_self_impulses(b,auditor_b,c.skin,c.skin+c.activation_width,
                                  c.restitution,c.impulse_iterations,!c.hierarchy,s.self_b);
        return;
    }
    if(!c.nonincident_self_contact)return;
    apply_nonincident_self_impulses(a,c.skin,c.skin+c.activation_width,c.restitution,
                                   c.impulse_iterations,c.hierarchy,s.self_a);
    apply_nonincident_self_impulses(b,c.skin,c.skin+c.activation_width,c.restitution,
                                   c.impulse_iterations,c.hierarchy,s.self_b);
}
inline bool self_safety(const Body& a0,const Body& a1,const Body& b0,const Body& b1,
                        const CoupledFEMSettings& c,CoupledFEMStats& s,
                        TypedSelfAuditor& auditor_a,TypedSelfAuditor& auditor_b){
    if(c.typed_self_contact){
        auto check=[&](TypedSelfAuditor& auditor,const Body& before,const Body& after,SelfContactStats& stats){
            const auto r=auditor.audit(before,after,c.skin,c.interval_depth,!c.hierarchy);
            stats.certificate_tests+=r.certificates;stats.projections+=r.projections;
            stats.box_tests+=r.box_tests;stats.incident_exclusions+=r.incident_rejections;
            stats.interval_nodes+=r.interval_nodes;
            return r.separated;
        };
        return check(auditor_a,a0,a1,s.self_a)&&check(auditor_b,b0,b1,s.self_b);
    }
    return !c.nonincident_self_contact ||
        (certify_nonincident_self_sweep(a0,a1,c.skin,c.interval_depth,c.hierarchy,s.self_a)&&
         certify_nonincident_self_sweep(b0,b1,c.skin,c.interval_depth,c.hierarchy,s.self_b));
}
// This is the actual pre-certification physical proposal. It operates
// on private copies; never label its geometry accepted. External CCD
// audits use this SAME path for a positive rejected-trial control.
inline void propose_drift(Body& ta,Body& tb,double h,const CoupledFEMSettings& c,
                          CoupledFEMStats& local,TypedSelfAuditor& auditor_a,
                          TypedSelfAuditor& auditor_b){
    kick(ta,h);kick(tb,h);
    impulses(ta,tb,c,local);
    self_impulses(ta,tb,c,local,auditor_a,auditor_b);
    for(auto& n:ta.nodes)n.x+=n.v*h;
    for(auto& n:tb.nodes)n.x+=n.v*h;
}
inline void accumulate(CoupledFEMStats& a,const CoupledFEMStats& b){
    self_accepted(a.self_a,b.self_a);self_accepted(a.self_b,b.self_b);
    a.normal_impulse+=b.normal_impulse;
    a.contact_kinetic_change+=b.contact_kinetic_change;
    a.impulse_updates+=b.impulse_updates;
    a.impulse_on_a+=b.impulse_on_a;
    a.min_j=std::min(a.min_j,b.min_j);
    a.certified_min_j=std::min(a.certified_min_j,b.certified_min_j);
}
} // namespace coupled_detail

inline CoupledFEMStats step_coupled_fem(Body& a,Body& b,double dt,
                                      const CoupledFEMSettings& c={},
                                      std::vector<CoupledFEMLeaf>* trace=nullptr,
                                      CoupledFEMContext* context=nullptr){
    using namespace coupled_detail;
    if(&a==&b||(c.external_self_candidates&&!c.typed_self_contact)||(c.typed_self_contact&&c.nonincident_self_contact)||!(std::isfinite(dt)&&dt>0&&std::isfinite(c.skin)&&c.skin>0&&
         std::isfinite(c.activation_width)&&c.activation_width>=0&&
         std::isfinite(c.skin+c.activation_width)&&std::isfinite(c.restitution)&&
         c.restitution>=0&&c.restitution<=1&&c.impulse_iterations>=1&&
         c.impulse_iterations<=10&&c.interval_depth<=20&&c.orientation_depth<=20&&c.max_bisection<=20&&
         c.max_leaves>0&&std::isfinite(c.min_dt)&&c.min_dt>0&&dt>=c.min_dt))
        throw std::invalid_argument("invalid coupled FEM settings");
    if(!rounded::prerequisites())throw std::runtime_error("unsupported coupled FEM FP model");
    validate_body(a);validate_body(b);
    if(a.time_seconds!=b.time_seconds)
        throw std::invalid_argument("coupled bodies have different clocks");
    Body sa=a,sb=b;
    CoupledFEMStats total;
    CoupledFEMContext local_context;
    auto& warm=context?*context:local_context;
    struct ResetOnFailure {
        CoupledFEMContext& value;
        bool committed{};
        ~ResetOnFailure(){if(!committed)value.reset();}
    } guard{warm};
    warm.bind(c.external_self_candidates);
    auto& auditor_a=warm.first();auto& auditor_b=warm.second();
    // Verify initial cross-surface separation; volume disjointness and
    // self-validity are explicit caller preconditions, not inferred here.
    if(!swept_safe(sa,sa,sb,sb,c,total)||!self_safety(sa,sa,sb,sb,c,total,auditor_a,auditor_b))
        throw std::runtime_error("initial coupled surfaces violate skin");
    std::vector<double> ia(a.nodes.size()),ib(b.nodes.size());
    std::vector<CoupledFEMLeaf> staged_trace;
    const auto recurse=[&](auto&& self,double h,unsigned depth)->void{
        if(h<c.min_dt||total.accepted_leaves>=c.max_leaves)
            throw std::runtime_error("coupled FEM timestep budget exhausted");
        Body ta=sa,tb=sb;
        CoupledFEMStats local;
        propose_drift(ta,tb,h,c,local,auditor_a,auditor_b);
        const bool geo=swept_safe(sa,ta,sb,tb,c,local)&&self_safety(sa,ta,sb,tb,c,local,auditor_a,auditor_b);
        const bool tet=positive_tets(ta)&&positive_tets(tb)&&
            continuous_tets(sa,ta,c,local)&&continuous_tets(sb,tb,c,local);
        total.box_tests+=local.box_tests;
        total.certificate_tests+=local.certificate_tests;
        total.interval_nodes+=local.interval_nodes;
        total.orientation_nodes+=local.orientation_nodes;
        total.rounded_directions+=local.rounded_directions;
        total.rounded_projections+=local.rounded_projections;
        if(!geo||!tet){
            self_work(total.self_a,local.self_a);self_work(total.self_b,local.self_b);
            total.rejected_geometry+=!geo;total.rejected_tets+=!tet;
            if(depth>=c.max_bisection||h*.5<c.min_dt)
                throw std::runtime_error("coupled FEM cannot certify physical trial");
            self(self,h*.5,depth+1);self(self,h-h*.5,depth+1);return;
        }
        // Both proposed trajectories are certified before post-drift
        // contact response; no position clamping or phantom elapsed time.
        const auto old_tests=local.box_tests;
        impulses(ta,tb,c,local);self_impulses(ta,tb,c,local,auditor_a,auditor_b);
        self_work(total.self_a,local.self_a);self_work(total.self_b,local.self_b);
        total.box_tests+=local.box_tests-old_tests;
        local.min_j=std::min(ta.evaluate().min_jacobian,tb.evaluate().min_jacobian);
        for(std::size_t i=0;i<ia.size();++i)ia[i]+=ta.nodes[i].last_normal_impulse;
        for(std::size_t i=0;i<ib.size();++i)ib[i]+=tb.nodes[i].last_normal_impulse;
        ta.time_seconds+=h;tb.time_seconds+=h;
        local.integrated_dt=h;local.accepted_leaves=1;
        if(trace)staged_trace.push_back({sa,sb,ta,tb,h,depth,local});
        total.integrated_dt+=h;++total.accepted_leaves;
        accumulate(total,local);
        sa=std::move(ta);sb=std::move(tb);
    };
    recurse(recurse,dt,0);
    if(std::abs(total.integrated_dt-dt)>3e-12*std::max(1.,dt)||
       std::abs(sa.time_seconds-(a.time_seconds+dt))>3e-12*std::max(1.,dt)||
       sa.time_seconds!=sb.time_seconds)
        throw std::runtime_error("coupled physical time accounting failure");
    sa.time_seconds=sb.time_seconds=a.time_seconds+dt;
    if(!std::isfinite(sa.time_seconds))throw std::runtime_error("coupled clock overflow");
    sa.contact_count_step=total.impulse_updates+total.self_a.impulse_updates;
    sb.contact_count_step=total.impulse_updates+total.self_b.impulse_updates;
    sa.normal_impulse_step=total.normal_impulse+2.*total.self_a.normal_impulse;
    sb.normal_impulse_step=total.normal_impulse+2.*total.self_b.normal_impulse;
    for(std::size_t i=0;i<ia.size();++i)sa.nodes[i].last_normal_impulse=ia[i];
    for(std::size_t i=0;i<ib.size();++i)sb.nodes[i].last_normal_impulse=ib[i];
    // Single publication point for both bodies, after every leaf passed.
    a=std::move(sa);b=std::move(sb);
    if(trace)trace->swap(staged_trace); // replace only after successful macro-step
    guard.committed=true;
    return total;
}
} // namespace kyoto::solid
