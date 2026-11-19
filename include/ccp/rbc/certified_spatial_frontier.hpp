#pragma once
// Reach–Budget Calculus: an ACTUAL indexed, incremental, exact-current
// contact candidate engine. Distinct from Stage-0 quadratic proof oracle.
//
// Certificate: at the last individually verified reference configuration,
// object i owns reach r_i and six-face skin s_i. Its padded reference AABB
// is inflated by (r_i+s_i). If its current AABB has moved by <=s_i in
// every face coordinate, any current reach candidate (i,j) MUST appear
// among pairs of intersecting reference padded boxes.
//
// This implementation:
//   * builds reference potential pairs via X-axis sweep and complete Y/Z;
//   * maintains symmetric indexed incident sets for local dirty-item repair;
//   * repairs only changed objects, comparing them against all other
//     reference envelopes, OR globally rebuilds when many items are dirty;
//   * checks actual six-face displacement, so dishonest translation hints
//     cannot silently omit candidates;
//   * filters the cached pair frontier against exact CURRENT reach AABBs;
//   * accepts dynamic category/mask changes: masks only filter at output;
//   * refuses identity changes, NaNs, infinities and envelope overflow;
//   * reports *actual* rebuild, local repair and comparison costs.
//
// The local repair is O(D*N) (not a claimed optimal spatial index).
// Initial sweep is O(N log N + sweep overlaps), memory O(N+K_skin).
// Query is O(N+K_skin); K_skin can be O(N²) in all-overlap scenes.
// This is NOT CCD, collision response or proven solver acceleration.
#include "ccp/rbc/reach_budget.hpp"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <numeric>
#include <set>
#include <stdexcept>
#include <utility>
#include <vector>

namespace ccp::rbc {
struct SpatialQuery {
    Pairs pairs;
    std::size_t potential_pairs{};
    std::size_t local_repairs{};
    std::size_t dirty_primitives{};
    std::size_t build_comparisons{};
    std::size_t query_pair_checks{};
    std::size_t global_build_count{};
    std::size_t local_repair_count{};
    bool global_rebuilt{};
};

class CertifiedSpatialFrontier {
public:
    void initialize(const std::vector<Bounds>& input,
                    const std::vector<Reach>& reach) {
        validate(input,reach);
        reach_=reach;
        reference_=input;
        buildAll();
        anchor_={};
        initialized_=true;
    }
    void reset() noexcept {
        reference_.clear();
        reach_.clear();
        envelopes_.clear();
        adjacency_.clear();
        potential_count_=0;
        total_global_builds_=0;
        total_local_repairs_=0;
        anchor_={};
        initialized_=false;
    }
    [[nodiscard]] bool initialized() const noexcept {return initialized_;}
    [[nodiscard]] std::size_t global_builds() const noexcept {
        return total_global_builds_;
    }
    [[nodiscard]] std::size_t local_repairs() const noexcept {
        return total_local_repairs_;
    }
    [[nodiscard]] std::size_t potential_pairs() const noexcept {
        return potential_count_;
    }

