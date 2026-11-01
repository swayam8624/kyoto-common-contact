#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace ccp {

struct Vec3 { double x{}, y{}, z{}; };
struct Body { int id{}; Vec3 position{}, velocity{}; double radius{0.12}, mass{1.0}; };
struct Pair { int a{}, b{}; auto operator<=>(const Pair&) const = default; };
struct Contact { int a{}, b{}; Vec3 normal{}, point{}; double distance{}, penetration{}, timeOfImpact{-1.0}; };
struct Snapshot {
    std::string scene, broadPhase, queryMode;
    std::size_t frame{}, potentialPairs{}, candidates{};
    double broadMs{}, narrowMs{};
    std::vector<Body> bodies;
    std::vector<Pair> candidatePairs;
    std::vector<Contact> contacts;
    std::vector<int> islands;
    std::vector<std::vector<Vec3>> motionPaths;
    std::vector<Vec3> freeFlightPositions;
};

// Earliest time in [0, maxTime] at which two linearly moving spheres touch.
[[nodiscard]] std::optional<double> sphereSphereTimeOfImpact(
    Vec3 relativePosition, Vec3 relativeVelocity, double radiusSum, double maxTime);

class Simulation {
public:
    Simulation();
    void reset(const std::string& scene);
    void setBroadPhase(const std::string& mode);
    void setQueryMode(const std::string& mode);
    void step();
    [[nodiscard]] Snapshot snapshot() const;
private:
    std::string scene_{"elastic"}, broadPhase_{"sweep"}, queryMode_{"discrete"};
    std::size_t frame_{};
    std::vector<Body> bodies_;
    std::vector<Pair> candidates_;
    std::vector<Contact> contacts_;
    std::vector<int> islands_;
    std::vector<std::vector<Vec3>> motionPaths_;
    std::vector<Vec3> freeFlightPositions_;
    double broadMs_{}, narrowMs_{};
    void detectAndResolve(bool applyResponse = true);
    void advanceSwept();
};

[[nodiscard]] std::string toJson(const Snapshot& snapshot);

} // namespace ccp
