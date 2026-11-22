#pragma once
// Stage 1: conservative swept envelopes for cubic Bezier vertex paths.
// Each Bezier curve lies inside the convex hull of its four control points.
// The hull encloses entire trajectories, including intermediate excursions
// that endpoint-only boxes can miss. This is candidate generation, NOT CCD.
//
// FP caveat: outer nextafter covers rounding in thickness arithmetic at
// this boundary, but this header is not a fully verified interval-arithmetic
// library. Generality across arbitrary FP environments is NOT asserted.
#include "kyoto/contact_candidates.hpp"
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <vector>

namespace kyoto::contact {
struct CubicVertex {
    std::array<Vec3, 4> control{};
};

struct MovingTriangle {
    std::uint64_t id{};
    std::array<CubicVertex, 3> vertices{};
    double thickness{0.0};
    std::uint64_t category{1};
    std::uint64_t mask{~std::uint64_t{0}};
};

struct MotionEnvelope {
    Aabb complete{};      // Conservative for the whole cubic step.
    Aabb endpoint_only{}; // Diagnostic counterexample, unsafe for cubics.
};

inline bool finite(const Vec3& x) {
    return std::isfinite(x.x) && std::isfinite(x.y) && std::isfinite(x.z);
}

inline Vec3 affine(const Vec3& a, const Vec3& b, double t) {
    const double u = 1.0 - t;
    return {u*a.x+t*b.x, u*a.y+t*b.y, u*a.z+t*b.z};
}

// A sampled position on the declared trajectory, NOT a CCD event test.
inline Vec3 evaluate(const CubicVertex& p, double t) {
    if (!(std::isfinite(t) && t >= 0 && t <= 1))
        throw std::invalid_argument("time outside [0,1]");
    if (t == 0.0) return p.control.front();
    if (t == 1.0) return p.control.back();
    const Vec3 a = affine(p.control[0], p.control[1], t);
    const Vec3 b = affine(p.control[1], p.control[2], t);
    const Vec3 c = affine(p.control[2], p.control[3], t);
    return affine(affine(a, b, t), affine(b, c, t), t);
}

inline MotionEnvelope envelope(const MovingTriangle& tri) {
    if (!(std::isfinite(tri.thickness) && tri.thickness >= 0))
        throw std::invalid_argument("invalid thickness");
    const double inf = std::numeric_limits<double>::infinity();
    Aabb all{{inf,inf,inf},{-inf,-inf,-inf}};
    Aabb endpoints = all;
    auto include = [](Aabb& b, const Vec3& p) {
        b.min.x=std::min(b.min.x,p.x); b.min.y=std::min(b.min.y,p.y);
        b.min.z=std::min(b.min.z,p.z); b.max.x=std::max(b.max.x,p.x);
        b.max.y=std::max(b.max.y,p.y); b.max.z=std::max(b.max.z,p.z);
    };
    for (const auto& vertex : tri.vertices) {
        for (const auto& p : vertex.control) {
            if (!finite(p)) throw std::invalid_argument("nonfinite control");
            include(all,p);
        }
        include(endpoints,vertex.control.front());
        include(endpoints,vertex.control.back());
    }
    const auto inflate=[&](Aabb& b) {
        b.min.x=std::nextafter(b.min.x-tri.thickness,-inf);
        b.min.y=std::nextafter(b.min.y-tri.thickness,-inf);
        b.min.z=std::nextafter(b.min.z-tri.thickness,-inf);
        b.max.x=std::nextafter(b.max.x+tri.thickness,+inf);
        b.max.y=std::nextafter(b.max.y+tri.thickness,+inf);
        b.max.z=std::nextafter(b.max.z+tri.thickness,+inf);
        if (!valid(b)) throw std::overflow_error("nonfinite inflated box");
    };
    inflate(all);
    inflate(endpoints);
    return {all,endpoints};
}

inline std::vector<Primitive> as_primitives(
    const std::vector<MovingTriangle>& triangles,
    bool endpoint_diagnostic=false) {
    std::vector<Primitive> out;
    out.reserve(triangles.size());
    for (const auto& triangle:triangles) {
        const auto b=envelope(triangle);
        const auto& box=endpoint_diagnostic?b.endpoint_only:b.complete;
        // The full envelope is reused in both slots intentionally:
        // it already bounds the entire step, not just its endpoints.
        out.push_back({triangle.id,box,box,triangle.category,triangle.mask});
    }
    return out;
}
} // namespace kyoto::contact
