#pragma once
// Reach–Budget Calculus (RBC), Stage 0: a formally inspectable CPU oracle.
//
// This header is a NEW research-only contract, not a replacement for frozen
// Guard/SAP V4. It deliberately uses O(N^2) construction. The purpose of the
// first stage is to test the certificate theorem and correctness invariants
// before attempting any purported accelerated structure.
//
// Current contract: exact sorted canonical pairs whose CURRENT axis-aligned
// boxes intersect after independent per-primitive l_infinity reach inflation.
// Euclidean distance, CCD and penetration-free solver semantics are NOT proven.
//
// Key invariant: If each face of B_i(t) is within skin_i of its reference
// face B_i(t0), a pair may newly qualify only if its reference box gap was
// at most radius_i + radius_j + skin_i + skin_j. Stored potential pairs
// are therefore sufficient, and exact CURRENT filtering recovers the oracle.
//
// Dynamic category/mask changes are safe because potential pairs do NOT
// exclude masked-out pairs. ID rearrangement, invalid AABBs and invalid reach
// inputs fail closed. Exhausting a skin causes a new exact O(N^2) build.
// The code reports rebuilds; it makes no performance or novelty claim.
#include "ccp/live_cloth.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

namespace ccp::rbc {
using Bounds = ccp::live::Bounds;
using Pair = ccp::live::Pair;
using Pairs = std::vector<Pair>;

struct Reach {
    double radius{}; // Current contact-proximity inflation, l_infinity.
    double skin{};   // Per-primitive reference-face motion allowance.
};

// Independent geometrical reference; not a triangle-triangle distance oracle.
// Positive gap is the largest positive separating coordinate interval.
[[nodiscard]] inline double linf_box_gap(const Bounds& a, const Bounds& b) {
    return std::max({
        0.0,
        b.min.x - a.max.x, a.min.x - b.max.x,
        b.min.y - a.max.y, a.min.y - b.max.y,
        b.min.z - a.max.z, a.min.z - b.max.z
    });
}

[[nodiscard]] inline double face_motion(const Bounds& a, const Bounds& b) {
    return std::max({
        std::abs(a.min.x-b.min.x), std::abs(a.min.y-b.min.y),
        std::abs(a.min.z-b.min.z), std::abs(a.max.x-b.max.x),
        std::abs(a.max.y-b.max.y), std::abs(a.max.z-b.max.z)
    });
}

[[nodiscard]] inline bool compatible(const Bounds& a, const Bounds& b) {
    return (a.mask & b.category) != 0 && (b.mask & a.category) != 0;
}

inline void validate(const std::vector<Bounds>& boxes,
                     const std::vector<Reach>& reach) {
    if (boxes.size() != reach.size())
        throw std::invalid_argument("RBC requires one reach contract per primitive");
    std::vector<int> ids;
    ids.reserve(boxes.size());
    for (std::size_t i=0; i<boxes.size(); ++i) {
        const auto& b = boxes[i];
        const auto& r = reach[i];
        const double values[] = {b.min.x,b.min.y,b.min.z,
                                 b.max.x,b.max.y,b.max.z,
                                 r.radius,r.skin};
        for (const double value : values)
            if (!std::isfinite(value))
                throw std::invalid_argument("RBC rejects NaN/infinite source boxes/reach");
        if (b.min.x>b.max.x || b.min.y>b.max.y || b.min.z>b.max.z)
            throw std::invalid_argument("RBC rejects inverted AABBs");
        if (r.radius<0.0 || r.skin<0.0)
            throw std::invalid_argument("RBC requires nonnegative reach and skin");
        ids.push_back(b.id);
    }
    std::sort(ids.begin(),ids.end());
    if (std::adjacent_find(ids.begin(),ids.end()) != ids.end())
        throw std::invalid_argument("RBC requires unique stable primitive IDs");
}

// Oracle at CURRENT geometry, with current compatibility and inclusive
// l_infinity reach. No cached or future pairs, no narrow-phase guarantee.
[[nodiscard]] inline Pairs exhaustive(
    const std::vector<Bounds>& boxes, const std::vector<Reach>& reach) {
    validate(boxes,reach);
    Pairs pairs;
    for (std::size_t i=0;i<boxes.size();++i)
        for (std::size_t j=i+1;j<boxes.size();++j) {
            if (!compatible(boxes[i],boxes[j])) continue;
            const double limit=reach[i].radius+reach[j].radius;
            if (linf_box_gap(boxes[i],boxes[j])<=limit)
                pairs.push_back({std::min(boxes[i].id,boxes[j].id),
                                 std::max(boxes[i].id,boxes[j].id)});
        }
    std::sort(pairs.begin(),pairs.end());
    return pairs;
}

// A bound supplied by an external integrator must be honest. The ledger
// can prove no more than the integrator's externally guaranteed per-step
// maxima. Even if this ledger passes, the actual face displacement is
// independently verified in CertifiedReferenceList::query().
class BudgetLedger {
public:
    explicit BudgetLedger(double skin) : skin_(skin) {
        if (!std::isfinite(skin)||skin<0.0)
            throw std::invalid_argument("RBC budget skin must be finite and nonnegative");
    }
    [[nodiscard]] bool can_reserve(double delta) const {
        if (!std::isfinite(delta)||delta<0.0)
            throw std::invalid_argument("RBC step displacement must be finite, nonnegative");
        return delta<=skin_-spent_;
    }
    [[nodiscard]] bool reserve(double delta) {
        if (!can_reserve(delta)) return false;
        spent_ += delta;
        return true;
    }
    void reset() noexcept { spent_=0.0; }
    [[nodiscard]] double remaining() const noexcept {return skin_-spent_;}
    [[nodiscard]] double spent() const noexcept {return spent_;}
private:
    double skin_{};
    double spent_{};
};

struct QueryResult {
    Pairs pairs;
    bool rebuilt{};
    std::size_t potential_pair_count{};
    std::size_t rebuild_count{};
};

// Validate and recheck the exact current AABBs, even after a stale or
// dishonest solver-provided bound. This is the baseline SAFE fallback,
// not yet a local repair algorithm or an optimized spatial index.
class CertifiedReferenceList {
public:
    void initialize(const std::vector<Bounds>& boxes,
                    const std::vector<Reach>& reach) {
        build(boxes,reach);
        translation_anchor_={};
    }

