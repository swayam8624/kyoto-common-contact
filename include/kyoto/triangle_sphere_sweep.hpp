#pragma once
// Stage 6B: conservative continuous triangle/sphere *separation certificate*.
//
// Each triangle vertex moves linearly over the same normalized interval.
// The Hausdorff distance between two moving triangles cannot exceed
// max_i ||p_i(t)-p_i(s)||. Consequently, if at interval midpoint t_mid:
//
//  distance(center,T(t_mid)) - radius > 0.5*max_i||p_i(t1)-p_i(t0)||
//
// the triangle cannot touch the sphere anywhere in [t0,t1].
// This is a mathematical one-sided guarantee IN EXACT ARITHMETIC.
// Ordinary double arithmetic is not a machine-certified interval proof.
//
// This source does NOT apply contact impulses. The returned safe prefix
// may be used to limit *globally synchronized* motion later.
// Never label this narrow-phase culling as a complete contact solver.
#include "kyoto/sphere_surface_audit.hpp"
#include <array>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <stdexcept>

namespace kyoto::solid {
struct LinearTriangleMotion {
    std::array<V3,3> start{},end{};
};
struct TriangleSweepCertificate {
    bool entirely_separated{};
    bool sampled_overlap{};
    double certified_safe_prefix{};    // time fraction in [0,1]
    double first_unresolved_upper{1};  // end of first unresolved leaf
    std::size_t interval_tests{},pruned_intervals{};
    unsigned subdivisions{};
};

inline double triangle_distance_to_center(
    const std::array<V3,3>& vertices,V3 center) {
    return closest_triangle_point(center,vertices[0],vertices[1],
                                  vertices[2]).distance;
}
inline std::array<V3,3> lerp_vertices(
    const std::array<V3,3>& a,const std::array<V3,3>& b,double t) {
    std::array<V3,3> out;
    for(unsigned i=0;i<3;i++)out[i]=a[i]+(b[i]-a[i])*t;
    return out;
}
inline TriangleSweepCertificate certify_triangle_sphere_prefix(
    const LinearTriangleMotion& motion,const RigidSphere& sphere,
    unsigned max_depth=12) {
    validate(sphere);
    if(max_depth>24)throw std::invalid_argument("excessive depth");
    for(unsigned i=0;i<3;i++)
        if(!finite(motion.start[i])||!finite(motion.end[i]))
            throw std::invalid_argument("nonfinite triangle path");
    TriangleSweepCertificate result;
    result.entirely_separated=true;
    result.certified_safe_prefix=1;
    const auto descend=[&](auto&& self,
                           const std::array<V3,3>& a,
                           const std::array<V3,3>& b,
                           double t0,double t1,
                           unsigned remaining)->bool {
        ++result.interval_tests;
        const auto mid=lerp_vertices(a,b,.5);
        const double distance=triangle_distance_to_center(mid,sphere.center);
        double max_displacement=0;
        for(unsigned i=0;i<3;i++)
            max_displacement=std::max(max_displacement,norm(b[i]-a[i]));
        if(!std::isfinite(distance)||!std::isfinite(max_displacement))
            throw std::overflow_error("nonfinite triangle sweep");
        const double error_margin=1e-12*std::max({
            1.,sphere.radius,distance,max_displacement});
        // Separating guard deliberately exceeds numerical noise.
        const double guaranteed_lower=
            distance-.5*max_displacement-error_margin;
        if(guaranteed_lower>sphere.radius) {
            ++result.pruned_intervals;
            return true;
        }
        if(distance< sphere.radius-error_margin)
            result.sampled_overlap=true;
        if(remaining==0){
            result.entirely_separated=false;
            result.certified_safe_prefix=t0;
            result.first_unresolved_upper=t1;
            return false;
        }
        result.subdivisions++;
        if(!self(self,a,mid,t0,.5*(t0+t1),remaining-1))
            return false;
        return self(self,mid,b,.5*(t0+t1),t1,remaining-1);
    };
    const bool all_clear=descend(descend,motion.start,motion.end,0,1,max_depth);
    if(all_clear) {
        result.entirely_separated=true;
        result.certified_safe_prefix=1;
        result.first_unresolved_upper=1;
    }
    return result;
}
} // namespace kyoto::solid
