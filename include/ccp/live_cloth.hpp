#pragma once

#include <array>
#include <cstddef>
#include <compare>
#include <cstdint>
#include <string>
#include <vector>

namespace ccp::live {

struct Vec3 { double x{}, y{}, z{}; };
struct Bounds {
    int id{};
    Vec3 min{}, max{};
    std::uint32_t category{1};
    std::uint32_t mask{~std::uint32_t{0}};
    bool movable{true};
};
struct Pair { int a{}, b{}; auto operator<=>(const Pair&) const = default; };
enum class UpdatePolicy { RebuildEveryIteration, GuardedReuse };
struct QueryRequest {
    double margin{0.05};
    double displacementBudget{0.04};
    UpdatePolicy updatePolicy{UpdatePolicy::GuardedReuse};
};
struct QueryMetrics {
    std::size_t builds{}, reuses{}, pairTests{};
    double broadMs{};
    // Last cache action across the current broad-phase query group.
    // Exposes WHY we rebuilt, not merely how often.
    std::string lastDecision{"UNQUERIED"};
};

// Returns canonical, sorted unique ID pairs with inclusive AABB overlap.
// SAP and exhaustive modes share this exact overlap contract.
[[nodiscard]] std::vector<Pair> candidates(const std::vector<Bounds>& bounds,
                                            bool useSAP,
                                            std::size_t* pairTests = nullptr);

// Solver-independent broad-phase cache. Metrics are accumulated into the
// caller's object. The returned reference remains valid until the next query
// or invalidate() call on this service.
class BroadPhaseService {
public:
    [[nodiscard]] const std::vector<Pair>& query(const std::vector<Bounds>& rawGeometryBounds,
                                                  QueryRequest request,
                                                  bool useSAP,
                                                  QueryMetrics& metrics);
    [[nodiscard]] const std::vector<Bounds>& envelopes() const;
    void invalidate();
private:
    bool cacheValid_{};
    QueryRequest cachedRequest_{};
    bool cachedSAP_{};
    std::vector<Bounds> envelopes_;
    std::vector<Pair> pairs_;
};

struct Stats {
    std::size_t frame{}, candidateCount{}, proximityCount{}, oracleMisses{}, pairTests{}, iterations{};
    std::size_t queryBuilds{}, queryReuses{};
    std::string cacheDecision{"UNQUERIED"};
    // CPU times are partitioned: broad, candidate-only narrow, optional
    // exhaustive oracle, final display diagnostics, and remaining integration.
    double broadMs{}, narrowMs{}, oracleMs{}, diagnosticMs{}, solverMs{}, stepMs{}, minGap{}, maxStrain{};
    std::size_t preciseTests{}, oracleTests{}, diagnosticTests{};
    bool oracleChecked{}, oracleEnabled{true};
    bool sap{true}, collisions{true}, guardedReuse{true};
    std::vector<Bounds> queryBounds;
    std::vector<int> candidateTriangles, contactTriangles;
};

class ClothSimulation {
public:
    ClothSimulation();
    void reset();
    void step();
    void setBroadPhase(bool sap);
    void setCollisions(bool enabled);
    // Oracle is independent of production narrow phase: on for verification,
    // off for unbiased timing. No check means no claim of zero misses.
    void setOracleEnabled(bool enabled);
    [[nodiscard]] bool oracleEnabled() const;
    void setQueryRequest(QueryRequest request);
    [[nodiscard]] QueryRequest queryRequest() const;
    [[nodiscard]] const std::vector<Vec3>& positions() const;
    [[nodiscard]] const std::vector<std::array<int, 3>>& triangles() const;
    [[nodiscard]] Vec3 sphereCenter() const;
    [[nodiscard]] double sphereRadius() const;
    [[nodiscard]] const Stats& stats() const;
private:
    struct Constraint { int a{}, b{}; double rest{}, compliance{}, lambda{}; };
    std::vector<Vec3> positions_, previousPositions_;
    std::vector<std::array<int, 3>> triangles_;
    std::vector<Constraint> constraints_;
    Stats stats_;
    QueryRequest queryRequest_;
    bool oracleEnabled_{true};
    BroadPhaseService broadPhase_;
};

} // namespace ccp::live