    // Optional solver-SUPPLIED CUMULATIVE common translation of the
    // reference frame. This avoids invalidating intra-cluster potential
    // pairs merely because every primitive translates together. It is
    // safe even if the hint is wrong: actual face residuals are CHECKED
    // against the motion-compensated reference before any reuse.
    //
    // This is a single globally co-translating group only; per-cluster
    // inter-group certificates are a separate, unsolved research stage.
    [[nodiscard]] QueryResult query_translated(const std::vector<Bounds>& current,
                                              ccp::live::Vec3 translation) {
        if (!std::isfinite(translation.x) ||
            !std::isfinite(translation.y) ||
            !std::isfinite(translation.z))
            throw std::invalid_argument("RBC translation must be finite");
        if (!ready_)
            throw std::logic_error("RBC initialize() must precede query()");
        validate(current,reach_);
        for (std::size_t i=0;i<current.size();++i)
            if (current[i].id!=reference_[i].id)
                throw std::invalid_argument(
                    "RBC primitive identity or ordering changed; explicit reinitialize required");
        bool rebuilt=false;
        const ccp::live::Vec3 relative={
            translation.x-translation_anchor_.x,
            translation.y-translation_anchor_.y,
            translation.z-translation_anchor_.z
        };
        for (std::size_t i=0;i<current.size();++i) {
            Bounds shifted=reference_[i];
            shifted.min.x+=relative.x; shifted.max.x+=relative.x;
            shifted.min.y+=relative.y; shifted.max.y+=relative.y;
            shifted.min.z+=relative.z; shifted.max.z+=relative.z;
            // If an untrustworthy translation hint underestimates
            // residual motion, THIS direct check prevents stale reuse.
            // The reference's pairwise gap remains invariant under
            // any common translation.
            if (face_motion(shifted,current[i])>reach_[i].skin) {
                build(current,reach_); // exact, total fallback
                translation_anchor_=translation;
                rebuilt=true;
                break;
            }
        }
        Pairs result;
        result.reserve(potential_.size());
        for (const auto& p : potential_) {
            // Private potential pairs store *input indices* rather than
            // ID values, which keeps lookup O(1) and order stable.
            const auto& a=current[p.first],&b=current[p.second];
            if (!compatible(a,b)) continue;
            if (linf_box_gap(a,b) <= reach_[p.first].radius+reach_[p.second].radius)
                result.push_back({std::min(a.id,b.id),std::max(a.id,b.id)});
        }
        std::sort(result.begin(),result.end());
        return {std::move(result),rebuilt,potential_.size(),rebuild_count_};
    }
    // Original fixed-world query is still available without translation.
    [[nodiscard]] QueryResult query(const std::vector<Bounds>& current) {
        return query_translated(current,{});
    }
    [[nodiscard]] std::size_t rebuilds() const noexcept {return rebuild_count_;}
    [[nodiscard]] std::size_t potential_pairs() const noexcept {return potential_.size();}
private:
    using IndexPair = std::pair<std::size_t,std::size_t>;
    std::vector<Bounds> reference_;
    std::vector<Reach> reach_;
    std::vector<IndexPair> potential_;
    std::size_t rebuild_count_{};
    ccp::live::Vec3 translation_anchor_{};
    bool ready_{};

