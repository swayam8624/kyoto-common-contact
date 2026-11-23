#pragma once
// Shared original-solver feature snapshot adaptor for Pass 13 / Pass 15.
// Both internal refittable trees and upstream IPC LBVH MUST receive the
// same IDs, connectivity, linearized macro-step endpoints and skin.
// This represents *macro-step endpoint-linear interpolation*, not
// unrecorded nonlinear/subdivided solver paths or physically certified CCD.
#include "kyoto/feature_broadphase.hpp"
#include "kyoto/rigid_mesh_fem_contact.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <set>
#include <stdexcept>
#include <utility>

namespace kyoto::contact::fem {
inline Vec3 cast(solid::V3 v){return {v.x,v.y,v.z};}

inline CubicVertex linear_vertex(solid::V3 a,solid::V3 b) {
    const Vec3 p=cast(a),q=cast(b);
    return {{p,affine(p,q,1./3.),affine(p,q,2./3.),q}};
}

inline features::Mesh feature_snapshot(
    const solid::Body& before,const solid::Body& after,
    const solid::StaticTriangleMesh& rigid,double skin=.0005) {
    if(before.nodes.size()!=after.nodes.size() ||
       before.surface_faces.size()!=after.surface_faces.size())
        throw std::invalid_argument("solver macro-step changed FEM topology");
    features::Mesh mesh;
    mesh.thickness=skin;
    const auto nv=std::uint64_t(before.nodes.size());
    for(std::size_t i=0;i<before.nodes.size();++i) {
        mesh.vertices.push_back({std::uint64_t(i),
            linear_vertex(before.nodes[i].x,after.nodes[i].x),0});
    }
    for(std::size_t i=0;i<rigid.vertices.size();++i) {
        mesh.vertices.push_back({nv+std::uint64_t(i),
            linear_vertex(rigid.vertices[i],rigid.vertices[i]),1});
    }
    std::set<std::pair<std::uint64_t,std::uint64_t>> edges;
    const auto add_face=[&](std::array<std::uint64_t,3> v) {
        mesh.faces.push_back({std::uint64_t(mesh.faces.size()),v});
        for(int j=0;j<3;++j) {
            auto a=v[j],b=v[(j+1)%3];
            if(a>b)std::swap(a,b);
            edges.emplace(a,b);
        }
    };
    for(const auto& tri:before.surface_faces)
        add_face({std::uint64_t(tri[0]),std::uint64_t(tri[1]),
                  std::uint64_t(tri[2])});
    for(const auto& tri:rigid.faces)
        add_face({nv+std::uint64_t(tri[0]),nv+std::uint64_t(tri[1]),
                  nv+std::uint64_t(tri[2])});
    for(const auto& [a,b]:edges)
        mesh.edges.push_back({std::uint64_t(mesh.edges.size()),{a,b}});
    (void)features::build(mesh); // Enforce the identical typed mesh contract.
    return mesh;
}
} // namespace kyoto::contact::fem
