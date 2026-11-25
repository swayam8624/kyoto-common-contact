#pragma once
// Pass 32A: restricted self-contact for nonincident surface face pairs.
// Pairs sharing ANY vertex are excluded from the face/face contract.
// This avoids imposing positive clearance on adjacent faces that must
// touch. It does NOT cover nonincident VF/EE within adjacent face pairs,
// nor local folding/containment. Each impulse uses six DISTINCT node IDs
// and writes both sides into the same Body, preserving internal momentum.
#include "kyoto/coupled_triangle_normal_impulse.hpp"
#include "kyoto/rounded_relative_triangle_certificate.hpp"
#include "kyoto/static_face_hierarchy.hpp"
#include <optional>
namespace kyoto::solid {
struct SelfContactStats{
    std::size_t box_tests{},incident_exclusions{},certificate_tests{},interval_nodes{},
                projections{},impulse_updates{};
    double normal_impulse{},contact_work{};
};
using SelfFace=std::array<std::size_t,3>;
inline bool self_incident(const SelfFace& a,const SelfFace& b){
    for(auto i:a)for(auto j:b)if(i==j)return true;
    return false;
}
inline Triangle3 self_positions(const Body& b,const SelfFace& f){
    return {b.nodes.at(f[0]).x,b.nodes.at(f[1]).x,b.nodes.at(f[2]).x};
}
inline ContactTriangleState self_state(const Body& b,const SelfFace& f){
    ContactTriangleState t;t.x=self_positions(b,f);
    for(int i=0;i<3;++i){t.v[i]=b.nodes.at(f[i]).v;t.mass[i]=b.nodes.at(f[i]).mass;}
    return t;
}
inline void apply_nonincident_self_impulses(Body& b,double skin,double activation,
        double restitution,unsigned iterations,bool hierarchical,SelfContactStats& s){
    if(!(std::isfinite(skin)&&skin>0&&std::isfinite(activation)&&activation>=skin))
        throw std::invalid_argument("invalid self-contact activation");
    std::vector<TriangleBox> boxes;
    for(const auto& f:b.surface_faces)boxes.push_back(bounding(self_positions(b,f)));
    std::optional<StaticFaceHierarchy> tree;
    if(hierarchical)tree.emplace(boxes);
    std::vector<std::size_t> rows;
    for(unsigned it=0;it<iterations;++it)for(std::size_t i=0;i<boxes.size();++i){
        collect_static_face_rows(boxes[i],activation,boxes,tree?&*tree:nullptr,rows,s.box_tests);
        const auto& f=b.surface_faces[i];
        for(auto j:rows){
            if(j<=i)continue;
            const auto& g=b.surface_faces[j];
            if(self_incident(f,g)){++s.incident_exclusions;continue;}
            auto a=self_state(b,f),c=self_state(b,g);
            const auto near=triangle_triangle_closest(a.x,c.x);
            if(near.distance>activation)continue;
            if(!(near.distance>skin))throw std::runtime_error("nonincident self pair violates skin");
            const auto r=apply_two_triangle_normal_impulse(a,c,near.deformable_weights,
                                                          near.rigid_weights,restitution);
            if(r.normal_impulse==0.)continue;
            // No shared node can be overwritten by the opposite stencil.
            for(int k=0;k<3;++k){
                b.nodes[f[k]].v=a.v[k];b.nodes[g[k]].v=c.v[k];
                b.nodes[f[k]].last_normal_impulse+=r.normal_impulse*near.deformable_weights[k];
                b.nodes[g[k]].last_normal_impulse+=r.normal_impulse*near.rigid_weights[k];
            }
            ++s.impulse_updates;s.normal_impulse+=r.normal_impulse;
            s.contact_work+=r.predicted_kinetic_change;
        }
    }
}
inline bool certify_nonincident_self_sweep(const Body& a,const Body& b,
        double skin,unsigned depth,bool hierarchical,SelfContactStats& s){
    if(a.nodes.size()!=b.nodes.size()||a.surface_faces!=b.surface_faces||
       !(std::isfinite(skin)&&skin>0)||!rounded::prerequisites())
        throw std::invalid_argument("invalid self-contact sweep");
    std::vector<LinearTriangleMotion> paths;
    std::vector<TriangleBox> boxes;
    for(const auto& f:a.surface_faces){
        paths.push_back({self_positions(a,f),self_positions(b,f)});
        boxes.push_back(swept_bounding(paths.back()));
    }
    std::optional<StaticFaceHierarchy> tree;
    if(hierarchical)tree.emplace(boxes);
    std::vector<std::size_t> rows;
    for(std::size_t i=0;i<paths.size();++i){
        collect_static_face_rows(boxes[i],rounded::up(skin+1e-12),boxes,
                                 tree?&*tree:nullptr,rows,s.box_tests);
        for(auto j:rows){
            if(j<=i)continue;
            if(self_incident(a.surface_faces[i],a.surface_faces[j])){
                ++s.incident_exclusions;continue;
            }
            ++s.certificate_tests;
            const auto c=certify_rounded_relative_triangle_pair(paths[i],paths[j],skin,depth);
            s.interval_nodes+=c.nodes;s.projections+=c.interval_projections;
            if(!c.entirely_separated)return false;
        }
    }
    return true;
}
} // namespace kyoto::solid
