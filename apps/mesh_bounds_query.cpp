#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <numeric>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using Vec3 = std::array<double, 3>;

struct PrimitiveBounds {
    int id;
    std::string kind;
    Vec3 min;
    Vec3 max;
};

struct Triangle {
    int id;
    std::array<Vec3, 3> vertices;
};

struct MeshInput {
    Vec3 sphereCenter{};
    double radius{};
    double margin{};
    std::vector<Triangle> triangles;
};

struct QueryResult {
    std::vector<int> candidateTriangleIds;
    std::vector<int> proximityTriangleIds;
    std::vector<int> misses;
    std::vector<double> distances;
    std::vector<double> gaps;
    std::vector<bool> candidates;
    std::vector<bool> withinMargin;
    double minGap{std::numeric_limits<double>::infinity()};
    bool verifiedSameOracle{};
};

struct Pair {
    int a;
    int b;
    auto operator<=>(const Pair&) const = default;
};

Vec3 subtract(const Vec3& a, const Vec3& b) {
    return {a[0] - b[0], a[1] - b[1], a[2] - b[2]};
}
Vec3 add(const Vec3& a, const Vec3& b) {
    return {a[0] + b[0], a[1] + b[1], a[2] + b[2]};
}
Vec3 scale(const Vec3& a, double s) { return {a[0] * s, a[1] * s, a[2] * s}; }
double dot(const Vec3& a, const Vec3& b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }
double norm(const Vec3& a) { return std::hypot(a[0], a[1], a[2]); }

void requireFinite(const Vec3& value, const std::string& label) {
    for (double component : value)
        if (!std::isfinite(component)) throw std::runtime_error(label + " must contain only finite values");
}

MeshInput readInput(std::istream& input) {
    std::size_t vertexCount = 0, triangleCount = 0;
    if (!(input >> vertexCount >> triangleCount))
        throw std::runtime_error("expected vertex_count triangle_count on the first line");
    if (vertexCount > static_cast<std::size_t>(std::numeric_limits<int>::max()) ||
        triangleCount > static_cast<std::size_t>(std::numeric_limits<int>::max()))
        throw std::runtime_error("vertex and triangle counts must fit in signed 32-bit indices");

    MeshInput mesh;
    if (!(input >> mesh.sphereCenter[0] >> mesh.sphereCenter[1] >> mesh.sphereCenter[2] >> mesh.radius >> mesh.margin))
        throw std::runtime_error("expected sphere_x sphere_y sphere_z radius margin on the second line");
    requireFinite(mesh.sphereCenter, "sphere center");
    if (!std::isfinite(mesh.radius) || mesh.radius < 0.0) throw std::runtime_error("sphere radius must be finite and non-negative");
    if (!std::isfinite(mesh.margin) || mesh.margin < 0.0) throw std::runtime_error("margin must be finite and non-negative");

    std::vector<Vec3> vertices(vertexCount);
    for (std::size_t i = 0; i < vertexCount; ++i) {
        if (!(input >> vertices[i][0] >> vertices[i][1] >> vertices[i][2]))
            throw std::runtime_error("missing or invalid coordinates for vertex " + std::to_string(i));
        requireFinite(vertices[i], "vertex " + std::to_string(i));
    }

    mesh.triangles.reserve(triangleCount);
    for (std::size_t i = 0; i < triangleCount; ++i) {
        long long a = -1, b = -1, c = -1;
        if (!(input >> a >> b >> c)) throw std::runtime_error("missing or invalid indices for triangle " + std::to_string(i));
        if (a < 0 || b < 0 || c < 0 || static_cast<unsigned long long>(a) >= vertexCount ||
            static_cast<unsigned long long>(b) >= vertexCount || static_cast<unsigned long long>(c) >= vertexCount)
            throw std::runtime_error("vertex index out of range in triangle " + std::to_string(i));
        mesh.triangles.push_back({static_cast<int>(i), {vertices[static_cast<std::size_t>(a)],
                                                        vertices[static_cast<std::size_t>(b)],
                                                        vertices[static_cast<std::size_t>(c)]}});
    }
    std::string extra;
    if (input >> extra) throw std::runtime_error("unexpected data after the final triangle");
    return mesh;
}