    [[nodiscard]] SpatialQuery query(const std::vector<Bounds>& current) {
        return query_translated(current,{});
    }
    [[nodiscard]] SpatialQuery query_translated(
        const std::vector<Bounds>& current, ccp::live::Vec3 cumulativeShift) {
        if(!initialized_)
            throw std::logic_error("RBC spatial frontier requires initialize()");
        validate(current,reach_);
        const double shift[]{cumulativeShift.x,cumulativeShift.y,cumulativeShift.z};
        for(double x:shift)
            if(!std::isfinite(x))
                throw std::invalid_argument("RBC spatial translation nonfinite");
        std::vector<Bounds> normalized;
        normalized.reserve(current.size());
        std::vector<std::size_t> dirty;
        dirty.reserve(current.size());
        for(std::size_t i=0;i<current.size();++i) {
            if(current[i].id!=reference_[i].id)
                throw std::invalid_argument(
                    "RBC spatial primitive order/identity changed");
            Bounds n=current[i];
            // A reference-space transform removes only a COMMON
            // translation, not object-specific relative motion.
            n.min.x-=cumulativeShift.x; n.max.x-=cumulativeShift.x;
            n.min.y-=cumulativeShift.y; n.max.y-=cumulativeShift.y;
            n.min.z-=cumulativeShift.z; n.max.z-=cumulativeShift.z;
            const double v[]{n.min.x,n.min.y,n.min.z,n.max.x,n.max.y,n.max.z};
            for(double x:v)
                if(!std::isfinite(x))
                    throw std::overflow_error(
                        "RBC spatial reference transform overflow");
            if(face_motion(reference_[i],n)>reach_[i].skin)
                dirty.push_back(i);
            normalized.push_back(n);
        }
        SpatialQuery result{};
        result.dirty_primitives=dirty.size();
        // Fail-safe all-dirty: avoid D*N local scanning if a whole
        // scene teleports. Threshold is an engineering heuristic,
        // not a frozen V4 or publication claim.
        if(dirty.size()>=std::max<std::size_t>(8,current.size()/4)
           && !dirty.empty()) {
            reference_=std::move(normalized);
            result.build_comparisons=buildAll();
            result.global_rebuilt=true;
        } else {
            // Updating one dirty vertex only invalidates the potential
            // edges incident to it. Every edge touching a second dirty
            // vertex is reevaluated when that vertex is repaired.
            for(std::size_t i:dirty) {
                result.build_comparisons+=repairOne(i,normalized[i]);
                ++result.local_repairs;
            }
        }
        anchor_=cumulativeShift;
        // Iterate symmetric potential neighbor lists once. Reference
        // envelopes DO NOT use compatibility filtering: current masks
        // are applied here, so masks can change while certificates live.
        result.pairs.reserve(potential_count_);
        for(std::size_t i=0;i<adjacency_.size();++i)
            for(std::size_t j:adjacency_[i]) {
                if(j<=i) continue;
                ++result.query_pair_checks;
                if(!compatible(current[i],current[j])) continue;
                if(linf_box_gap(current[i],current[j])<=
                    reach_[i].radius+reach_[j].radius)
                    result.pairs.push_back({
                        std::min(current[i].id,current[j].id),
                        std::max(current[i].id,current[j].id)});
            }
        std::sort(result.pairs.begin(),result.pairs.end());
        if(std::adjacent_find(result.pairs.begin(),result.pairs.end())!=
           result.pairs.end())
            throw std::logic_error("RBC spatial duplicate candidate IDs");
        result.potential_pairs=potential_count_;
        result.global_build_count=total_global_builds_;
        result.local_repair_count=total_local_repairs_;
        return result;
    }
private:
    // Each reference box is expanded by ITS OWN radius+skin. Pairwise
    // overlap is conservative for all current reach-AABB candidates.
    [[nodiscard]] static Bounds envelope(const Bounds& b,const Reach& reach) {
        Bounds e=b;
        const double padding=reach.radius+reach.skin;
        if(!std::isfinite(padding))
            throw std::overflow_error("RBC spatial radius+skin overflow");
        auto lower=[&](double x) {
            const double value=x-padding;
            if(!std::isfinite(value))
                throw std::overflow_error("RBC spatial lower face overflow");
            // Directed outward rounding preserves boundary inclusion.
            const double outside=std::nextafter(value,
                -std::numeric_limits<double>::infinity());
            if(!std::isfinite(outside))
                throw std::overflow_error("RBC spatial lower face not finite");
            return outside;
        };
        auto upper=[&](double x) {
            const double value=x+padding;
            if(!std::isfinite(value))
                throw std::overflow_error("RBC spatial upper face overflow");
            const double outside=std::nextafter(value,
                std::numeric_limits<double>::infinity());
            if(!std::isfinite(outside))
                throw std::overflow_error("RBC spatial upper face not finite");
            return outside;
        };
        e.min.x=lower(b.min.x); e.min.y=lower(b.min.y); e.min.z=lower(b.min.z);
        e.max.x=upper(b.max.x); e.max.y=upper(b.max.y); e.max.z=upper(b.max.z);
        return e;
    }
    [[nodiscard]] static bool overlap(const Bounds& a,const Bounds& b) {
        return a.min.x<=b.max.x&&b.min.x<=a.max.x&&
               a.min.y<=b.max.y&&b.min.y<=a.max.y&&
               a.min.z<=b.max.z&&b.min.z<=a.max.z;
    }
    void link(std::size_t i,std::size_t j) {
        if(i==j) return;
        if(adjacency_[i].insert(j).second) {
            adjacency_[j].insert(i);
            ++potential_count_;
        }
    }
    std::size_t buildAll() {
        std::vector<Bounds> envelopes;
        envelopes.reserve(reference_.size());
        for(std::size_t i=0;i<reference_.size();++i)
            envelopes.push_back(envelope(reference_[i],reach_[i]));
        // Only publish after all envelope construction succeeded.
        envelopes_=std::move(envelopes);
        adjacency_.assign(reference_.size(),{});
        potential_count_=0;
        std::vector<std::size_t> order(reference_.size());
        std::iota(order.begin(),order.end(),std::size_t{0});
        std::sort(order.begin(),order.end(),[&](std::size_t a,std::size_t b){
            if(envelopes_[a].min.x!=envelopes_[b].min.x)
                return envelopes_[a].min.x<envelopes_[b].min.x;
            return reference_[a].id<reference_[b].id;
        });
        std::vector<std::size_t> active;
        std::size_t tests=0;
        for(std::size_t i:order) {
            const Bounds& a=envelopes_[i];
            active.erase(std::remove_if(active.begin(),active.end(),
               [&](std::size_t j){return envelopes_[j].max.x<a.min.x;}),
               active.end());
            for(std::size_t j:active) {
                ++tests;
                if(overlap(a,envelopes_[j])) link(i,j);
            }
            active.push_back(i);
        }
        ++total_global_builds_;
        return tests;
    }
    std::size_t repairOne(std::size_t i,const Bounds& newReference) {
        // Stage the new envelope so overflow cannot partially delete edges.
        const Bounds updated=envelope(newReference,reach_[i]);
        for(std::size_t j:adjacency_[i])
            adjacency_[j].erase(i);
        potential_count_-=adjacency_[i].size();
        adjacency_[i].clear();
        reference_[i]=newReference;
        envelopes_[i]=updated;
        std::size_t tests=0;
        for(std::size_t j=0;j<envelopes_.size();++j) {
            if(j==i) continue;
            ++tests;
            if(overlap(updated,envelopes_[j])) link(i,j);
        }
        ++total_local_repairs_;
        return tests;
    }
    std::vector<Bounds> reference_;
    std::vector<Reach> reach_;
    std::vector<Bounds> envelopes_;
    std::vector<std::set<std::size_t>> adjacency_;
    std::size_t potential_count_{};
    std::size_t total_global_builds_{};
    std::size_t total_local_repairs_{};
    ccp::live::Vec3 anchor_{};
    bool initialized_{};
};
} // namespace ccp::rbc
