#pragma once
// Pass 20: fully paid, selective swept-feature regeneration.
// Experimental alternative to CachedFeatureBoxes, NOT an unverified
// replacement of the full refit path.
//
// Invariant: a warm query checks the COMPLETE input feature signature
// externally, compares EVERY vertex's current cubic motion and thickness,
// recomputes EVERY changed vertex envelope with the production outward
// rounding rules, then refreshes EVERY incident edge and face envelope.
//
// A face/edge is dirty if any constituent vertex path changed, whether
// or not the derived floating-point box happens to remain equal.
// Stationary vertices with unchanged skin reuse their prior validated
// boxes. A cold rebuild independently validates the entire source mesh.
#include "kyoto/feature_broadphase.hpp"
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <map>
#include <stdexcept>
#include <utility>
#include <vector>

namespace kyoto::contact::atlas {
namespace f = kyoto::contact::features;

class SelectiveFeatureBoxes {
    f::Built built_;
    std::vector<CubicVertex> paths_;
    std::vector<std::array<std::size_t,2>> edge_vertices_;
    std::vector<std::array<std::size_t,3>> face_vertices_;
    std::vector<std::vector<std::size_t>> incident_edges_,incident_faces_;
    std::vector<std::size_t> dirty_vertices_,dirty_edges_,dirty_faces_;
    double thickness_{};
    bool ready_{};

    static bool same(const CubicVertex& a,const CubicVertex& b) {
        for(std::size_t i=0;i<a.control.size();++i) {
            const auto& x=a.control[i];
            const auto& y=b.control[i];
            if(x.x!=y.x || x.y!=y.y || x.z!=y.z)return false;
        }
        return true;
    }
    void cold(const f::Mesh& mesh) {
        // All allocations and input validation are paid within update().
        auto verified=f::build(mesh);
        std::map<std::uint64_t,std::size_t> indices;
        for(std::size_t i=0;i<mesh.vertices.size();++i)
            indices.emplace(mesh.vertices[i].id,i);

        std::vector<std::array<std::size_t,2>> edges;
        std::vector<std::array<std::size_t,3>> faces;
        std::vector<std::vector<std::size_t>> incident_edges(mesh.vertices.size());
        std::vector<std::vector<std::size_t>> incident_faces(mesh.vertices.size());
        for(const auto& e:mesh.edges) {
            const auto at=edges.size();
            std::array<std::size_t,2> ids{
                indices.at(e.vertices[0]),indices.at(e.vertices[1])};
            edges.push_back(ids);
            for(auto v:ids)incident_edges[v].push_back(at);
        }
        for(const auto& face:mesh.faces) {
            const auto at=faces.size();
            std::array<std::size_t,3> ids{
                indices.at(face.vertices[0]),indices.at(face.vertices[1]),
                indices.at(face.vertices[2])};
            faces.push_back(ids);
            for(auto v:ids)incident_faces[v].push_back(at);
        }
        std::vector<CubicVertex> paths;
        paths.reserve(mesh.vertices.size());
        for(const auto& v:mesh.vertices)paths.push_back(v.path);
        // Commit only after the full source mesh and every dependency
        // has validated. Prior state remains untouched on failure.
        built_=std::move(verified);
        paths_=std::move(paths);
        edge_vertices_=std::move(edges);
        face_vertices_=std::move(faces);
        incident_edges_=std::move(incident_edges);
        incident_faces_=std::move(incident_faces);
        thickness_=mesh.thickness;
        ready_=true;
        dirty_vertices_.clear();dirty_edges_.clear();dirty_faces_.clear();
        for(std::size_t i=0;i<built_.vertices.size();++i)
            dirty_vertices_.push_back(i);
        for(std::size_t i=0;i<built_.edges.size();++i)
            dirty_edges_.push_back(i);
        for(std::size_t i=0;i<built_.faces.size();++i)
            dirty_faces_.push_back(i);
    }
public:
    const std::vector<std::size_t>& dirty_vertices()const{return dirty_vertices_;}
    const std::vector<std::size_t>& dirty_edges()const{return dirty_edges_;}
    const std::vector<std::size_t>& dirty_faces()const{return dirty_faces_;}
    const f::Built& update(const f::Mesh& mesh,bool topology_changed) {
        if(topology_changed||!ready_) {
            cold(mesh);
            return built_;
        }
        if(mesh.topology_epoch!=built_.topology_epoch ||
           mesh.vertices.size()!=built_.vertices.size() ||
           mesh.edges.size()!=built_.edges.size() ||
           mesh.faces.size()!=built_.faces.size())
            throw std::logic_error("selective cache used across changed topology");
        if(!(std::isfinite(mesh.thickness)&&mesh.thickness>=0))
            throw std::invalid_argument("invalid selective swept-box thickness");

        // Phase 1: stage and validate all changed bounds before altering
        // any cached box, even when a later input vertex is non-finite.
        std::vector<std::size_t> changed;
        std::vector<Aabb> fresh;
        changed.reserve(mesh.vertices.size());
        fresh.reserve(mesh.vertices.size());
        const bool all_dirty=mesh.thickness!=thickness_;
        for(std::size_t i=0;i<mesh.vertices.size();++i) {
            if(all_dirty||!same(paths_[i],mesh.vertices[i].path)) {
                changed.push_back(i);
                fresh.push_back(f::vertex_box(mesh.vertices[i].path,
                                               mesh.thickness));
            }
        }
        std::vector<unsigned char> edge_mark(mesh.edges.size(),0);
        std::vector<unsigned char> face_mark(mesh.faces.size(),0);
        std::vector<std::size_t> changed_edges,changed_faces;
        for(auto v:changed) {
            for(auto e:incident_edges_[v]) {
                if(!edge_mark[e]) {
                    edge_mark[e]=1;
                    changed_edges.push_back(e);
                }
            }
            for(auto face:incident_faces_[v]) {
                if(!face_mark[face]) {
                    face_mark[face]=1;
                    changed_faces.push_back(face);
                }
            }
        }
        // Phase 2: update leaves before reconstructing incident feature
        // bounds. Every dependent feature is refreshed exactly once.
        for(std::size_t i=0;i<changed.size();++i) {
            const auto v=changed[i];
            built_.vertices[v].bounds=fresh[i];
            paths_[v]=mesh.vertices[v].path;
        }
        for(auto e:changed_edges){
            const auto ids=edge_vertices_[e];
            built_.edges[e].bounds=f::unite(
                built_.vertices[ids[0]].bounds,
                built_.vertices[ids[1]].bounds);
        }
        for(auto face:changed_faces){
            const auto ids=face_vertices_[face];
            built_.faces[face].bounds=f::unite(
                f::unite(built_.vertices[ids[0]].bounds,
                         built_.vertices[ids[1]].bounds),
                built_.vertices[ids[2]].bounds);
        }
        thickness_=mesh.thickness;
        dirty_vertices_=std::move(changed);
        dirty_edges_=std::move(changed_edges);
        dirty_faces_=std::move(changed_faces);
        return built_;
    }
};
} // namespace kyoto::contact::atlas
