#pragma once
// RBC Stage 2B: flat contiguous certified interaction frontier.
//
// This is a REPLACEMENT CANDIDATE, not an overwrite of the Stage-2A
// std::set-based indexed engine. The original negative 54-scene Stage-2A
// result remains in Git history and is deliberately still executable.
//
// Current exact certificate:
//  - fixed reach r_i, skin s_i per primitive;
//  - input IDs and their order remain stable until explicit reset;
//  - each current six-face motion residual after a SHARED supplied
//    translation is bounded by its local skin;
//  - potential pairs are all overlapping reference (radius+skin)
//    envelopes, with masks excluded only at current filtering.
//
// Flat design:
//  - initial and bulk build use actual sweep-and-prune;
//  - incremental repair removes edges touching dirty primitives in ONE
//    compact linear pass and recreates dirty incident edges in a BATCH;
//  - no heap-allocated std::set node per potential edge;
//  - dynamic masks allowed without missing newly eligible candidates;
//  - conservative rebuild if many references move;
//  - query emits FULL exact sorted canonical pairs and telemetry.
//
// Complexity: full build O(NlogN+K_sweep), repair O(K_skin+D*N) plus
// sort of only newly generated incident edges; query O(N+K_skin+KlogK).
// All-overlap remains Θ(N²) because K itself is Θ(N²).
//
// This is still broad phase only; not a CCD/IPC solver or novelty proof.
#include "ccp/rbc/certified_spatial_frontier.hpp"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <numeric>
#include <stdexcept>
#include <utility>
#include <vector>

