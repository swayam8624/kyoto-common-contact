#pragma once
// Stage 2: conservative broad-phase candidate refinement in shared time slabs.
// Every triangle vertex follows its DECLARED cubic Bezier path.
// This is NOT independent continuous collision detection nor a physics solver.
//
// Exact arithmetic theorem: splitting a Bezier path by de Casteljau keeps the
// same curve over two half-intervals. Each child control hull encloses that
// child trajectory. Every actual contact occurs in at least one synchronized
// time slab. Consequently union of per-slab candidates covers real contacts,
// assuming valid motion envelopes, topology and filter semantics.
//
// FP restrictions: IEEE binary64, FE_TONEAREST, no FTZ/DAZ, no fast-math.
// Every midpoint elementary operation has outward nextafter padding.
// General floating point/CCD soundness remains subject to independent audit.
#include "kyoto/motion_envelopes.hpp"
#include <algorithm>
#include <array>
#include <cfenv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

namespace kyoto::contact::temporal {
struct Interval { double lo{},hi{}; };
struct IntervalPoint { std::array<Interval,3> xyz{}; };
struct IntervalCurve { std::array<IntervalPoint,4> control{}; };
struct IntervalTriangle {
    std::uint64_t id{};
    std::array<IntervalCurve,3> vertices{};
    double thickness{};
    std::uint64_t category{1},mask{~std::uint64_t{0}};
};
inline double down(double x) {
    return std::nextafter(x,-std::numeric_limits<double>::infinity());
}
inline double up(double x) {
    return std::nextafter(x,std::numeric_limits<double>::infinity());
}
// Avoid (a+b)/2 overflow; outward-round every floating point operation.
inline Interval midpoint(Interval a,Interval b) {
    const double lo=down(down(.5*a.lo)+down(.5*b.lo));
    const double hi=up(up(.5*a.hi)+up(.5*b.hi));
    if(!std::isfinite(lo)||!std::isfinite(hi)||lo>hi)
        throw std::overflow_error("nonfinite interval midpoint");
    return {lo,hi};
}
inline IntervalPoint midpoint(const IntervalPoint& a,const IntervalPoint& b) {
    IntervalPoint r;
    for(std::size_t k=0;k<3;k++)r.xyz[k]=midpoint(a.xyz[k],b.xyz[k]);
    return r;
}
inline IntervalCurve initial(const CubicVertex& curve) {
    IntervalCurve c;
    for(std::size_t j=0;j<4;j++) {
        const auto p=curve.control[j];
        if(!finite(p))throw std::invalid_argument("nonfinite Bezier control");
        c.control[j].xyz={Interval{p.x,p.x},Interval{p.y,p.y},Interval{p.z,p.z}};
    }
    return c;
}
inline IntervalTriangle initial(const MovingTriangle& t) {
    if(!std::isfinite(t.thickness)||t.thickness<0)
        throw std::invalid_argument("invalid collision thickness");
    IntervalTriangle b;
    b.id=t.id;b.thickness=t.thickness;
    b.category=t.category;b.mask=t.mask;
    for(std::size_t k=0;k<3;k++)b.vertices[k]=initial(t.vertices[k]);
    return b;
}
inline std::pair<IntervalCurve,IntervalCurve> bisect(const IntervalCurve& p) {
    const auto q0=midpoint(p.control[0],p.control[1]);
    const auto q1=midpoint(p.control[1],p.control[2]);
    const auto q2=midpoint(p.control[2],p.control[3]);
    const auto r0=midpoint(q0,q1),r1=midpoint(q1,q2);
    const auto s=midpoint(r0,r1);
    return {IntervalCurve{{p.control[0],q0,r0,s}},
            IntervalCurve{{s,r1,q2,p.control[3]}}};
}
inline std::pair<IntervalTriangle,IntervalTriangle> bisect(
    const IntervalTriangle& t) {
    auto l=t,r=t;
    for(std::size_t k=0;k<3;k++){
        const auto pair=bisect(t.vertices[k]);
        l.vertices[k]=pair.first;r.vertices[k]=pair.second;
    }
    return {l,r};
}
inline Aabb conservative_box(const IntervalTriangle& t) {
    const double inf=std::numeric_limits<double>::infinity();
    std::array<double,3> lo{inf,inf,inf},hi{-inf,-inf,-inf};
    for(const auto& v:t.vertices)
        for(const auto& p:v.control)
            for(std::size_t k=0;k<3;k++){
                const auto z=p.xyz[k];
                if(!std::isfinite(z.lo)||!std::isfinite(z.hi)||z.lo>z.hi)
                    throw std::invalid_argument("invalid interval");
                lo[k]=std::min(lo[k],z.lo);
                hi[k]=std::max(hi[k],z.hi);
            }
    for(std::size_t k=0;k<3;k++){
        lo[k]=down(lo[k]-t.thickness);
        hi[k]=up(hi[k]+t.thickness);
    }
    const Aabb box{{lo[0],lo[1],lo[2]},{hi[0],hi[1],hi[2]}};
    if(!valid(box))throw std::overflow_error("inflated bounds overflow");
    return box;
}
inline std::vector<Primitive> as_slab_primitives(
    const std::vector<IntervalTriangle>& triangles) {
    std::vector<Primitive> out;out.reserve(triangles.size());
    for(const auto& t:triangles){
        const auto box=conservative_box(t);
        out.push_back({t.id,box,box,t.category,t.mask});
    }
    return out;
}
struct Query {
    std::vector<Pair> unique_pairs;
    std::size_t slab_occurrences{};
    std::size_t slabs{},boxes_built{};
};
// Full query: split EVERY feature into common synchronized slabs.
// Comparing unsynchronized slabs would be unsound for event exclusion.
inline Query query(const std::vector<MovingTriangle>& objects,
                   unsigned depth,bool exhaustive_per_slab=false) {
    if(depth>6)throw std::invalid_argument("depth limit 6");
    if(std::fegetround()!=FE_TONEAREST)
        throw std::invalid_argument("requires FE_TONEAREST");
    std::vector<IntervalTriangle> root;root.reserve(objects.size());
    for(const auto& t:objects)root.push_back(initial(t));
    std::vector<std::vector<IntervalTriangle>> slabs;
    slabs.emplace_back(std::move(root));
    for(unsigned level=0;level<depth;level++){
        std::vector<std::vector<IntervalTriangle>> next;
        next.reserve(slabs.size()*2);
        for(const auto& group:slabs){
            std::vector<IntervalTriangle> l,r;
            l.reserve(group.size());r.reserve(group.size());
            for(const auto& t:group){
                auto parts=bisect(t);
                l.push_back(std::move(parts.first));
                r.push_back(std::move(parts.second));
            }
            next.push_back(std::move(l));next.push_back(std::move(r));
        }
        slabs=std::move(next);
    }
    Query result;result.slabs=slabs.size();
    for(const auto& group:slabs){
        const auto input=as_slab_primitives(group);
        result.boxes_built+=input.size();
        auto pairs=exhaustive_per_slab?exhaustive(input):sweep_and_prune(input);
        result.slab_occurrences+=pairs.size();
        result.unique_pairs.insert(result.unique_pairs.end(),
                                   pairs.begin(),pairs.end());
    }
    std::sort(result.unique_pairs.begin(),result.unique_pairs.end());
    result.unique_pairs.erase(
        std::unique(result.unique_pairs.begin(),result.unique_pairs.end()),
        result.unique_pairs.end());
    return result;
}
struct SelectiveQuery {
    std::vector<Pair> unique_pairs;
    std::size_t coarse_candidates{};
    std::size_t synchronized_box_tests{};
    std::size_t split_pairs{};
};
// Conservative pair-level refinement: a pair can be rejected only if
// its two primitives are separated in EVERY matching-time leaf slab.
inline bool potentially_meets(const IntervalTriangle& a,
                              const IntervalTriangle& b,
                              unsigned depth,SelectiveQuery& stats) {
    ++stats.synchronized_box_tests;
    if(!overlaps(conservative_box(a),conservative_box(b)))return false;
    if(depth==0)return true;
    ++stats.split_pairs;
    const auto [al,ar]=bisect(a);
    const auto [bl,br]=bisect(b);
    return potentially_meets(al,bl,depth-1,stats) ||
           potentially_meets(ar,br,depth-1,stats);
}
inline SelectiveQuery selective_query(
    const std::vector<MovingTriangle>& objects,unsigned depth) {
    if(depth>6)throw std::invalid_argument("depth limit 6");
    if(std::fegetround()!=FE_TONEAREST)
        throw std::invalid_argument("requires FE_TONEAREST");
    // Coarse candidate generation uses the SAME public primitive API.
    const auto coarse=sweep_and_prune(as_primitives(objects));
    SelectiveQuery result;result.coarse_candidates=coarse.size();
    std::vector<IntervalTriangle> bounds;
    bounds.reserve(objects.size());
    std::vector<std::pair<std::uint64_t,std::size_t>> ids;
    ids.reserve(objects.size());
    for(std::size_t i=0;i<objects.size();i++){
        bounds.push_back(initial(objects[i]));
        ids.emplace_back(objects[i].id,i);
    }
    std::sort(ids.begin(),ids.end());
    for(const auto& pair:coarse){
        const auto ia=std::lower_bound(ids.begin(),ids.end(),pair.first,
            [](const auto& x,std::uint64_t y){return x.first<y;});
        const auto ib=std::lower_bound(ids.begin(),ids.end(),pair.second,
            [](const auto& x,std::uint64_t y){return x.first<y;});
        if(ia==ids.end()||ib==ids.end()||ia->first!=pair.first||
           ib->first!=pair.second)
            throw std::runtime_error("coarse candidate ID lost");
        if(potentially_meets(bounds[ia->second],bounds[ib->second],
                             depth,result))
            result.unique_pairs.push_back(pair);
    }
    return result;
}
} // namespace kyoto::contact::temporal