    void build(const std::vector<Bounds>& boxes,
               const std::vector<Reach>& reach) {
        validate(boxes,reach);
        reference_=boxes;
        reach_=reach;
        potential_.clear();
        for (std::size_t i=0;i<boxes.size();++i)
            for (std::size_t j=i+1;j<boxes.size();++j) {
                // The potential set includes masked-out pairs. A change
                // of collision mask during reuse therefore cannot create
                // an omitted pair. We only filter masks at query time.
                const double limit=reach[i].radius+reach[j].radius+
                                   reach[i].skin+reach[j].skin;
                if (linf_box_gap(boxes[i],boxes[j])<=limit)
                    potential_.emplace_back(i,j);
            }
        ++rebuild_count_;
        ready_=true;
    }
};

// Sufficient swept envelope for LINEAR trajectories of source vertices,
// because a vertex coordinate varies affinely over a step.
// This helper takes two endpoint AABBs with matching stable IDs.
// For curved/accelerated trajectories, endpoints are NOT sufficient:
// the solver must supply a certified enclosure of the entire path.
[[nodiscard]] inline Bounds linear_endpoint_hull(const Bounds& a,
                                                 const Bounds& b) {
    if (a.id!=b.id)
        throw std::invalid_argument("RBC swept endpoints must share stable IDs");
    validate(std::vector<Bounds>{a},std::vector<Reach>{{}});
    validate(std::vector<Bounds>{b},std::vector<Reach>{{}});
    if (a.category!=b.category || a.mask!=b.mask)
        throw std::invalid_argument(
            "RBC linear swept endpoints require fixed category/mask within step");
    Bounds h=a;
    h.min.x=std::min(a.min.x,b.min.x);
    h.min.y=std::min(a.min.y,b.min.y);
    h.min.z=std::min(a.min.z,b.min.z);
    h.max.x=std::max(a.max.x,b.max.x);
    h.max.y=std::max(a.max.y,b.max.y);
    h.max.z=std::max(a.max.z,b.max.z);
    return h;
}
} // namespace ccp::rbc