namespace ccp::rbc {
class CertifiedFlatFrontier {
public:
    void initialize(const std::vector<Bounds>& input,
                    const std::vector<Reach>& reach) {
        validate(input,reach);
        reset();
        reference_=input;
        reach_=reach;
        buildAll();
        ready_=true;
    }
    void reset() noexcept {
        reference_.clear();
        reach_.clear();
        envelopes_.clear();
        edges_.clear();
        globalBuilds_=0;
        localRepairs_=0;
        ready_=false;
    }
    [[nodiscard]] bool initialized() const noexcept {return ready_;}
    [[nodiscard]] std::size_t global_builds() const noexcept {return globalBuilds_;}
    [[nodiscard]] std::size_t local_repairs() const noexcept {return localRepairs_;}
    [[nodiscard]] std::size_t potential_pairs() const noexcept {return edges_.size();}
    [[nodiscard]] SpatialQuery query(const std::vector<Bounds>& boxes) {
        return query_translated(boxes,{});
    }
    [[nodiscard]] SpatialQuery query_translated(
        const std::vector<Bounds>& boxes,ccp::live::Vec3 cumulativeShift) {
        if(!ready_)throw std::logic_error("RBC flat frontier not initialized");
        validate(boxes,reach_);
        for(double value:{cumulativeShift.x,cumulativeShift.y,cumulativeShift.z})
            if(!std::isfinite(value))
                throw std::invalid_argument("RBC flat translation must be finite");
        std::vector<Bounds> normalized;
        normalized.reserve(boxes.size());
        std::vector<std::size_t> dirty;
        dirty.reserve(boxes.size());
        for(std::size_t i=0;i<boxes.size();++i) {
            if(boxes[i].id!=reference_[i].id)
                throw std::invalid_argument("RBC flat stable primitive IDs changed");
            Bounds v=boxes[i];
            v.min.x-=cumulativeShift.x;v.max.x-=cumulativeShift.x;
            v.min.y-=cumulativeShift.y;v.max.y-=cumulativeShift.y;
            v.min.z-=cumulativeShift.z;v.max.z-=cumulativeShift.z;
            for(double x:{v.min.x,v.min.y,v.min.z,v.max.x,v.max.y,v.max.z})
                if(!std::isfinite(x))
                    throw std::overflow_error("RBC flat translation overflow");
            if(face_motion(reference_[i],v)>reach_[i].skin)
                dirty.push_back(i);
            normalized.push_back(v);
        }
        SpatialQuery out{};
        out.dirty_primitives=dirty.size();
        // When many reference envelopes drift, one compact rebuild often
        // costs less than D*N scans. This is a heuristic to TEST, not a
        // tuned evidence-backed "optimal" threshold.
        if(!dirty.empty() &&
           (dirty.size()>=std::max<std::size_t>(8,boxes.size()/5))) {
            reference_=std::move(normalized);
            out.build_comparisons=buildAll();
            out.global_rebuilt=true;
        }else if(!dirty.empty()) {
            out.build_comparisons=repairBatch(dirty,normalized);
            out.local_repairs=dirty.size();
        }
        out.pairs.reserve(edges_.size());
        for(const auto [i,j]:edges_) {
            ++out.query_pair_checks;
            if(!compatible(boxes[i],boxes[j]))continue;
            if(linf_box_gap(boxes[i],boxes[j])<=
               reach_[i].radius+reach_[j].radius)
                out.pairs.push_back({std::min(boxes[i].id,boxes[j].id),
                                     std::max(boxes[i].id,boxes[j].id)});
        }
        std::sort(out.pairs.begin(),out.pairs.end());
        if(std::adjacent_find(out.pairs.begin(),out.pairs.end())!=out.pairs.end())
            throw std::logic_error("duplicate pair in RBC flat frontier");
        out.global_build_count=globalBuilds_;
        out.local_repair_count=localRepairs_;
        out.potential_pairs=edges_.size();
        return out;
    }
private:
    using IndexPair=std::pair<std::size_t,std::size_t>;
    static bool overlap(const Bounds& a,const Bounds& b) {
        return a.min.x<=b.max.x && b.min.x<=a.max.x &&
               a.min.y<=b.max.y && b.min.y<=a.max.y &&
               a.min.z<=b.max.z && b.min.z<=a.max.z;
    }
    [[nodiscard]] static Bounds fat(const Bounds& b,const Reach& r) {
        const double padding=r.radius+r.skin;
        if(!std::isfinite(padding))
            throw std::overflow_error("RBC flat padding overflow");
        Bounds v=b;
        const auto lower=[&](double x){
            const double y=x-padding;
            if(!std::isfinite(y))
                throw std::overflow_error("RBC flat lower face overflow");
            const double z=std::nextafter(y,-std::numeric_limits<double>::infinity());
            if(!std::isfinite(z))
                throw std::overflow_error("RBC flat outward lower overflow");
            return z;
        };
        const auto upper=[&](double x){
            const double y=x+padding;
            if(!std::isfinite(y))
                throw std::overflow_error("RBC flat upper face overflow");
            const double z=std::nextafter(y,std::numeric_limits<double>::infinity());
            if(!std::isfinite(z))
                throw std::overflow_error("RBC flat outward upper overflow");
            return z;
        };
        v.min={lower(b.min.x),lower(b.min.y),lower(b.min.z)};
        v.max={upper(b.max.x),upper(b.max.y),upper(b.max.z)};
        return v;
    }
    std::size_t buildAll(){
        std::vector<Bounds> newFat;
        newFat.reserve(reference_.size());
        for(std::size_t i=0;i<reference_.size();++i)
            newFat.push_back(fat(reference_[i],reach_[i]));
        envelopes_=std::move(newFat);
        std::vector<std::size_t> order(reference_.size());
        std::iota(order.begin(),order.end(),std::size_t{0});
        std::sort(order.begin(),order.end(),[&](std::size_t i,std::size_t j){
            if(envelopes_[i].min.x!=envelopes_[j].min.x)
                return envelopes_[i].min.x<envelopes_[j].min.x;
            return reference_[i].id<reference_[j].id;
        });
        std::vector<IndexPair> pairs;
        std::vector<std::size_t> active;
        std::size_t tests=0;
        for(std::size_t i:order){
            const Bounds& a=envelopes_[i];
            active.erase(std::remove_if(active.begin(),active.end(),
                [&](std::size_t j){return envelopes_[j].max.x<a.min.x;}),
                active.end());
            for(std::size_t j:active){
                ++tests;
                if(overlap(a,envelopes_[j]))
                    pairs.emplace_back(std::min(i,j),std::max(i,j));
            }
            active.push_back(i);
        }
        std::sort(pairs.begin(),pairs.end());
        if(std::adjacent_find(pairs.begin(),pairs.end())!=pairs.end())
            throw std::logic_error("RBC flat bulk builder duplicate edge");
        edges_=std::move(pairs);
        ++globalBuilds_;
        return tests;
    }
    std::size_t repairBatch(const std::vector<std::size_t>& dirty,
                            const std::vector<Bounds>& newReference){
        const std::size_t n=reference_.size();
        std::vector<unsigned char> changed(n,0);
        std::vector<std::pair<std::size_t,Bounds>> pending;
        pending.reserve(dirty.size());
        // Prevalidate ALL replacement envelopes before touching state.
        for(std::size_t i:dirty){
            pending.emplace_back(i,fat(newReference[i],reach_[i]));
            changed[i]=1;
        }
        for(const auto& [i,b]:pending){
            reference_[i]=newReference[i];
            envelopes_[i]=b;
        }
        // Old edge set is index-ordered and unique. Preserve all
        // non-dirty edges, because neither reference envelope moved.
        std::vector<IndexPair> retained;
        retained.reserve(edges_.size());
        for(const auto& p:edges_)
            if(!changed[p.first]&&!changed[p.second])
                retained.push_back(p);
        // Re-examine ALL possible incident pairs once. Both dirty
        // endpoints are considered only by the smaller dirty index.
        std::vector<IndexPair> additions;
        std::size_t tests=0;
        for(std::size_t i:dirty)
            for(std::size_t j=0;j<n;++j) {
                if(i==j || (changed[j]&&j<i))continue;
                ++tests;
                if(overlap(envelopes_[i],envelopes_[j]))
                    additions.emplace_back(std::min(i,j),std::max(i,j));
            }
        std::sort(additions.begin(),additions.end());
        if(std::adjacent_find(additions.begin(),additions.end())!=
           additions.end())
            throw std::logic_error("RBC batch repair duplicate incident edge");
        std::vector<IndexPair> merged;
        merged.reserve(retained.size()+additions.size());
        std::merge(retained.begin(),retained.end(),
                   additions.begin(),additions.end(),
                   std::back_inserter(merged));
        edges_=std::move(merged);
        localRepairs_+=dirty.size();
        return tests;
    }
    std::vector<Bounds> reference_;
    std::vector<Reach> reach_;
    std::vector<Bounds> envelopes_;
    // Flat original source of truth: no per-edge heap nodes.
    std::vector<IndexPair> edges_;
    std::size_t globalBuilds_{};
    std::size_t localRepairs_{};
    bool ready_{};
};
} // namespace ccp::rbc
