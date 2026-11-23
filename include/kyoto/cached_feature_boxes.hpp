#pragma once
// Pass 16: topology-validated, maintained swept-feature box builder.
//
// Cold path pays full independent f::build() validation plus ID-index
// preparation. Warm path recomputes EVERY conservative vertex/edge/face
// bound, but reuses verified topology lookup tables instead of repeating
// ordered-map construction for every physical timestep.
//
// This cache is PRIVATE to a caller that independently verifies a complete
// topology signature on every invocation (RefitHierarchy). The caller MUST
// pass rebuild=true whenever IDs, mesh epoch, source connectivity, masks,
// object labels, categories, record ordering or record counts change.
// Thickness and trajectories are dynamic and always recomputed.
// The algorithm does NOT cache candidate pairs, CCD events, or source physics.
#include "kyoto/feature_broadphase.hpp"
#include <array>
#include <cstddef>
#include <map>
#include <stdexcept>
#include <utility>
#include <vector>

namespace kyoto::contact::atlas {
namespace f=kyoto::contact::features;

class CachedFeatureBoxes {
    f::Built records_;
    std::vector<std::array<std::size_t,2>> edge_vertex_indices_;
    std::vector<std::array<std::size_t,3>> face_vertex_indices_;
    bool initialized_{};

    void rebuild_from(const f::Mesh& mesh) {
        // Prepare everything off to the side; a failed source mesh leaves
        // the previously successful cache intact.
        auto fresh=f::build(mesh);
        std::map<std::uint64_t,std::size_t> id_to_vertex;
        for(std::size_t i=0;i<mesh.vertices.size();++i)
            id_to_vertex.emplace(mesh.vertices[i].id,i);
        std::vector<std::array<std::size_t,2>> edges;
        std::vector<std::array<std::size_t,3>> faces;
        edges.reserve(mesh.edges.size());faces.reserve(mesh.faces.size());
        for(const auto& e:mesh.edges)
            edges.push_back({id_to_vertex.at(e.vertices[0]),
                             id_to_vertex.at(e.vertices[1])});
        for(const auto& face:mesh.faces)
            faces.push_back({id_to_vertex.at(face.vertices[0]),
                             id_to_vertex.at(face.vertices[1]),
                             id_to_vertex.at(face.vertices[2])});
        records_=std::move(fresh);
        edge_vertex_indices_=std::move(edges);
        face_vertex_indices_=std::move(faces);
        initialized_=true;
    }
public:
    const f::Built& update(const f::Mesh& mesh,bool rebuild) {
        if(rebuild||!initialized_) {
            rebuild_from(mesh);
            return records_;
        }
        if(mesh.topology_epoch!=records_.topology_epoch ||
           mesh.vertices.size()!=records_.vertices.size() ||
           mesh.edges.size()!=records_.edges.size() ||
           mesh.faces.size()!=records_.faces.size())
            throw std::logic_error(
                "warm feature box cache used across invalid topology");

        // No candidate can be emitted until ALL swept boxes have been
        // rebuilt. A failed numeric conversion must abort before querying
        // a possibly partially updated hierarchy.
        std::vector<Aabb> vertex_bounds;
        vertex_bounds.reserve(mesh.vertices.size());
        for(const auto& vertex:mesh.vertices)
            vertex_bounds.push_back(f::vertex_box(vertex.path,mesh.thickness));

        for(std::size_t i=0;i<vertex_bounds.size();++i)
            records_.vertices[i].bounds=vertex_bounds[i];
        for(std::size_t i=0;i<edge_vertex_indices_.size();++i) {
            const auto ids=edge_vertex_indices_[i];
            records_.edges[i].bounds=f::unite(vertex_bounds[ids[0]],
                                              vertex_bounds[ids[1]]);
        }
        for(std::size_t i=0;i<face_vertex_indices_.size();++i) {
            const auto ids=face_vertex_indices_[i];
            records_.faces[i].bounds=f::unite(
                f::unite(vertex_bounds[ids[0]],vertex_bounds[ids[1]]),
                vertex_bounds[ids[2]]);
        }
        return records_;
    }
};
} // namespace kyoto::contact::atlas
