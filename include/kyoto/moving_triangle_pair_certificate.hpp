#pragma once
// Pass 22A: conservative continuous SEPARATION certificate for two
// linearly moving finite 3D triangular surfaces.
//
// The earlier finite-mesh source checks one moving triangle against a
// STATIC triangle. Coupled deformable/deformable contact requires both
// triangle surfaces to move over the *same accepted physical timestep*.
//
// This routine only proves absence of contact when entirely_separated.
// "Unresolved" NEVER means collision or safe passage. It requests
// smaller time integration or an independent robust IPC narrowphase.
//
// Mathematical invariant (exact real arithmetic):
//   d(A(t),B(t)) >= d(A(mid),B(mid))
//                  - .5*(max_i ||a_i(t1)-a_i(t0)||
//                      + max_j ||b_j(t1)-b_j(t0)||)
// for any t in [t0,t1] and affinely moving triangle vertices.
// This is a Hausdorff-distance Lipschitz enclosure for compact sets.
// A conservative scale-dependent floating guard is included, but this
// routine is NOT a machine-directed-rounded universally certified CCD.
//
// Every call is a pure geometric check; no physical positions,
// impulses or solver time are changed. No broadphase candidate pruning
// may use a stale result from a different source interval.
#include "kyoto/triangle_mesh_contact.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <stdexcept>

namespace kyoto::solid {

inline MeshSweepCertificate certify_two_moving_triangles(
    const LinearTriangleMotion& a,
    const LinearTriangleMotion& b,
    double clearance=0.,
    unsigned maximum_depth=12) {
    if(!(std::isfinite(clearance)&&clearance>=0) ||
       maximum_depth>20)
        throw std::invalid_argument("invalid moving-moving CCD certificate parameters");
    for(const auto& p:a.start)
        if(!finite(p))throw std::invalid_argument("nonfinite first triangle start");
    for(const auto& p:a.end)
        if(!finite(p))throw std::invalid_argument("nonfinite first triangle end");
    for(const auto& p:b.start)
        if(!finite(p))throw std::invalid_argument("nonfinite second triangle start");
    for(const auto& p:b.end)
        if(!finite(p))throw std::invalid_argument("nonfinite second triangle end");

    MeshSweepCertificate out;
    const auto recurse=[&](auto&& self,double t0,double t1,
                           unsigned depth)->bool {
        ++out.nodes;
        const auto a0=lerp_vertices(a.start,a.end,t0);
        const auto a1=lerp_vertices(a.start,a.end,t1);
        const auto b0=lerp_vertices(b.start,b.end,t0);
        const auto b1=lerp_vertices(b.start,b.end,t1);

        // If complete time-swept AABBs are separated, the swept
        // triangles are disjoint independently of triangle shape.
        const auto first=swept_bounding({a0,a1});
        const auto second=swept_bounding({b0,b1});
        constexpr double numerical_guard=1e-12;
        if(!maybe_near(first,second,clearance+numerical_guard)) {
            ++out.pruned;
            return true;
        }
        const auto amid=lerp_vertices(a0,a1,.5);
        const auto bmid=lerp_vertices(b0,b1,.5);
        const double d=triangle_triangle_closest(amid,bmid).distance;
        double ma=0.,mb=0.;
        for(int j=0;j<3;++j){
            ma=std::max(ma,norm(a1[j]-a0[j]));
            mb=std::max(mb,norm(b1[j]-b0[j]));
        }
        const double combined=ma+mb;
        if(!(std::isfinite(d)&&std::isfinite(combined)))
            throw std::runtime_error("nonfinite relative swept triangle motion");
        // Excess guard errs toward unresolved, never toward safe.
        const double scale=std::max({1.,clearance,d,combined});
        const double guard=8e-11*scale;
        if(d-.5*combined-guard>clearance){
            ++out.pruned;
            return true;
        }
        if(d<=clearance+guard)++out.sampled_possible_contacts;
        if(depth==maximum_depth){
            out.entirely_separated=false;
            out.safe_prefix=t0;
            out.first_unresolved_upper=t1;
            return false;
        }
        const double mid=.5*(t0+t1);
        return self(self,t0,mid,depth+1) &&
               self(self,mid,t1,depth+1);
    };
    recurse(recurse,0.,1.,0);
    return out;
}

} // namespace kyoto::solid
