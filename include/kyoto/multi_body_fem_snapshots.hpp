#pragma once
// Pass 21: physically integrated MULTI-OBJECT source feature adapter.
// Two neo-Hookean bodies are integrated independently against one
// ORIGINAL static finite triangular obstacle. The broadphase also
// considers cross-body VF/EE candidates, but their collision RESPONSE is
// intentionally not solved. Never advertise this as coupled deformable
// vs deformable dynamics. A disjoint-body geometry audit is required.
//
// All three input object's original vertex IDs are remapped to ONE
// canonical shared index space, with stable body IDs and deterministic
// globally unique edge/face IDs. This adapter does not invent positions:
// four endpoint/linearity controls reflect source solver states.
#include "kyoto/fem_feature_snapshots.hpp"
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <set>
#include <stdexcept>
#include <utility>

namespace kyoto::contact::fem {

// Conservative x-axis gap of two deformable bodies over one accepted
// LINEAR physical leaf (the source solver must have accepted exactly
// one leaf over this interval). If positive by more than the contact
// clearance, the bodies cannot collide during that motion in 3D:
// their x-coordinate convex hulls are disjoint at all t in [0,1].
// This is a sufficient separation witness, not a general CCD solver.
inline double uncoupled_two_body_swept_x_gap(
    const solid::Body& before_a,const solid::Body& after_a,
    const solid::Body& before_b,const solid::Body& after_b){
    if(before_a.nodes.empty()||before_b.nodes.empty() ||
       before_a.nodes.size()!=after_a.nodes.size() ||
       before_b.nodes.size()!=after_b.nodes.size())
        throw std::invalid_argument("invalid physical two-body swept time interval");
    const double inf=std::numeric_limits<double>::infinity();
    double max_a=-inf,min_b=inf;
    for(std::size_t i=0;i<before_a.nodes.size();++i){
        const auto p=before_a.nodes[i].x,q=after_a.nodes[i].x;
        if(!solid::finite(p)||!solid::finite(q))
            throw std::invalid_argument("nonfinite swept first physical body");
        max_a=std::max({max_a,p.x,q.x});
    }
    for(std::size_t i=0;i<before_b.nodes.size();++i){
        const auto p=before_b.nodes[i].x,q=after_b.nodes[i].x;
        if(!solid::finite(p)||!solid::finite(q))
            throw std::invalid_argument("nonfinite swept second physical body");
        min_b=std::min({min_b,p.x,q.x});
    }
    // Round both extrema outward so the gap is not artificially enlarged.
    return std::nextafter(min_b,-inf)-std::nextafter(max_a,inf);
}

inline features::Mesh uncoupled_two_body_feature_snapshot(
    const solid::Body& before_a,const solid::Body& after_a,
    const solid::Body& before_b,const solid::Body& after_b,
    const solid::StaticTriangleMesh& rigid,double skin=.0005) {
    if(!(std::isfinite(skin)&&skin>=0))
        throw std::invalid_argument("invalid multibody contact skin");
    if(before_a.nodes.size()!=after_a.nodes.size() ||
       before_b.nodes.size()!=after_b.nodes.size() ||
       before_a.surface_faces!=after_a.surface_faces ||
       before_b.surface_faces!=after_b.surface_faces ||
       before_a.surface_edges!=after_a.surface_edges ||
       before_b.surface_edges!=after_b.surface_edges ||
       before_a.tets.size()!=after_a.tets.size() ||
       before_b.tets.size()!=after_b.tets.size())
        throw std::invalid_argument("physically integrated multibody source topology changed");
    if(before_a.nodes.empty()||before_b.nodes.empty()||rigid.vertices.empty())
        throw std::invalid_argument("empty physical multibody source object");
    const auto na=std::uint64_t(before_a.nodes.size());
    const auto nb=std::uint64_t(before_b.nodes.size());
    if(before_a.nodes.size()>std::size_t(std::numeric_limits<int>::max()) ||
       before_b.nodes.size()>std::size_t(std::numeric_limits<int>::max()) ||
       rigid.vertices.size()>std::size_t(std::numeric_limits<int>::max()))
        throw std::overflow_error("physical source vertex indexing exceeds bounded benchmark contract");

    features::Mesh m;
    m.thickness=skin;
    m.vertices.reserve(before_a.nodes.size()+before_b.nodes.size()+
                       rigid.vertices.size());
    m.faces.reserve(before_a.surface_faces.size()+
                    before_b.surface_faces.size()+rigid.faces.size());
    std::set<std::pair<std::uint64_t,std::uint64_t>> undirected_edges;
    const auto append_vertex=[&](std::uint64_t id,solid::V3 p,solid::V3 q,
                                 std::uint64_t object) {
        m.vertices.push_back({id,linear_vertex(p,q),object});
    };
    for(std::size_t i=0;i<before_a.nodes.size();++i)
        append_vertex(std::uint64_t(i),before_a.nodes[i].x,
                      after_a.nodes[i].x,0);
    for(std::size_t i=0;i<before_b.nodes.size();++i)
        append_vertex(na+std::uint64_t(i),before_b.nodes[i].x,
                      after_b.nodes[i].x,1);
    for(std::size_t i=0;i<rigid.vertices.size();++i)
        append_vertex(na+nb+std::uint64_t(i),rigid.vertices[i],
                      rigid.vertices[i],2);

    const auto append_face=[&](std::array<std::uint64_t,3> v){
        m.faces.push_back({std::uint64_t(m.faces.size()),v});
        for(int j=0;j<3;++j){
            auto a=v[j],b=v[(j+1)%3];
            if(a>b)std::swap(a,b);
            undirected_edges.emplace(a,b);
        }
    };
    for(const auto& f:before_a.surface_faces)
        append_face({std::uint64_t(f[0]),std::uint64_t(f[1]),
                     std::uint64_t(f[2])});
    for(const auto& f:before_b.surface_faces)
        append_face({na+std::uint64_t(f[0]),na+std::uint64_t(f[1]),
                     na+std::uint64_t(f[2])});
    for(const auto& f:rigid.faces)
        append_face({na+nb+std::uint64_t(f[0]),na+nb+std::uint64_t(f[1]),
                     na+nb+std::uint64_t(f[2])});
    for(const auto& [a,b]:undirected_edges)
        m.edges.push_back({std::uint64_t(m.edges.size()),{a,b}});
    (void)features::build(m); // Verify the complete original ID, geometry and mask contract.
    return m;
}
} // namespace kyoto::contact::fem
