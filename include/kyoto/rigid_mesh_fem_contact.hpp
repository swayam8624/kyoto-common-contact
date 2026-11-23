#pragma once
// Kyoto Pass 11B: first REAL neo-Hookean FEM / finite rigid triangle
// mesh CONTACT RESPONSE. No analytical sphere and no infinite plane.
//
// Two-sided finite static triangles form an open, nonplanar obstacle;
// all 320 moving faces of the 480-tet 180-node hyperelastic body can
// contact any of the finite obstacle faces.
//
// The solver is intentionally explicit, LOCAL, and limited:
// - semi-implicit neo-Hookean volumetric forces and gravity;
// - sequential effective-mass barycentric VF/EE/face-nearest impulses;
// - full-face conservative continuous finite triangle-triangle CCD;
// - adaptive rejection and actual reintegration of full physical time;
// - finite skin and activation shell, Coulomb-limited friction;
// - GLOBAL transactional rollback if any required substep cannot
//   be certified or would invert a tetrahedron.
//
// This is a genuinely coupled geometric CONTACT RESPONSE, but NOT an
// implicit IPC barrier/KKT solver, a closed-manifold inside/outside
// classification, general deformable self-contact, moving rigid
// obstacle response, or machine-directed-rounded predicate. Unknown
// continuous face intervals are NEVER silently accepted.

#include "kyoto/triangle_mesh_contact.hpp"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <iterator>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <vector>

