#pragma once
// Experimental multi-cloth collision candidate laboratory.
//
// Research boundary: A genuine THREE-dimensional, coupled XPBD-style cloth
// fixture with inter-sheet vertex/triangle and edge/edge response, not an IPC,
// OGC, CCFR or continuous collision detection implementation. The innovation
// candidate is a region-selective, certificate-checked candidate *policy*;
// novelty beyond existing BVH/fat-bound techniques requires literature proof.
#include "ccp/live_cloth.hpp"
#include "ccp/online_partitioner.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <vector>
namespace ccp::research {
using Vec3 = live::Vec3;
using Bounds = live::Bounds;
using Pair = live::Pair;
enum class Policy { RebuildSAP, GlobalGuard, RegionalCertified, OnlineContactAware };
struct Sphere { Vec3 center; double radius; };
struct Primitive { Bounds box; int region{}, sheet{}; };
struct Metrics {
  std::size_t queryCount{}, pairs{}, preciseTests{}, contacts{}, interSheetContacts{},
              sphereContacts{}, broadAabbTests{}, regionalPairRebuilds{},
              regionalPairReuses{}, dirtyRegions{}, cachedPairsRejected{}, oracleMisses{},
              regionTopLevelTests{}, regionTopLevelOverlaps{}, regionPairsSkipped{},
              regionCacheEvictions{},onlinePatches{},onlineSplits{},onlineMerges{},
              onlineStrategySwitches{};
  double broadMs{}, narrowMs{}, stepMs{}, meanRegionBudget{},onlineDecisionMs{},
         maxFrameParticleTravel{},maxStructuralEdgeStretch{},maxWorldCoordinate{};
  std::size_t velocityClamps{};
         maxFrameParticleTravel{},maxStructuralEdgeStretch{},maxWorldCoordinate{};
  std::size_t velocityClamps{};
         onlineMeasuredWork{},onlineContactHeat{},onlineCoarsePairFraction{};
  bool oracleChecked{};
  std::string lastEvent{"UNQUERIED"};
  std::vector<Pair> candidatePairs,contactPairs;
  std::vector<Bounds> regionGuards;
  std::map<int,RegionWork> regionWork;
};
class RegionalCandidates {
public:
  const std::vector<Pair>& query(const std::vector<Primitive>& items, double margin,
                                 bool adaptive, Metrics& metrics,
                                 bool hierarchicalRegionSweep=false,
                                 bool collectFeedback=false);
  void reset();
private:
  struct Region {
    int id{};
    std::vector<int> itemIndices;
    std::vector<Bounds> envelopes, previous;
    Bounds unionEnvelope;
    bool dirty{};
    double budget{0.045}, motionEwma{}, previousInflation{};
  };
  std::vector<Region> regions_;
  std::map<std::uint64_t,std::vector<Pair>> cachedRegionPairs_;
  std::vector<Pair> output_;
  std::vector<int> knownIds_,knownRegions_;
  double previousMargin_{-1};
};
class MultiCloth {
public:
  explicit MultiCloth(int sheetCount=3,int sphereCount=2,Policy policy=Policy::RegionalCertified);
  void reset(int sheetCount,int sphereCount);
  void step();
  void setPolicy(Policy policy);
  void setAdaptive(bool adaptive);
  void setHierarchicalRegions(bool enabled);
  // Experimental *fixed* patch-granularity ablation, in mesh cell units.
  // Repartitioning invalidates cached region pair blocks; physical
  // triangle/particle identities and the caller-owned solver do not change.
  void setPatchCellSpan(int cells);
  int patchCellSpan() const {return patchCellSpan_;}
  bool onlineEnabled() const {return policy_==Policy::OnlineContactAware;}
  const OnlinePartitioner& onlineController() const {return online_;}
  void setAuditing(bool enable);
  bool hierarchicalRegions() const { return hierarchicalRegions_; }
  const std::vector<Vec3>& positions() const {return positions_;}
  const std::vector<std::array<int,3>>& triangles() const {return triangles_;}
  const std::vector<int>& triangleSheets() const {return triangleSheets_;}
  const std::vector<Sphere>& spheres() const {return spheres_;}
  const Metrics& metrics() const {return metrics_;}
  int sheetCount() const {return sheets_;}
  int sphereCount() const {return balls_;}
  int frame() const {return frame_;}
  Policy policy() const {return policy_;}
  // Immutable broad-phase adapter for independent, equal-input published
  // library benchmarks. It does NOT expose solver internal constraints.
  std::vector<Primitive> currentPrimitives() const;
private:
  struct Constraint {int a{},b{};double length{},compliance{},lambda{};};
  int sheets_{3},balls_{2},frame_{},patchCellSpan_{3};
  Policy policy_{Policy::RegionalCertified};
  bool adaptive_{true},auditing_{false},hierarchicalRegions_{false};
  std::vector<Vec3> positions_,previous_,initialPins_;
  std::vector<double> inverseMasses_;
  std::vector<std::array<int,3>> triangles_;
  std::vector<int> triangleSheets_,triangleRegions_,pinned_;
  std::vector<Constraint> constraints_;
  std::vector<Sphere> spheres_;
  RegionalCandidates regional_;
  OnlinePartitioner online_;
  void assignOnlineRegionIds();
  live::BroadPhaseService global_;
  Metrics metrics_;
  void updateSpheres(double t);
  std::vector<Primitive> makePrimitives() const;
  void projectContacts(const std::vector<Pair>& pairs,const std::vector<Bounds>& rawBounds);
};
} // namespace ccp::research
