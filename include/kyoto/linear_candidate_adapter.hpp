#pragma once
// Pass 27: solver-facing linear VF/EE candidate boundary.
// This deliberately has no dependency on Eigen or an external solver.
// Dense indices correspond to the solver's CURRENT vertex/edge/face rows.
// Only the universal-collision, nonincident VF/EE contract is supported.
// A change of row order/connectivity/epoch invalidates the retained BVH.
// No physical response, nonlinear CCD, or additional filters are implied.
#include "kyoto/refit_feature_hierarchy.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <vector>

namespace kyoto::contact::atlas {
using IndexedEdge = std::array<std::size_t,2>;
using IndexedFace = std::array<std::size_t,3>;

// External candidate producers receive the same validated universal
// collision mesh, including outward spatial padding. This optional
// hook is deliberately audit-only: a backend must match exhaustive
// canonical candidates before any result may reach a physical solver.
using ExternalCandidateProducer = f::Result (*)(const f::Mesh&);

class LinearCandidateAdapter {
    // Retained full-refit baseline; selective or frontier optimizations
    // should be compared separately, not silently change this boundary.
    RefitHierarchy hierarchy_;
public:
    std::size_t rebuilds() const { return hierarchy_.rebuilds(); }
    std::size_t refits() const { return hierarchy_.refits(); }

    f::Result query(std::span<const Vec3> start,
                    std::span<const Vec3> end,
                    std::span<const IndexedEdge> edges,
                    std::span<const IndexedFace> faces,
                    std::uint64_t epoch=1,
                    double per_vertex_padding=0.0,
                    bool exhaustive_reference=false,
                    ExternalCandidateProducer external=nullptr) {
        if(start.size()!=end.size() || epoch==0 ||
           !std::isfinite(per_vertex_padding) || per_vertex_padding<0)
            throw std::invalid_argument("invalid indexed linear motion");
        f::Mesh mesh;
        mesh.topology_epoch=epoch;
        mesh.thickness=per_vertex_padding;
        mesh.vertices.reserve(start.size());
        mesh.edges.reserve(edges.size());
        mesh.faces.reserve(faces.size());
        for(std::size_t i=0;i<start.size();++i){
            if(!finite(start[i])||!finite(end[i]))
                throw std::invalid_argument("nonfinite indexed motion");
            // These repeated endpoints encode the same spatial hull as
            // the straight segment. They are ONLY envelope controls:
            // the external CCD receives the ORIGINAL linear endpoints,
            // never this cubic's nonlinear time parameterization.
            mesh.vertices.push_back({std::uint64_t(i),
                CubicVertex{{start[i],start[i],end[i],end[i]}}});
        }
        for(std::size_t i=0;i<edges.size();++i){
            const auto [a,b]=edges[i];
            if(a>=start.size()||b>=start.size()||a==b)
                throw std::invalid_argument("invalid indexed edge");
            mesh.edges.push_back({std::uint64_t(i),{a,b}});
        }
        for(std::size_t i=0;i<faces.size();++i){
            const auto [a,b,c]=faces[i];
            if(a>=start.size()||b>=start.size()||c>=start.size()||
               a==b||a==c||b==c)
                throw std::invalid_argument("invalid indexed face");
            mesh.faces.push_back({std::uint64_t(i),{a,b,c}});
        }
        // Query state belongs to this adapter alone. If any exception
        // occurs during a hierarchy mutation, discard its caches so
        // the next valid call cannot observe a partly updated tree.
        if(external){
            const auto built=f::build(mesh);
            auto result=external(mesh);
            f::normalize(result);
            const auto truth=f::exhaustive(built);
            if(result.pairs!=truth.pairs)
                throw std::runtime_error("external candidate producer differs from exhaustive contract");
            return result;
        }
        if(exhaustive_reference)return f::exhaustive(mesh);
        try { return hierarchy_.query(mesh); }
        catch(...) { hierarchy_=RefitHierarchy{}; throw; }
    }
};
} // namespace kyoto::contact::atlas
