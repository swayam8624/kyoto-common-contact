#pragma once
// Online contact-work-aware mesh patch selection (experimental, not claimed
// novel over published hierarchical and adaptive broad phases).
//
// One 12x12-cell cloth sheet is partitioned into disjoint topology-aligned
// square patches. Leaves may split 12->6->3 or merge four 3->6 / 6->12.
// Controller signals are measured by the broad phase + actual contact solver;
// changes only affect primitive-to-region membership, never physics inputs.
#include "ccp/live_cloth.hpp"
#include <cstddef>
#include <map>
#include <vector>
namespace ccp::research {
struct RegionWork {
 std::size_t aabbChecks{}, cachedChecks{}, candidateTouches{},
             rejectedCandidates{}, contactCorrections{}, invalidations{};
 double maxBoundMotion{};
};
struct OnlinePatch {
 int sheet{},x{},y{},span{6}; // mesh-cell coordinates; span 3, 6 or 12
 double workEwma{},contactsEwma{},candidatesEwma{};
 int hotWindows{},quietWindows{};
};
struct OnlineUpdate {
 std::size_t splits{},merges{},strategySwitches{},patches{};
 bool hierarchy{};
 double decisionMs{},observedWork{},observedContacts{},coarsePairFraction{};
};
class OnlinePartitioner {
public:
 void reset(int sheets);
 int regionForCell(int sheet,int cellX,int cellY) const;
 // Called exactly once per full solver timestep (NOT every iteration).
 // Consumes actual region work accumulated by all broad-phase queries and
 // confirmed inter-sheet/sphere corrections, never predicted contact hits.
 OnlineUpdate observe(int frame,const std::map<int,RegionWork>& work,
                      const std::vector<ccp::live::Bounds>& guards,
                      double broadMilliseconds,std::size_t queries);
 const std::vector<OnlinePatch>& patches() const {return patches_;}
 bool hierarchical() const {return hierarchical_;}
 static int stableId(const OnlinePatch& patch);
private:
 std::vector<OnlinePatch> patches_;
 int lastPatchEditFrame_{-1000},lastModeSwitchFrame_{-1000};
 bool hierarchical_{true};
 double flatTimeEwma_{},hierTimeEwma_{};
 int flatSamples_{},hierSamples_{};
 void split(std::size_t index);
 void merge(std::size_t first,const OnlinePatch& parent);
};
} // namespace ccp::research
