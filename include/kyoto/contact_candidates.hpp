#pragma once
// STAGE 0 ONLY: swept-AABB candidate generation for linearly moving features.
// These are not true contact events, not CCD, and not a physics solver.
// Endpoint hulls contain all intermediate boxes only when underlying feature
// vertices move linearly over this interval; otherwise require certified
// full-path enclosures from the solver.
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numeric>
#include <stdexcept>
#include <utility>
#include <vector>

namespace kyoto::contact {
struct Vec3 { double x{}, y{}, z{}; };
struct Aabb { Vec3 min{}, max{}; };
struct Primitive {
    std::uint64_t id{};
    Aabb begin{}, end{};
    std::uint64_t category{1}, mask{~std::uint64_t{0}};
};
using Pair = std::pair<std::uint64_t, std::uint64_t>;

inline bool valid(const Aabb& b) {
    return std::isfinite(b.min.x) && std::isfinite(b.min.y) &&
        std::isfinite(b.min.z) && std::isfinite(b.max.x) &&
        std::isfinite(b.max.y) && std::isfinite(b.max.z) &&
        b.min.x<=b.max.x && b.min.y<=b.max.y && b.min.z<=b.max.z;
}
inline Aabb swept(const Primitive& p) {
    if(!valid(p.begin)||!valid(p.end))
        throw std::invalid_argument("non-finite or inverted box");
    return {{std::min(p.begin.min.x,p.end.min.x),
             std::min(p.begin.min.y,p.end.min.y),
             std::min(p.begin.min.z,p.end.min.z)},
            {std::max(p.begin.max.x,p.end.max.x),
             std::max(p.begin.max.y,p.end.max.y),
             std::max(p.begin.max.z,p.end.max.z)}};
}
inline bool overlaps(const Aabb& a,const Aabb& b) {
    // Touching is included. Actual contact requires narrow-phase CCD.
    return a.min.x<=b.max.x && b.min.x<=a.max.x &&
           a.min.y<=b.max.y && b.min.y<=a.max.y &&
           a.min.z<=b.max.z && b.min.z<=a.max.z;
}
inline bool allowed(const Primitive& a,const Primitive& b) {
    // Masks are NOT a substitute for mesh adjacency/topology semantics.
    return (a.category&b.mask)!=0 && (b.category&a.mask)!=0;
}
inline void check_ids(const std::vector<Primitive>& p) {
    std::vector<std::uint64_t> ids;
    ids.reserve(p.size());
    for(const auto& x:p) ids.push_back(x.id);
    std::sort(ids.begin(),ids.end());
    if(std::adjacent_find(ids.begin(),ids.end())!=ids.end())
        throw std::invalid_argument("duplicate primitive id");
}
inline std::vector<Pair> exhaustive(const std::vector<Primitive>& p) {
    check_ids(p);
    std::vector<Aabb> b;
    b.reserve(p.size());
    for(const auto& x:p) b.push_back(swept(x));
    std::vector<Pair> pairs;
    for(std::size_t i=0;i<p.size();++i)
        for(std::size_t j=i+1;j<p.size();++j)
            if(allowed(p[i],p[j])&&overlaps(b[i],b[j]))
                pairs.emplace_back(std::min(p[i].id,p[j].id),
                                   std::max(p[i].id,p[j].id));
    std::sort(pairs.begin(),pairs.end());
    return pairs;
}
inline std::vector<Pair> sweep_and_prune(const std::vector<Primitive>& p) {
    check_ids(p);
    std::vector<Aabb> b;
    b.reserve(p.size());
    for(const auto& x:p) b.push_back(swept(x));
    std::vector<std::size_t> order(p.size());
    std::iota(order.begin(),order.end(),std::size_t{0});
    std::sort(order.begin(),order.end(),[&](auto a,auto c) {
        if(b[a].min.x != b[c].min.x) return b[a].min.x<b[c].min.x;
        return p[a].id<p[c].id;
    });
    std::vector<std::size_t> active;
    std::vector<Pair> pairs;
    for(const auto i:order) {
        const double x=b[i].min.x;
        active.erase(std::remove_if(active.begin(),active.end(),
          [&](auto j){return b[j].max.x<x;}),active.end());
        for(const auto j:active)
            if(allowed(p[i],p[j]) && overlaps(b[i],b[j]))
                pairs.emplace_back(std::min(p[i].id,p[j].id),
                                   std::max(p[i].id,p[j].id));
        active.push_back(i);
    }
    std::sort(pairs.begin(),pairs.end());
    return pairs;
}
} // namespace kyoto::contact