PrimitiveBounds triangleBounds(const Triangle& triangle, double padding) {
    PrimitiveBounds bounds{triangle.id, "triangle", triangle.vertices[0], triangle.vertices[0]};
    for (const auto& vertex : triangle.vertices) {
        for (std::size_t axis = 0; axis < 3; ++axis) {
            bounds.min[axis] = std::min(bounds.min[axis], vertex[axis]);
            bounds.max[axis] = std::max(bounds.max[axis], vertex[axis]);
        }
    }
    for (std::size_t axis = 0; axis < 3; ++axis) {
        bounds.min[axis] -= padding;
        bounds.max[axis] += padding;
        if (!std::isfinite(bounds.min[axis]) || !std::isfinite(bounds.max[axis]))
            throw std::runtime_error("triangle bounds overflowed for triangle " + std::to_string(triangle.id));
    }
    return bounds;
}

PrimitiveBounds sphereBounds(const Vec3& center, double radius, double padding) {
    const double extent = radius + padding;
    if (!std::isfinite(extent)) throw std::runtime_error("sphere radius plus margin padding overflowed");
    PrimitiveBounds bounds{-1, "sphere", {}, {}};
    for (std::size_t axis = 0; axis < 3; ++axis) {
        bounds.min[axis] = center[axis] - extent;
        bounds.max[axis] = center[axis] + extent;
        if (!std::isfinite(bounds.min[axis]) || !std::isfinite(bounds.max[axis]))
            throw std::runtime_error("sphere bounds overflowed");
    }
    return bounds;
}

bool overlaps(const PrimitiveBounds& a, const PrimitiveBounds& b) {
    for (std::size_t axis = 0; axis < 3; ++axis)
        if (a.max[axis] < b.min[axis] || b.max[axis] < a.min[axis]) return false;
    return true;
}

std::vector<Pair> exhaustive(const std::vector<PrimitiveBounds>& bounds) {
    std::vector<Pair> pairs;
    for (std::size_t i = 0; i < bounds.size(); ++i) {
        for (std::size_t j = i + 1; j < bounds.size(); ++j) {
            if (bounds[i].kind == bounds[j].kind || !overlaps(bounds[i], bounds[j])) continue;
            pairs.push_back({bounds[i].id, bounds[j].id});
        }
    }
    std::sort(pairs.begin(), pairs.end());
    return pairs;
}

std::vector<Pair> sweepAndPrune(const std::vector<PrimitiveBounds>& bounds) {
    std::vector<std::size_t> order(bounds.size());
    std::iota(order.begin(), order.end(), 0);
    std::sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b) {
        return bounds[a].min[0] == bounds[b].min[0] ? bounds[a].id < bounds[b].id
                                                     : bounds[a].min[0] < bounds[b].min[0];
    });
    std::vector<Pair> pairs;
    for (std::size_t i = 0; i < order.size(); ++i) {
        const auto ia = order[i];
        for (std::size_t j = i + 1; j < order.size() && bounds[order[j]].min[0] <= bounds[ia].max[0]; ++j) {
            const auto ib = order[j];
            if (bounds[ia].kind != bounds[ib].kind && overlaps(bounds[ia], bounds[ib]))
                pairs.push_back({std::min(bounds[ia].id, bounds[ib].id), std::max(bounds[ia].id, bounds[ib].id)});
        }
    }
    std::sort(pairs.begin(), pairs.end());
    return pairs;
}

double pointSegmentDistance(const Vec3& point, const Vec3& a, const Vec3& b) {
    const Vec3 ab = subtract(b, a);
    const double lengthSquared = dot(ab, ab);
    if (lengthSquared == 0.0) return norm(subtract(point, a));
    const double t = std::clamp(dot(subtract(point, a), ab) / lengthSquared, 0.0, 1.0);
    return norm(subtract(point, add(a, scale(ab, t))));
}

