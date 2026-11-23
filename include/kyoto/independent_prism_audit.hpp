#pragma once
// PASS 9 / independent continuous geometric audit, NOT a solver narrow phase.
//
// A moving triangle under LINEAR vertex interpolation over [a,b] is
// contained inside the convex hull of its SIX endpoint positions.
// Therefore, if we find ANY plane n·(x-c) > r||n|| separating ALL six
// points from the sphere, the complete swept triangle is separated.
//
// This is independent of Pass 6B's midpoint-Lipschitz distance
// certificate: no triangle/sphere closest-point routine is used and
// no midpoint distance drives the decision. We search for a SUPPORT
// DIRECTION using a fixed-budget Gilbert/Frank-Wolfe iteration over the
// six-point hull, and verify each candidate by six explicit projections.
//
// The returned "clear" value is a one-sided separation proof in exact
// arithmetic. Failure to find a plane is UNRESOLVED, NOT a collision.
// IEEE 754 dot/norm here use ordinary binary64 with an ample
// scale-aware guard; this is NOT an MPFR/directed-rounded proof.
// Large coordinates can make the guard deliberately overly conservative.

#include "kyoto/triangle_sphere_sweep.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <stdexcept>

namespace kyoto::solid {
struct IndependentPrismReport {
    bool separated{};
    double separating_gap_m{}; // positive only for a found witness
    std::size_t support_planes_tested{};
    std::size_t time_nodes{};
    std::size_t safely_pruned_nodes{};
    std::size_t unresolved_terminal_nodes{};
    unsigned deepest_level{};
};

inline bool independent_six_endpoint_separating_plane(
    const std::array<V3,6>& p,const RigidSphere& sphere,
    double& measured_gap,std::size_t& tested) {
    validate(sphere);
    std::array<V3,6> v;
    double scale=std::max(1.,sphere.radius);
    for(int i=0;i<6;i++){
        if(!finite(p[i]))throw std::invalid_argument("nonfinite swept prism");
        v[i]=p[i]-sphere.center;
        scale=std::max(scale,norm(v[i]));
    }
    // Scale-aware inward conservatism: avoid pretending near-grazing
    // binary64 differences have proved machine-precision clearance.
    const double slack=4e-11*scale;
    bool found=false;
    double best=-std::numeric_limits<double>::infinity();
    const auto test=[&](V3 direction) {
        const double len=norm(direction);
        if(!(std::isfinite(len)&&len>1e-14*scale))return;
        ++tested;
        double smallest=std::numeric_limits<double>::infinity();
        for(const auto& q:v)
            smallest=std::min(smallest,dot(direction,q));
        const double gap=smallest/len-sphere.radius;
        if(gap>slack) {
            found=true;
            best=std::max(best,gap);
        }
    };
    // Axis-aligned, point-radial and face-normal support directions
    // are mathematically distinct from a moving triangle distance.
    for(const auto& n:std::array<V3,6>{
         V3{1,0,0},V3{-1,0,0},V3{0,1,0},V3{0,-1,0},
         V3{0,0,1},V3{0,0,-1}})test(n);
    for(const auto& q:v)test(q);
    for(int side=0;side<2;side++){
        const int i=3*side;
        const V3 n=cross(v[i+1]-v[i],v[i+2]-v[i]);
        test(n);test(-n);
    }
    // Independent convex-hull support search, using no triangle
    // nearest-point primitive from the actual solver.
    int nearest=0;
    for(int j=1;j<6;j++)
        if(dot(v[j],v[j])<dot(v[nearest],v[nearest]))nearest=j;
    V3 q=v[nearest];
    for(int iter=0;iter<64;iter++){
        test(q);
        if(found)break;
        int extreme=0;
        for(int j=1;j<6;j++)
            if(dot(q,v[j])<dot(q,v[extreme]))extreme=j;
        const V3 delta=q-v[extreme];
        const double denom=dot(delta,delta);
        if(!(denom>1e-28*scale*scale))break;
        const double rate=std::clamp(dot(q,delta)/denom,0.,1.);
        if(rate<1e-16)break;
        q=q-delta*rate;
    }
    measured_gap=found?best:0.0;
    return found;
}

inline IndependentPrismReport audit_triangle_sphere_prism(
    const LinearTriangleMotion& path,const RigidSphere& sphere,
    unsigned depth_limit=13) {
    validate(sphere);
    if(depth_limit>22)
        throw std::invalid_argument("unbounded independent time audit");
    IndependentPrismReport report;
    report.separated=true;
    report.separating_gap_m=std::numeric_limits<double>::infinity();
    const auto search=[&](auto&& self,
                          const std::array<V3,3>& a,
                          const std::array<V3,3>& b,
                          unsigned level)->void {
        ++report.time_nodes;
        report.deepest_level=std::max(report.deepest_level,level);
        const std::array<V3,6> corners={
            a[0],a[1],a[2],b[0],b[1],b[2]};
        double witness=0;
        if(independent_six_endpoint_separating_plane(
            corners,sphere,witness,report.support_planes_tested)) {
            report.safely_pruned_nodes++;
            report.separating_gap_m=
                std::min(report.separating_gap_m,witness);
            return;
        }
        if(level==depth_limit){
            report.separated=false;
            report.unresolved_terminal_nodes++;
            return;
        }
        const auto midpoint=lerp_vertices(a,b,.5);
        self(self,a,midpoint,level+1);
        self(self,midpoint,b,level+1);
    };
    search(search,path.start,path.end,0);
    if(!report.separated)report.separating_gap_m=0;
    return report;
}
} // namespace kyoto::solid