namespace kyoto::solid {

struct RigidMeshContactSettings{
    double clearance_skin_m{.0005};
    double activation_width_m{.003};
    double restitution{};
    double friction{.2};
    unsigned impulse_iterations{3};
    unsigned interval_depth{12};
    unsigned maximum_bisection_depth{16};
    std::size_t maximum_accepted_leaves{10000};
    double minimum_dt_s{1e-10};
};
inline void validate(const RigidMeshContactSettings& c){
    if(!(c.clearance_skin_m>0&&std::isfinite(c.clearance_skin_m)&&
         c.activation_width_m>=0&&std::isfinite(c.activation_width_m)&&
         c.restitution>=0&&c.restitution<=1&&
         std::isfinite(c.restitution)&&c.friction>=0&&
         std::isfinite(c.friction)&&
         c.impulse_iterations>=1&&c.impulse_iterations<=10&&
         c.interval_depth>=4&&c.interval_depth<=18&&
         c.maximum_bisection_depth<=20&&
         c.maximum_accepted_leaves>=1&&
         c.minimum_dt_s>0&&std::isfinite(c.minimum_dt_s)))
        throw std::invalid_argument("invalid finite rigid mesh contact settings");
}
struct MeshPhysicalLeaf{
    double start_time_s{},end_time_s{},dt_s{};
    double closest_face_gap_m{},minimum_tetra_j{};
    double external_normal_impulse_ns{};
    V3 actual_external_contact_impulse{};
    std::size_t active_face_pairs{},face_impulse_updates{},
        continuous_pair_tests{},interval_nodes{};
    unsigned subdivision_depth{};
};
// Original accepted FEM start/end states are stored only when a caller
// explicitly requests this verification trace. Every record corresponds
// to an actually integrated leaf, never a rejected trial or fabricated
// interpolation between separate solver macro-steps.
struct MeshAcceptedTransition{
    Body before{}, after{};
    MeshPhysicalLeaf receipt{};
};
struct MeshStepResult{
    bool accepted{};
    bool was_rejected_by_continuous_geometry{};
    bool was_rejected_by_tetrahedron{};
    std::size_t face_impulse_updates{},active_face_pairs{};
    std::size_t broadphase_box_tests{},continuous_pair_tests{},
                interval_nodes{};
    double normal_impulse_ns{},tangential_impulse_ns{};
    V3 external_contact_impulse{};
    double minimum_face_clearance_m{};
    double minimum_tetra_j{};
};
struct MeshAdaptiveStats{
    double start_s{},end_s{},integrated_dt_s{};
    std::size_t accepted_leaves{},rejected_geom_trials{},
                rejected_tetra_trials{},continuous_pair_tests{},
                interval_nodes{},face_impulse_updates{},
                active_face_pairs{};
    double min_clearance_m{std::numeric_limits<double>::infinity()};
    double min_tetra_j{std::numeric_limits<double>::infinity()};
    double normal_impulse_ns{},tangential_impulse_ns{};
    V3 external_contact_impulse{};
};

// Per-contact impulses use deformable triangle barycentric nearest
// weights w_i, with effective inverse mass k=sum_i(w_i*w_i/m_i).
// The rigid target is static/infinite-mass, so its opposite reaction
// is not integrated. Geometry is 3D and two-sided, not oriented z.
inline void apply_rigid_mesh_impulses(
    const Body& body,const StaticTriangleMesh& rigid,
    const std::vector<Triangle3>& rigid_triangles,
    const std::vector<TriangleBox>& rigid_boxes,
    const std::vector<V3>& positions,std::vector<V3>& velocity,
    std::vector<double>& nodal_normal_impulses,
    const RigidMeshContactSettings& cfg,MeshStepResult& metrics){
    const double activation=cfg.clearance_skin_m+cfg.activation_width_m;
    for(unsigned iter=0;iter<cfg.impulse_iterations;iter++){
        for(const auto& face:body.surface_faces){
            const Triangle3 a={positions[face[0]],positions[face[1]],
                               positions[face[2]]};
            const auto aabb=bounding(a);
            for(std::size_t j=0;j<rigid.faces.size();j++){
                ++metrics.broadphase_box_tests;
                if(!maybe_near(aabb,rigid_boxes[j],activation))
                    continue;
                const auto nearest=triangle_triangle_closest(
                    a,rigid_triangles[j]);
                if(nearest.distance>activation)continue;
                if(iter==0)metrics.active_face_pairs++;
                if(!(nearest.distance>1e-12))
                    throw std::runtime_error(
                        "rigid mesh impulse normal indeterminate at overlap");
                const V3 normal=(nearest.on_deformable-
                                  nearest.on_rigid)/nearest.distance;
                double k=0;V3 u{};
                for(int i=0;i<3;i++){
                    const auto id=face[i];
                    const double w=nearest.deformable_weights[i];
                    k+=w*w/body.nodes[id].mass;
                    u+=velocity[id]*w;
                }
                if(!(k>0&&std::isfinite(k)))
                    throw std::runtime_error("rigid mesh contact effective mass invalid");
                const double un=dot(u,normal);
                if(un>=-1e-12)continue;
                const double jn=-(1+cfg.restitution)*un/k;
                const V3 tangent=u-normal*un;
                const double vt=norm(tangent);
                const double jt=vt>1e-15?
                    std::min(cfg.friction*jn,vt/k):0.;
                const V3 impulse=normal*jn-
                       (vt>1e-15?tangent*(jt/vt):V3{});
                for(int i=0;i<3;i++){
                    const auto id=face[i];
                    const double w=nearest.deformable_weights[i];
                    velocity[id]+=impulse*(w/body.nodes[id].mass);
                    nodal_normal_impulses[id]+=jn*w;
                }
                metrics.normal_impulse_ns+=jn;
                metrics.tangential_impulse_ns+=jt;
                metrics.external_contact_impulse+=impulse;
                metrics.face_impulse_updates++;
            }
        }
    }
}

inline MeshStepResult try_rigid_mesh_fem_full_step(
    Body& body,const StaticTriangleMesh& rigid,
    const std::vector<Triangle3>& rigid_triangles,
    const std::vector<TriangleBox>& rigid_boxes,
    double dt,const RigidMeshContactSettings& cfg) {
    MeshStepResult out;
    if(!(std::isfinite(dt)&&dt>0))
        throw std::invalid_argument("invalid rigid mesh physical trial dt");
    const auto starting=audit_deformable_rigid_mesh(
        body,rigid,cfg.clearance_skin_m*.5);
    if(!(starting.minimum_distance_m>cfg.clearance_skin_m*.5))
        throw std::runtime_error(
            "rigid mesh source body intersects contact skin");
    const auto fields=body.evaluate();
    const double damp=std::exp(-body.material.damping*dt);
    std::vector<V3> x,v,predicted;
    x.reserve(body.nodes.size());v.reserve(body.nodes.size());
    predicted.reserve(body.nodes.size());
    for(std::size_t i=0;i<body.nodes.size();i++){
        const auto& node=body.nodes[i];
        x.push_back(node.x);
        const V3 acceleration=fields.elastic_force[i]/node.mass+
                     V3{0,0,-body.material.gravity};
        v.push_back((node.v+acceleration*dt)*damp);
    }
    std::vector<double> nodal_impulses(body.nodes.size(),0.);
    // Apply velocity reaction before the trial; otherwise a common
    // gravitating near-contact state repeatedly clips and loses time.
    apply_rigid_mesh_impulses(body,rigid,rigid_triangles,rigid_boxes,
                             x,v,nodal_impulses,cfg,out);
    for(std::size_t i=0;i<x.size();i++)
        predicted.push_back(x[i]+v[i]*dt);
    for(const auto& face:body.surface_faces){
        const LinearTriangleMotion trajectory{
            {x[face[0]],x[face[1]],x[face[2]]},
            {predicted[face[0]],predicted[face[1]],
             predicted[face[2]]}};
        const auto box=swept_bounding(trajectory);
        for(std::size_t j=0;j<rigid.faces.size();j++){
            ++out.broadphase_box_tests;
            if(!maybe_near(box,rigid_boxes[j],cfg.clearance_skin_m))
                continue;
            out.continuous_pair_tests++;
            const auto c=certify_moving_triangle_rigid_triangle(
               trajectory,rigid_triangles[j],
               cfg.clearance_skin_m,cfg.interval_depth);
            out.interval_nodes+=c.nodes;
            if(!c.entirely_separated){
                out.was_rejected_by_continuous_geometry=true;
                return out; // do not modify the public physical state
            }
        }
    }
    // Test the ACTUAL new volumetric element Jacobians before commit.
    // An inverted proposal is rejected by caller and re-integrated with
    // a smaller actual physical step, not position-clamped at full dt.
    for(const auto& tet:body.tets){
        const V3 origin=predicted[tet.n[0]];
        const M3 Ds=from_cols(predicted[tet.n[1]]-origin,
            predicted[tet.n[2]]-origin,
            predicted[tet.n[3]]-origin);
        const double J=determinant(Ds*tet.inverse_rest);
        if(!(std::isfinite(J)&&J>1e-6)){
            out.was_rejected_by_tetrahedron=true;
            return out;
        }
    }
    // Only now is post-displacement impulse response permitted.
    apply_rigid_mesh_impulses(body,rigid,rigid_triangles,rigid_boxes,
                             predicted,v,nodal_impulses,cfg,out);
    Body candidate=body;
    for(std::size_t i=0;i<candidate.nodes.size();i++){
        candidate.nodes[i].x=predicted[i];
        candidate.nodes[i].v=v[i];
        candidate.nodes[i].last_normal_impulse=nodal_impulses[i];
    }
    const auto surface=audit_deformable_rigid_mesh(
        candidate,rigid,cfg.clearance_skin_m*.5);
    if(!(surface.minimum_distance_m>cfg.clearance_skin_m*.5)||
       surface.overlapping_face_pairs){
        out.was_rejected_by_continuous_geometry=true;
        return out;
    }
    const auto solid=candidate.evaluate();
    out.minimum_tetra_j=solid.min_jacobian;
    out.minimum_face_clearance_m=surface.minimum_distance_m;
    out.accepted=true;
    candidate.contact_count_step=out.face_impulse_updates;
    candidate.normal_impulse_step=out.normal_impulse_ns;
    candidate.time_seconds+=dt;
    body=std::move(candidate);
    return out;
}

inline MeshAdaptiveStats step_time_consistent_rigid_triangle_mesh(
    Body& body,const StaticTriangleMesh& rigid,double dt,
    const RigidMeshContactSettings& cfg={},
    std::vector<MeshPhysicalLeaf>* receipts=nullptr,
    std::vector<MeshAcceptedTransition>* accepted_transitions=nullptr){
    validate(rigid);validate(cfg);
    if(!(std::isfinite(dt)&&dt>=cfg.minimum_dt_s))
        throw std::invalid_argument("invalid mesh FEM timestep");
    if(body.nodes.empty()||body.surface_faces.empty()||body.tets.empty())
        throw std::invalid_argument("mesh FEM requires actual 3D tetrahedra");
    const auto initial_clock=body.time_seconds;
    Body staged=body;
    MeshAdaptiveStats result;
    result.start_s=initial_clock;
    std::vector<MeshPhysicalLeaf> local;
    std::vector<MeshAcceptedTransition> local_transitions;
    // Nodal impulses are accumulated across ACTUAL accepted physical
    // leaves. Rejected proposals do not contribute. Without this,
    // a macro-step's scalar impulse total would include all leaves
    // while the public per-vertex receipt showed only its last leaf.
    std::vector<double> accumulated_nodal_impulse(body.nodes.size(),0.);
    std::vector<Triangle3> triangles;
    std::vector<TriangleBox> boxes;
    triangles.reserve(rigid.faces.size());boxes.reserve(rigid.faces.size());
    for(std::size_t i=0;i<rigid.faces.size();i++){
        const auto tri=face_positions(rigid,i);
        triangles.push_back(tri);
        boxes.push_back(bounding(tri));
    }
    const auto subdivide=[&](auto&& self,double h,
                             unsigned depth)->void {
        if(result.accepted_leaves>=cfg.maximum_accepted_leaves)
            throw std::runtime_error(
                "mesh contact exceeded accepted physical leaf budget");
        if(h<cfg.minimum_dt_s)
            throw std::runtime_error(
                "mesh contact cannot certify minimum physical timestep");
        Body trial=staged;
        const auto r=try_rigid_mesh_fem_full_step(
            trial,rigid,triangles,boxes,h,cfg);
        result.continuous_pair_tests+=r.continuous_pair_tests;
        result.interval_nodes+=r.interval_nodes;
        if(!r.accepted){
            result.rejected_geom_trials+=r.was_rejected_by_continuous_geometry;
            result.rejected_tetra_trials+=r.was_rejected_by_tetrahedron;
            if(depth>=cfg.maximum_bisection_depth||
               h*.5<cfg.minimum_dt_s)
                throw std::runtime_error(
                  "mesh contact cannot resolve physical CCD event or "
                  "tet inversion within subdivision budget");
            const double half=h*.5;
            self(self,half,depth+1);
            self(self,h-half,depth+1);
            return;
        }
        local.push_back({staged.time_seconds,trial.time_seconds,h,
                         r.minimum_face_clearance_m,r.minimum_tetra_j,
                         r.normal_impulse_ns,r.external_contact_impulse,
                         r.active_face_pairs,r.face_impulse_updates,
                         r.continuous_pair_tests,r.interval_nodes,depth});
        // Write snapshots to a PRIVATE staging vector. If any later leaf
        // fails, the caller receives no partial successful trajectory.
        if(accepted_transitions)
            local_transitions.push_back({staged,trial,local.back()});
        for(std::size_t i=0;i<trial.nodes.size();i++)
            accumulated_nodal_impulse[i]+=
                trial.nodes[i].last_normal_impulse;
        result.integrated_dt_s+=h;
        result.accepted_leaves++;
        result.active_face_pairs+=r.active_face_pairs;
        result.face_impulse_updates+=r.face_impulse_updates;
        result.normal_impulse_ns+=r.normal_impulse_ns;
        result.tangential_impulse_ns+=r.tangential_impulse_ns;
        result.external_contact_impulse+=r.external_contact_impulse;
        result.min_clearance_m=std::min(result.min_clearance_m,
                                      r.minimum_face_clearance_m);
        result.min_tetra_j=std::min(result.min_tetra_j,
                                  r.minimum_tetra_j);
        staged=std::move(trial);
    };
    subdivide(subdivide,dt,0);
    const double tol=3e-12*std::max(1.,dt);
    if(std::abs(result.integrated_dt_s-dt)>tol||
       std::abs(staged.time_seconds-(initial_clock+dt))>tol)
        throw std::runtime_error("rigid mesh full physical time not consumed");
    staged.time_seconds=initial_clock+dt;
    staged.contact_count_step=result.face_impulse_updates;
    staged.normal_impulse_step=result.normal_impulse_ns;
    for(std::size_t i=0;i<staged.nodes.size();i++)
        staged.nodes[i].last_normal_impulse=
            accumulated_nodal_impulse[i];
    result.end_s=staged.time_seconds;
    body=std::move(staged);
    if(receipts)receipts->insert(receipts->end(),
                                local.begin(),local.end());
    if(accepted_transitions)
        accepted_transitions->insert(accepted_transitions->end(),
            std::make_move_iterator(local_transitions.begin()),
            std::make_move_iterator(local_transitions.end()));
    return result;
}

// Native finite, curved, TWO-SIDED nonplanar triangulated obstacle:
// every face is a physical finite triangle (never an analytic heightfield
// inside the collision solver), with consistent upward orientation.
inline StaticTriangleMesh corrugated_contact_patch(
    unsigned resolution=7,double height=.035){
    if(resolution<3||resolution>32||!std::isfinite(height))
        throw std::invalid_argument("invalid rigid mesh fixture size");
    StaticTriangleMesh m;
    for(unsigned y=0;y<resolution;y++)for(unsigned x=0;x<resolution;x++){
        const double u=2.3*double(x)/(resolution-1)-1.15;
        const double v=2.3*double(y)/(resolution-1)-1.15;
        m.vertices.push_back({u,v,height+
             .07*std::sin(3*u)*std::cos(2*v)+.025*u});
    }
    for(unsigned y=0;y+1<resolution;y++)
       for(unsigned x=0;x+1<resolution;x++){
        const auto a=std::size_t(y)*resolution+x;
        const auto b=a+1,c=a+resolution,d=c+1;
        m.faces.push_back({a,b,d});
        m.faces.push_back({a,d,c});
    }
    validate(m);
    return m;
}
} // namespace kyoto::solid