double pointTriangleDistance(const Vec3& point, const Triangle& triangle) {
    const Vec3 a = triangle.vertices[0], b = triangle.vertices[1], c = triangle.vertices[2];
    const Vec3 ab = subtract(b, a), ac = subtract(c, a), bc = subtract(c, b);
    const double areaTwice = norm({ab[1] * ac[2] - ab[2] * ac[1],
                                   ab[2] * ac[0] - ab[0] * ac[2],
                                   ab[0] * ac[1] - ab[1] * ac[0]});
    const double edgeScale = std::max({norm(ab), norm(ac), norm(bc)});
    if (areaTwice <= std::numeric_limits<double>::epsilon() * edgeScale * edgeScale * 16.0)
        return std::min({pointSegmentDistance(point, a, b), pointSegmentDistance(point, b, c),
                         pointSegmentDistance(point, c, a)});

    // Closest-point region tests divide the plane into vertex, edge, and face regions.
    const Vec3 ap = subtract(point, a);
    const double d1 = dot(ab, ap), d2 = dot(ac, ap);
    if (d1 <= 0.0 && d2 <= 0.0) return norm(ap);
    const Vec3 bp = subtract(point, b);
    const double d3 = dot(ab, bp), d4 = dot(ac, bp);
    if (d3 >= 0.0 && d4 <= d3) return norm(bp);
    const double vc = d1 * d4 - d3 * d2;
    if (vc <= 0.0 && d1 >= 0.0 && d3 <= 0.0)
        return norm(subtract(point, add(a, scale(ab, d1 / (d1 - d3)))));
    const Vec3 cp = subtract(point, c);
    const double d5 = dot(ab, cp), d6 = dot(ac, cp);
    if (d6 >= 0.0 && d5 <= d6) return norm(cp);
    const double vb = d5 * d2 - d1 * d6;
    if (vb <= 0.0 && d2 >= 0.0 && d6 <= 0.0)
        return norm(subtract(point, add(a, scale(ac, d2 / (d2 - d6)))));
    const double va = d3 * d6 - d5 * d4;
    if (va <= 0.0 && (d4 - d3) >= 0.0 && (d5 - d6) >= 0.0) {
        const double edgeParameter = (d4 - d3) / ((d4 - d3) + (d5 - d6));
        return norm(subtract(point, add(b, scale(bc, edgeParameter))));
    }
    const double inverse = 1.0 / (va + vb + vc);
    const double v = vb * inverse, w = vc * inverse;
    return norm(subtract(point, add(a, add(scale(ab, v), scale(ac, w)))));
}

QueryResult query(const MeshInput& mesh) {
    const double padding = mesh.margin / 2.0;
    std::vector<PrimitiveBounds> bounds;
    bounds.reserve(mesh.triangles.size() + 1);
    bounds.push_back(sphereBounds(mesh.sphereCenter, mesh.radius, padding));
    for (const auto& triangle : mesh.triangles) bounds.push_back(triangleBounds(triangle, padding));

    const auto sapPairs = sweepAndPrune(bounds);
    const auto oraclePairs = exhaustive(bounds);
    QueryResult result;
    result.verifiedSameOracle = sapPairs == oraclePairs;
    if (!result.verifiedSameOracle) throw std::runtime_error("SAP candidate IDs do not match the exhaustive AABB oracle");
    for (const auto& pair : sapPairs) result.candidateTriangleIds.push_back(std::max(pair.a, pair.b));

    result.distances.reserve(mesh.triangles.size());
    result.gaps.reserve(mesh.triangles.size());
    result.candidates.reserve(mesh.triangles.size());
    result.withinMargin.reserve(mesh.triangles.size());
    for (const auto& triangle : mesh.triangles) {
        const double distance = pointTriangleDistance(mesh.sphereCenter, triangle);
        if (!std::isfinite(distance)) throw std::runtime_error("distance overflow while evaluating triangle " + std::to_string(triangle.id));
        const double gap = distance - mesh.radius;
        if (!std::isfinite(gap)) throw std::runtime_error("gap overflow while evaluating triangle " + std::to_string(triangle.id));
        const bool candidate = std::binary_search(result.candidateTriangleIds.begin(), result.candidateTriangleIds.end(), triangle.id);
        const bool within = distance <= mesh.radius + mesh.margin;
        result.distances.push_back(distance);
        result.gaps.push_back(gap);
        result.candidates.push_back(candidate);
        result.withinMargin.push_back(within);
        result.minGap = std::min(result.minGap, gap);
        if (within) {
            result.proximityTriangleIds.push_back(triangle.id);
            if (!candidate) result.misses.push_back(triangle.id);
        }
    }
    return result;
}

