#pragma once
// Reach–Budget Calculus Stage 2: spatially indexed reach certificates.
//
// Unlike the original O(N^2) CertifiedReferenceList ORACLE, this
// backend constructs potential pairs through a conservative sorted
// x-axis endpoint sweep, and filters current candidates on every query.
// It is meant to challenge tuned Guard on equal exact output, not to
// claim an unmeasured win.
//
// Inclusive contact contract: for two boxes B_i,B_j, include iff
// max(nonnegative axis gaps) <= r_i+r_j AND category/mask allow the pair.
// During reuse, reference AABBs must remain within independently
// declared six-face skin after removing supplied COMMON translation.
// Any escape forces a complete, exact indexed rebuild.
//
// Reference potential pairs do not apply collision masks, so changing
// a mask/category during a valid reference interval never misses an
// newly allowed pair. Stable ordered IDs are mandatory.
//
// Build: O(NlogN + X + K) in the conservative x-sweep, where X is the
// number of x-overlap events, K is full 3D candidate count. In dense
// overlapping configurations X=Theta(N^2); no worst-case subquadratic
// time or output guarantee is claimed. Query scans O(N+K) records.
#include "ccp/rbc/reach_budget.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

namespace ccp::rbc {

struct SweepMetrics {
    std::size_t reference_builds{};
    std::size_t reference_reuses{};
    std::size_t current_candidate_tests{};
    std::size_t potential_pairs{};
    std::size_t x_sweep_tests{};
    std::size_t potential_filter_tests{};
    std::size_t reference_breaks{};
};

class CertifiedSweepIndex {
public:
    void initialize(const std::vector<Bounds>& boxes,
                    const std::vector<Reach>& reach) {
        build(boxes, reach);
        translation_anchor_={};
        metrics_.reference_breaks=0;
        metrics_.reference_reuses=0;
        metrics_.current_candidate_tests=0;
    }

    [[nodiscard]] QueryResult query(const std::vector<Bounds>& current) {
        return query_translated(current, {});
    }

    [[nodiscard]] QueryResult query_translated(
        const std::vector<Bounds>& current,
        ccp::live::Vec3 translation) {
        if (!ready_)
            throw std::logic_error("RBC indexed certificate must initialize before query");
        validate(current, reach_);
        const double v[3]={translation.x,translation.y,translation.z};
        for(double x:v) if(!std::isfinite(x))
            throw std::invalid_argument("RBC translation cannot be NaN/Inf");
        if(current.size()!=reference_.size())
            throw std::invalid_argument("RBC source cardinality changed");
        for(std::size_t i=0;i<current.size();++i)
            if(current[i].id!=reference_[i].id)
                throw std::invalid_argument("RBC stable primitive IDs/order changed");

        const ccp::live::Vec3 delta={
            translation.x-translation_anchor_.x,
            translation.y-translation_anchor_.y,
            translation.z-translation_anchor_.z
        };
        bool dirty=false;
        for(std::size_t i=0;i<current.size();++i) {
            auto ref=reference_[i];
            ref.min.x+=delta.x;ref.max.x+=delta.x;
            ref.min.y+=delta.y;ref.max.y+=delta.y;
            ref.min.z+=delta.z;ref.max.z+=delta.z;
            if(!std::isfinite(ref.min.x)||!std::isfinite(ref.max.x)||
               !std::isfinite(ref.min.y)||!std::isfinite(ref.max.y)||
               !std::isfinite(ref.min.z)||!std::isfinite(ref.max.z)||
               face_motion(ref,current[i])>reach_[i].skin){
                dirty=true;
                break;
            }
        }
        if(dirty){
            // A false solver hint cannot produce an omitted pair:
            // rebuild directly at the current source positions.
            ++metrics_.reference_breaks;
            build(current, reach_);
            translation_anchor_=translation;
        } else {
            ++metrics_.reference_reuses;
        }

        Pairs pairs;
        pairs.reserve(potential_.size());
        for(const auto& p:potential_){
            const auto& a=current[p.first],&b=current[p.second];
            ++metrics_.current_candidate_tests;
            if(!compatible(a,b))continue;
            if(linf_box_gap(a,b)<=reach_[p.first].radius+
                                      reach_[p.second].radius)
                pairs.push_back({std::min(a.id,b.id),std::max(a.id,b.id)});
        }
        std::sort(pairs.begin(),pairs.end());
        return {std::move(pairs),dirty,potential_.size(),
                metrics_.reference_builds};
    }

    [[nodiscard]] const SweepMetrics& metrics()const noexcept {return metrics_;}
    [[nodiscard]] std::size_t potential_pairs()const noexcept {
        return potential_.size();
    }

private:
    using IndexPair=std::pair<std::size_t,std::size_t>;
    struct Endpoint {
        double min_x{},max_x{};
        std::size_t index{};
    };
    std::vector<Bounds> reference_;
    std::vector<Reach> reach_;
    std::vector<IndexPair> potential_;
    SweepMetrics metrics_;
    ccp::live::Vec3 translation_anchor_{};
    bool ready_{};

    void build(const std::vector<Bounds>& boxes,
               const std::vector<Reach>& reach) {
        validate(boxes,reach);
        reference_=boxes;
        reach_=reach;
        potential_.clear();
        std::vector<Endpoint> sorted;
        sorted.reserve(boxes.size());

        // Outward-rounded reach envelopes for X sweep. We do not
        // rely on inflated coordinates for exact acceptance; full
        // original-box gamma calculation below is authoritative.
        // nextafter ensures equality of touching boundaries is not
        // accidentally rejected by a rounded endpoint.
        for(std::size_t i=0;i<boxes.size();++i) {
            const double width=reach[i].radius+reach[i].skin;
            const double lo=std::nextafter(
                boxes[i].min.x-width,-std::numeric_limits<double>::infinity());
            const double hi=std::nextafter(
                boxes[i].max.x+width,std::numeric_limits<double>::infinity());
            sorted.push_back({lo,hi,i});
        }
        std::sort(sorted.begin(),sorted.end(),
                  [](const Endpoint& a,const Endpoint& b){
                     if(a.min_x!=b.min_x)return a.min_x<b.min_x;
                     return a.index<b.index;
                  });
        std::vector<Endpoint> active;
        active.reserve(sorted.size());
        for(const auto& incoming:sorted) {
            // Remove candidates whose inflated x maxima lie strictly
            // behind the incoming minimum. Equality is a valid touch.
            active.erase(std::remove_if(active.begin(),active.end(),
                [&](const Endpoint& a){return a.max_x<incoming.min_x;}),
                active.end());
            for(const auto& previous:active) {
                ++metrics_.x_sweep_tests;
                const auto i=previous.index,j=incoming.index;
                ++metrics_.potential_filter_tests;
                const double limit=reach[i].radius+reach[j].radius+
                                   reach[i].skin+reach[j].skin;
                if(linf_box_gap(boxes[i],boxes[j])<=limit)
                    potential_.emplace_back(std::min(i,j),std::max(i,j));
            }
            active.push_back(incoming);
        }
        ++metrics_.reference_builds;
        metrics_.potential_pairs=potential_.size();
        ready_=true;
    }
};
} // namespace ccp::rbc
