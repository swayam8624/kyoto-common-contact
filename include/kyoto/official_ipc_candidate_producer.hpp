#pragma once
// Pass 35A: official pinned IPC Toolkit LBVH as a candidate producer
// inside the native typed FEM solver. We give IPC the exact same
// outward-enclosed vertex boxes as SCA, NOT a separately rounded
// inflation rule. The public adapter compares the normalized result
// against exhaustive truth before it can affect physical dynamics.
// No timing or external physical integrator claim is made.
#include "kyoto/linear_candidate_adapter.hpp"
#include "kyoto/external_candidate_filter.hpp"
#include <ipc/broad_phase/lbvh.hpp>
#include <ipc/broad_phase/aabb.hpp>
#include <ipc/candidates/edge_edge.hpp>
#include <ipc/candidates/face_vertex.hpp>
#include <Eigen/Core>
#include <limits>
namespace kyoto::contact::atlas {
inline f::Result official_ipc_candidate_producer(const f::Mesh& mesh){
    const auto built=f::build(mesh);
    if(mesh.vertices.size()>std::size_t(std::numeric_limits<int>::max())||
       mesh.edges.size()>std::size_t(std::numeric_limits<int>::max())||
       mesh.faces.size()>std::size_t(std::numeric_limits<int>::max()))
        throw std::overflow_error("IPC dense index range exceeded");
    ipc::AABBs boxes;boxes.reserve(built.vertices.size());
    Eigen::MatrixXi edges(mesh.edges.size(),2),faces(mesh.faces.size(),3);
    for(std::size_t i=0;i<built.vertices.size();++i){
        if(mesh.vertices[i].id!=i)throw std::invalid_argument("IPC adapter requires dense vertex IDs");
        const auto b=built.vertices[i].bounds;
        ipc::AABB box(Eigen::Array3d(b.min.x,b.min.y,b.min.z),
                      Eigen::Array3d(b.max.x,b.max.y,b.max.z));
        box.vertex_ids={static_cast<ipc::index_t>(i),-1,-1};
        boxes.push_back(box);
    }
    for(std::size_t i=0;i<mesh.edges.size();++i){
        if(mesh.edges[i].id!=i)throw std::invalid_argument("IPC adapter requires dense edge IDs");
        for(int k=0;k<2;++k)edges(i,k)=int(mesh.edges[i].vertices[k]);
    }
    for(std::size_t i=0;i<mesh.faces.size();++i){
        if(mesh.faces[i].id!=i)throw std::invalid_argument("IPC adapter requires dense face IDs");
        for(int k=0;k<3;++k)faces(i,k)=int(mesh.faces[i].vertices[k]);
    }
    if(boxes.empty())return {};
    ipc::LBVH bp;bp.can_vertices_collide=[](auto,auto){return true;};
    bp.build(boxes,edges,faces,static_cast<std::uint8_t>(3));
    std::vector<ipc::FaceVertexCandidate> vf;
    std::vector<ipc::EdgeEdgeCandidate> ee;
    bp.detect_face_vertex_candidates(vf);bp.detect_edge_edge_candidates(ee);
    std::vector<f::Candidate> raw;
    for(const auto& p:vf)raw.push_back({f::Kind::VertexFace,
        std::uint64_t(p.vertex_id),std::uint64_t(p.face_id)});
    for(const auto& p:ee)raw.push_back({f::Kind::EdgeEdge,
        std::uint64_t(p.edge0_id),std::uint64_t(p.edge1_id)});
    // The pinned upstream LBVH outward-expands double boxes into float
    // nodes. Remove extra conservative hits using the original binary64
    // feature contract; do not manufacture a missing candidate.
    // The adapter's independent exhaustive check remains mandatory.
    // Counts here describe post-filter work, not upstream traversal.
    return f::filter_dense_external_candidates(built,raw);
}
} // namespace kyoto::contact::atlas