std::string number(double value) {
    std::ostringstream out;
    out << std::setprecision(17) << value;
    return out.str();
}

void writeIntArray(std::ostream& output, const std::vector<int>& values) {
    output << '[';
    for (std::size_t i = 0; i < values.size(); ++i) {
        if (i != 0) output << ',';
        output << values[i];
    }
    output << ']';
}

void writeJson(std::ostream& output, const MeshInput& mesh, const QueryResult& result) {
    output << "{\"candidateTriangleIds\":";
    writeIntArray(output, result.candidateTriangleIds);
    output << ",\"proximityTriangleIds\":";
    writeIntArray(output, result.proximityTriangleIds);
    output << ",\"misses\":";
    writeIntArray(output, result.misses);
    output << ",\"totalTriangles\":" << mesh.triangles.size() << ",\"minGap\":";
    if (std::isfinite(result.minGap)) output << number(result.minGap);
    else output << "null";
    output << ",\"margin\":" << number(mesh.margin) << ",\"verifiedSameOracle\":"
           << (result.verifiedSameOracle ? "true" : "false") << ",\"gapByTriangle\":[";
    for (std::size_t i = 0; i < mesh.triangles.size(); ++i) {
        if (i != 0) output << ',';
        output << "{\"triangleId\":" << mesh.triangles[i].id << ",\"distance\":" << number(result.distances[i])
               << ",\"gap\":" << number(result.gaps[i]) << ",\"candidate\":" << (result.candidates[i] ? "true" : "false")
               << ",\"withinMargin\":" << (result.withinMargin[i] ? "true" : "false") << '}';
    }
    output << "]}\n";
}

void selfTest() {
    const std::string fixture =
        "6 2\n"
        "0 0 0.1 0.1 0.02\n"
        "-1 -1 0\n1 -1 0\n0 1 0\n"
        "3 0 0\n4 0 0\n5 0 0\n"
        "0 1 2\n3 4 5\n";
    std::istringstream input(fixture);
    const MeshInput mesh = readInput(input);
    const QueryResult result = query(mesh);
    if (!result.verifiedSameOracle || result.candidateTriangleIds != std::vector<int>{0} ||
        result.proximityTriangleIds != std::vector<int>{0} || !result.misses.empty() ||
        std::abs(result.gaps[0]) > 1e-12 || std::abs(result.distances[1] - std::hypot(3.0, 0.1)) > 1e-12)
        throw std::runtime_error("analytic mesh query self-test failed");

    // A fully degenerate triangle is treated as its longest segment (or a point).
    const Triangle pointTriangle{7, {{{1.0, 2.0, 3.0}, {1.0, 2.0, 3.0}, {1.0, 2.0, 3.0}}}};
    if (std::abs(pointTriangleDistance({1.0, 2.0, 5.0}, pointTriangle) - 2.0) > 1e-12)
        throw std::runtime_error("degenerate-triangle distance self-test failed");
    std::cout << "mesh_bounds_query self-test passed: SAP/oracle agreement, analytic proximity/gaps, and degenerate triangle distance.\n";
}

} // namespace

int main(int argc, char** argv) {
    try {
        if (argc == 2 && std::string(argv[1]) == "--self-test") {
            selfTest();
            return 0;
        }
        if (argc != 3) {
            std::cerr << "Usage: mesh_bounds_query <input.mesh> <output.json>\n"
                         "       mesh_bounds_query --self-test\n";
            return 2;
        }
        std::ifstream input(argv[1]);
        if (!input) throw std::runtime_error("could not open input mesh: " + std::string(argv[1]));
        const MeshInput mesh = readInput(input);
        const QueryResult result = query(mesh);
        std::ofstream output(argv[2]);
        if (!output) throw std::runtime_error("could not open output JSON: " + std::string(argv[2]));
        writeJson(output, mesh, result);
        if (!output) throw std::runtime_error("failed while writing output JSON: " + std::string(argv[2]));
        std::cout << "Queried " << mesh.triangles.size() << " triangles; " << result.candidateTriangleIds.size()
                  << " broad-phase candidates, " << result.proximityTriangleIds.size() << " within margin; SAP matches exhaustive oracle.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "mesh_bounds_query: " << error.what() << '\n';
        return 1;
    }
}
