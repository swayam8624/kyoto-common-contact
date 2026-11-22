#pragma once
// Kyoto SCA Pass 3: a common, mesh-aware broad-phase contract.
// This file deliberately mirrors the TWO essential 3D feature families
// used by IPC-style surface contact: vertex-face and edge-edge.
// The output is a conservative *candidate* set, NOT actual CCD events.
//
// Topology is supplied explicitly. Only incident vertex-face pairs and
// edges sharing a vertex are rejected by this contract. More aggressive
// one-ring, rest-pose, material or solver-specific filters must be added
// as explicit equivalence-tested policies, NEVER guessed.
//
// Every vertex carries a full cubic Bezier path; its control-point AABB
// conservatively contains the entire spatial path (in exact arithmetic).
// Full box extraction, adjacency filtering, and normalization are paid work.

#include "kyoto/motion_envelopes.hpp"
#include <algorithm>
#include <array>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <map>
#include <numeric>
#include <stdexcept>
#include <utility>
#include <vector>

namespace kyoto::contact::features {

struct Vertex {
    std::uint64_t id{};
    CubicVertex path{};
    std::uint64_t object{};
    std::uint64_t category{1}, mask{~std::uint64_t{0}};
};
struct Edge {
    std::uint64_t id{};
    std::array<std::uint64_t,2> vertices{};
    std::uint64_t category{1},mask{~std::uint64_t{0}};
};
struct Face {
    std::uint64_t id{};
    std::array<std::uint64_t,3> vertices{};
    std::uint64_t category{1},mask{~std::uint64_t{0}};
};
struct Mesh {
    std::uint64_t topology_epoch{1};
    double thickness{0.0};
    std::vector<Vertex> vertices;
    std::vector<Edge> edges;
    std::vector<Face> faces;
};
enum class Kind: std::uint8_t { VertexFace=0, EdgeEdge=1 };
struct Candidate {
    Kind kind{Kind::VertexFace};
    std::uint64_t first{}, second{};
    auto operator<=>(const Candidate&) const = default;
};
struct Record {
    std::uint64_t id{};
    Aabb bounds{};
    std::array<std::uint64_t,3> ids{};
    unsigned arity{};
    std::uint64_t category{1},mask{~std::uint64_t{0}};
};
struct Built {
    std::uint64_t topology_epoch{};
    std::vector<Record> vertices,edges,faces;
};
struct Result {
    std::vector<Candidate> pairs;
    std::size_t candidate_aabb_tests{};
    std::size_t incident_rejections{};
    std::size_t mask_rejections{};
};

inline Aabb unite(const Aabb& a,const Aabb& b) {
    if(!valid(a)||!valid(b))
        throw std::invalid_argument("invalid feature AABB");
    return {{std::min(a.min.x,b.min.x),std::min(a.min.y,b.min.y),
             std::min(a.min.z,b.min.z)},
            {std::max(a.max.x,b.max.x),std::max(a.max.y,b.max.y),
             std::max(a.max.z,b.max.z)}};
}
inline Aabb vertex_box(const CubicVertex& path,double thickness) {
    if(!std::isfinite(thickness)||thickness<0)
        throw std::invalid_argument("invalid vertex reach");
    const double inf=std::numeric_limits<double>::infinity();
    Aabb b{{inf,inf,inf},{-inf,-inf,-inf}};
    for(const auto& p:path.control) {
        if(!finite(p))throw std::invalid_argument("nonfinite vertex control");
        b.min.x=std::min(b.min.x,p.x);b.max.x=std::max(b.max.x,p.x);
        b.min.y=std::min(b.min.y,p.y);b.max.y=std::max(b.max.y,p.y);
        b.min.z=std::min(b.min.z,p.z);b.max.z=std::max(b.max.z,p.z);
    }
    b.min.x=std::nextafter(b.min.x-thickness,-inf);
    b.min.y=std::nextafter(b.min.y-thickness,-inf);
    b.min.z=std::nextafter(b.min.z-thickness,-inf);
    b.max.x=std::nextafter(b.max.x+thickness,inf);
    b.max.y=std::nextafter(b.max.y+thickness,inf);
    b.max.z=std::nextafter(b.max.z+thickness,inf);
    if(!valid(b))throw std::overflow_error("vertex box overflow");
    return b;
}
inline bool allows(const Record& a,const Record& b) {
    return (a.category&b.mask)!=0 && (b.category&a.mask)!=0;
}
inline bool incident(const Record& a,const Record& b) {
    for(unsigned i=0;i<a.arity;i++)
        for(unsigned j=0;j<b.arity;j++)
            if(a.ids[i]==b.ids[j])return true;
    return false;
}
inline void normalize(Result& r) {
    std::sort(r.pairs.begin(),r.pairs.end());
    r.pairs.erase(std::unique(r.pairs.begin(),r.pairs.end()),r.pairs.end());
}
// Validate complete supplied mesh and derive swept feature AABBs.
// Building anew every query is deliberate: this is the immutable,
// correctness-focused *reference* path, not an invalidation cache.
inline Built build(const Mesh& mesh) {
    if(mesh.topology_epoch==0)
        throw std::invalid_argument("topology epoch must be nonzero");
    if(!std::isfinite(mesh.thickness)||mesh.thickness<0)
        throw std::invalid_argument("invalid thickness");
    Built out;out.topology_epoch=mesh.topology_epoch;
    std::map<std::uint64_t,Aabb> boxes;
    for(const auto& v:mesh.vertices) {
        const auto b=vertex_box(v.path,mesh.thickness);
        if(!boxes.emplace(v.id,b).second)
            throw std::invalid_argument("duplicate vertex id");
        out.vertices.push_back({v.id,b,{v.id,0,0},1,v.category,v.mask});
    }
    std::map<std::uint64_t,bool> edge_ids,face_ids;
    for(const auto& e:mesh.edges) {
        if(!edge_ids.emplace(e.id,true).second)
            throw std::invalid_argument("duplicate edge id");
        if(e.vertices[0]==e.vertices[1])
            throw std::invalid_argument("degenerate topological edge");
        const auto a=boxes.find(e.vertices[0]),b=boxes.find(e.vertices[1]);
        if(a==boxes.end()||b==boxes.end())
            throw std::invalid_argument("edge references missing vertex");
        out.edges.push_back({e.id,unite(a->second,b->second),
                             {e.vertices[0],e.vertices[1],0},2,e.category,e.mask});
    }
    for(const auto& f:mesh.faces) {
        if(!face_ids.emplace(f.id,true).second)
            throw std::invalid_argument("duplicate face id");
        if(f.vertices[0]==f.vertices[1]||f.vertices[0]==f.vertices[2]||
           f.vertices[1]==f.vertices[2])
            throw std::invalid_argument("degenerate topological face");
        const auto a=boxes.find(f.vertices[0]),b=boxes.find(f.vertices[1]),
                   c=boxes.find(f.vertices[2]);
        if(a==boxes.end()||b==boxes.end()||c==boxes.end())
            throw std::invalid_argument("face references missing vertex");
        out.faces.push_back({f.id,unite(unite(a->second,b->second),c->second),
                             f.vertices,3,f.category,f.mask});
    }
    return out;
}
inline bool valid_pair(const Record& a,const Record& b,Result& result) {
    if(!allows(a,b)) {
        ++result.mask_rejections;
        return false;
    }
    if(incident(a,b)) {
        ++result.incident_rejections;
        return false;
    }
    ++result.candidate_aabb_tests;
    return overlaps(a.bounds,b.bounds);
}
// Independent O(E^2 + V*F) exhaustive reference, including adjacency.
inline Result exhaustive(const Built& mesh) {
    Result result;
    for(const auto& v:mesh.vertices)for(const auto& f:mesh.faces)
        if(valid_pair(v,f,result))
            result.pairs.push_back({Kind::VertexFace,v.id,f.id});
    for(std::size_t i=0;i<mesh.edges.size();++i)
        for(std::size_t j=i+1;j<mesh.edges.size();++j)
            if(valid_pair(mesh.edges[i],mesh.edges[j],result))
                result.pairs.push_back({
                    Kind::EdgeEdge,
                    std::min(mesh.edges[i].id,mesh.edges[j].id),
                    std::max(mesh.edges[i].id,mesh.edges[j].id)});
    normalize(result);
    return result;
}
// Separate x-axis sweep-and-prune implementation.
// Cross type (vertex vs face) uses a unified sweep with two active lists.
// Each valid pair is seen once; inclusive touching is preserved.
inline Result sweep_and_prune(const Built& mesh) {
    Result result;
    std::vector<std::size_t> order(mesh.edges.size());
    std::iota(order.begin(),order.end(),std::size_t{0});
    std::sort(order.begin(),order.end(),[&](auto i,auto j){
        const auto& a=mesh.edges[i],&b=mesh.edges[j];
        if(a.bounds.min.x!=b.bounds.min.x)
            return a.bounds.min.x<b.bounds.min.x;
        return a.id<b.id;
    });
    std::vector<std::size_t> active;
    for(const auto i:order){
        const auto& a=mesh.edges[i];
        active.erase(std::remove_if(active.begin(),active.end(),[&](auto j){
            return mesh.edges[j].bounds.max.x<a.bounds.min.x;
        }),active.end());
        for(const auto j:active) {
            const auto& b=mesh.edges[j];
            if(valid_pair(a,b,result))
                result.pairs.push_back({Kind::EdgeEdge,
                    std::min(a.id,b.id),std::max(a.id,b.id)});
        }
        active.push_back(i);
    }
    struct Entry{bool vertex;std::size_t index;double min_x;double max_x;};
    std::vector<Entry> entries;
    entries.reserve(mesh.vertices.size()+mesh.faces.size());
    for(std::size_t i=0;i<mesh.vertices.size();++i)
        entries.push_back({true,i,mesh.vertices[i].bounds.min.x,
                          mesh.vertices[i].bounds.max.x});
    for(std::size_t i=0;i<mesh.faces.size();++i)
        entries.push_back({false,i,mesh.faces[i].bounds.min.x,
                           mesh.faces[i].bounds.max.x});
    std::sort(entries.begin(),entries.end(),[](const Entry& a,const Entry& b){
        if(a.min_x!=b.min_x)return a.min_x<b.min_x;
        if(a.vertex!=b.vertex)return a.vertex<b.vertex;
        return a.index<b.index;
    });
    std::vector<std::size_t> active_vertices,active_faces;
    for(const auto& e:entries) {
        auto& own=e.vertex?active_vertices:active_faces;
        auto& other=e.vertex?active_faces:active_vertices;
        auto& records=e.vertex?mesh.vertices:mesh.faces;
        const auto& opposing=e.vertex?mesh.faces:mesh.vertices;
        own.erase(std::remove_if(own.begin(),own.end(),[&](auto i){
            return records[i].bounds.max.x<e.min_x;
        }),own.end());
        other.erase(std::remove_if(other.begin(),other.end(),[&](auto i){
            return opposing[i].bounds.max.x<e.min_x;
        }),other.end());
        for(const auto j:other){
            const Record& v=e.vertex?mesh.vertices[e.index]:mesh.vertices[j];
            const Record& f=e.vertex?mesh.faces[j]:mesh.faces[e.index];
            if(valid_pair(v,f,result))
                result.pairs.push_back({Kind::VertexFace,v.id,f.id});
        }
        own.push_back(e.index);
    }
    normalize(result);
    return result;
}
inline Result exhaustive(const Mesh& m) {return exhaustive(build(m));}
inline Result sweep_and_prune(const Mesh& m) {
    return sweep_and_prune(build(m));
}
} // namespace kyoto::contact::features
