#include "template_editor.h"
#include "planar_layout.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <map>
#include <numeric>

namespace {
using Points = std::vector<DoubleVector2>;
constexpr double grid = 64;
constexpr double coordinateLimit = 1e7;

double pointEdgeDistance(DoubleVector2 p, DoubleVector2 a, DoubleVector2 b) {
    const double dx = b.x - a.x, dy = b.y - a.y;
    const double px = p.x - a.x, py = p.y - a.y;
    const auto distance = [&](double t) { return std::max(std::abs(px - t * dx), std::abs(py - t * dy)); };
    double result = std::min(distance(0), distance(1));
    // Exact distance in the square (L-infinity) metric. Its minimum is at an
    // endpoint or where the two absolute coordinate differences are equal.
    if (dx != dy) result = std::min(result, distance(std::clamp((px - py) / (dx - dy), 0.0, 1.0)));
    if (dx != -dy) result = std::min(result, distance(std::clamp((px + py) / (dx + dy), 0.0, 1.0)));
    return result;
}

// Keep a single complete pattern at 100% zoom. Square bounds and a small
// clearance replace the previous large circular halos and two-repeat padding.
double spacingScale(const Points& points, const std::vector<planar::Edge>& edges,
    const std::vector<double>& halfSizes, const std::vector<double>& patterns) {
    double scale = 0;
    for (size_t i = 0; i < points.size(); ++i) {
        for (size_t j = 0; j < i; ++j) {
            const double separation = std::max(std::abs(points[i].x - points[j].x), std::abs(points[i].y - points[j].y));
            if (separation == 0) return std::numeric_limits<double>::infinity();
            scale = std::max(scale, (halfSizes[i] + halfSizes[j] + 16) / separation);
        }
        for (auto [a, b] : edges) {
            if (i == a || i == b) continue;
            const double clearance = pointEdgeDistance(points[i], points[a], points[b]);
            if (clearance == 0) return std::numeric_limits<double>::infinity();
            scale = std::max(scale, (halfSizes[i] + 8) / clearance);
        }
    }
    for (size_t e = 0; e < edges.size(); ++e) {
        const auto [a, b] = edges[e];
        const double dx = points[a].x - points[b].x, dy = points[a].y - points[b].y;
        const double distance = std::hypot(dx, dy), maximumAxis = std::max(std::abs(dx), std::abs(dy));
        if (distance == 0) return std::numeric_limits<double>::infinity();
        scale = std::max(scale, (halfSizes[a] + halfSizes[b]) / maximumAxis + patterns[e] / distance);
    }
    return scale;
}

DoubleVector2 snap(DoubleVector2 point) {
    const DoubleVector2 cross{std::round(point.x / grid) * grid, std::round(point.y / grid) * grid};
    const DoubleVector2 center{std::round((point.x - grid / 2) / grid) * grid + grid / 2,
        std::round((point.y - grid / 2) / grid) * grid + grid / 2};
    return std::hypot(point.x - cross.x, point.y - cross.y) <= std::hypot(point.x - center.x, point.y - center.y)
        ? cross : center;
}

Points spacedDrawing(const Points& input, const planar::Topology& topology,
    const std::vector<double>& halfSizes, const std::vector<double>& patterns) {
    if (input.empty()) return {};
    double minX = input[0].x, maxX = minX, minY = input[0].y, maxY = minY;
    for (auto p : input) {
        minX = std::min(minX, p.x); maxX = std::max(maxX, p.x);
        minY = std::min(minY, p.y); maxY = std::max(maxY, p.y);
    }
    double scale = std::max(1.0, spacingScale(input, topology.edges, halfSizes, patterns));
    // A whole grid pitch preserves exact rows and columns, including symmetry
    // in rectangular harmonic diagrams. Only expand when validation needs it.
    const double pitch = std::ceil(scale / grid) * grid;
    for (int attempt = 0; attempt < 24 && std::isfinite(scale); ++attempt) {
        const double factor = attempt < 4 ? pitch + attempt * grid : scale;
        const DoubleVector2 center{std::round((minX + maxX) * factor / (2 * grid)) * grid,
            std::round((minY + maxY) * factor / (2 * grid)) * grid};
        Points result;
        for (auto p : input) {
            const auto placed = snap({p.x * factor - center.x, p.y * factor - center.y});
            if (!std::isfinite(placed.x) || !std::isfinite(placed.y)
                || std::abs(placed.x) > coordinateLimit || std::abs(placed.y) > coordinateLimit) return {};
            result.push_back(placed);
        }
        if (planar::validDrawing(result, topology.edges)
            && spacingScale(result, topology.edges, halfSizes, patterns) <= 1 + 1e-9) return result;
        if (attempt >= 4) scale *= 1.15;
    }
    return {};
}

Points rectangularDrawing(const planar::Topology& topology) {
    const size_t realCount = topology.ids.size();
    if (realCount < 3) {
        Points points;
        for (size_t i = 0; i < realCount; ++i) points.emplace_back(i, 0);
        return points;
    }
    auto boundary = topology.outerFace;
    const size_t m = boundary.size();
    if (m < 3) return {};
    // Use the private biconnected scaffold, including face centers. Solving
    // only real edges collapses leaves onto their parent and whole branches
    // onto a line, forcing an unnecessarily large triangular fallback.
    const size_t n = topology.scaffoldSize - topology.boundary.size();
    std::vector<std::vector<size_t>> neighbours(n);
    for (auto [a, b] : topology.scaffoldEdges) if (a < n && b < n) {
        neighbours[a].push_back(b); neighbours[b].push_back(a);
    }
    std::vector<size_t> degree(realCount);
    for (auto [a, b] : topology.edges) { ++degree[a]; ++degree[b]; }
    const auto first = std::min_element(boundary.begin(), boundary.end(), [&](size_t a, size_t b) {
        return std::pair{degree[a], topology.ids[a]} < std::pair{degree[b], topology.ids[b]};
    });
    std::rotate(boundary.begin(), first, boundary.end());
    if (topology.ids[boundary[1]] > topology.ids[boundary.back()]) std::reverse(boundary.begin() + 1, boundary.end());
    Points points(n);
    if (m == 3) {
        points[boundary[0]] = {0, 0}; points[boundary[1]] = {2, 0}; points[boundary[2]] = {1, 2};
    } else {
        // Prefer low-degree, evenly spaced corners. This recovers rectangular
        // perimeter grids without depending on IDs, existing XYs or a template.
        std::vector<std::vector<double>> cost(5, std::vector<double>(m + 1, std::numeric_limits<double>::infinity()));
        std::vector<std::vector<size_t>> previous(5, std::vector<size_t>(m + 1));
        cost[0][0] = 0;
        for (size_t side = 1; side <= 4; ++side) {
            for (size_t end = side; end <= m - (4 - side); ++end) {
                if (side == 4 && end != m) continue;
                for (size_t start = side - 1; start < end; ++start) {
                    const double imbalance = 4.0 * (end - start) / m - 1;
                    const double candidate = cost[side - 1][start] + imbalance * imbalance
                        + (side < 4 ? 4.0 * degree[boundary[end]] : 0);
                    if (candidate < cost[side][end]) { cost[side][end] = candidate; previous[side][end] = start; }
                }
            }
        }
        std::array<size_t, 5> corners{0, 0, 0, 0, m};
        for (size_t side = 4; side > 0; --side) corners[side - 1] = previous[side][corners[side]];
        const double width = std::max(corners[1], corners[3] - corners[2]);
        const double height = std::max(corners[2] - corners[1], m - corners[3]);
        const std::array<DoubleVector2, 5> rectangle{{{0, 0}, {width, 0}, {width, height}, {0, height}, {0, 0}}};
        for (size_t side = 0; side < 4; ++side) for (size_t i = corners[side]; i < corners[side + 1]; ++i) {
            const double t = double(i - corners[side]) / (corners[side + 1] - corners[side]);
            points[boundary[i]] = {rectangle[side].x + t * (rectangle[side + 1].x - rectangle[side].x),
                rectangle[side].y + t * (rectangle[side + 1].y - rectangle[side].y)};
        }
    }
    // Uniform barycenters retain graph symmetries; no random perturbation,
    // magnetic forces, generation geometry weights or radius forces are used.
    std::vector<bool> fixed(n);
    for (auto vertex : boundary) fixed[vertex] = true;
    std::vector<size_t> free, index(n, n);
    for (size_t i = 0; i < n; ++i) if (!fixed[i]) { index[i] = free.size(); free.push_back(i); }
    const size_t count = free.size();
    std::vector<std::vector<double>> matrix(count, std::vector<double>(count + 2));
    for (size_t row = 0; row < count; ++row) {
        const auto vertex = free[row];
        matrix[row][row] = neighbours[vertex].size();
        for (auto next : neighbours[vertex]) {
            if (fixed[next]) { matrix[row][count] += points[next].x; matrix[row][count + 1] += points[next].y; }
            else matrix[row][index[next]] -= 1;
        }
    }
    for (size_t column = 0; column < count; ++column) {
        size_t pivot = column;
        for (size_t row = column + 1; row < count; ++row)
            if (std::abs(matrix[row][column]) > std::abs(matrix[pivot][column])) pivot = row;
        if (std::abs(matrix[pivot][column]) < 1e-12) return {};
        std::swap(matrix[column], matrix[pivot]);
        for (size_t row = column + 1; row < count; ++row) {
            const double factor = matrix[row][column] / matrix[column][column];
            for (size_t j = column + 1; j < count + 2; ++j) matrix[row][j] -= factor * matrix[column][j];
        }
    }
    for (size_t row = count; row-- > 0;) {
        double x = matrix[row][count], y = matrix[row][count + 1];
        for (size_t column = row + 1; column < count; ++column) {
            x -= matrix[row][column] * points[free[column]].x;
            y -= matrix[row][column] * points[free[column]].y;
        }
        points[free[row]] = {x / matrix[row][row], y / matrix[row][row]};
    }
    points.resize(realCount);
    return planar::validDrawing(points, topology.edges) ? points : Points{};
}

Points alignRows(Points points) {
    for (bool horizontal : {true, false}) {
        std::vector<size_t> order(points.size());
        std::iota(order.begin(), order.end(), 0);
        const auto coordinate = [&](size_t i) { return horizontal ? points[i].x : points[i].y; };
        std::sort(order.begin(), order.end(), [&](size_t a, size_t b) { return coordinate(a) < coordinate(b); });
        if (order.empty()) continue;
        const double epsilon = 1e-7 * std::max(1.0, coordinate(order.back()) - coordinate(order.front()));
        std::vector<double> ranks(points.size());
        double rank = 0, last = coordinate(order.front());
        for (auto i : order) {
            if (coordinate(i) - last > epsilon) { ++rank; last = coordinate(i); }
            ranks[i] = rank;
        }
        for (size_t i = 0; i < points.size(); ++i) {
            if (horizontal) points[i].x = ranks[i];
            else points[i].y = ranks[i];
        }
    }
    return points;
}

double diagramScore(const Points& points, const std::vector<planar::Edge>& edges) {
    if (points.empty()) return std::numeric_limits<double>::infinity();
    double minX = points[0].x, maxX = minX, minY = points[0].y, maxY = minY, awkward = 0;
    for (auto p : points) { minX = std::min(minX, p.x); maxX = std::max(maxX, p.x); minY = std::min(minY, p.y); maxY = std::max(maxY, p.y); }
    for (auto [a, b] : edges) {
        const double dx = std::abs(points[a].x - points[b].x), dy = std::abs(points[a].y - points[b].y);
        if (dx > 0 && dy > 0 && std::abs(dx - dy) > 1e-6) ++awkward;
    }
    const double width = maxX - minX + grid, height = maxY - minY + grid;
    return (width * height + 0.25 * std::max(width * width, height * height))
        * (1 + awkward / std::max(size_t(1), edges.size()));
}

Points relaxDiagram(Points points, const planar::Topology& topology,
    const std::vector<double>& halfSizes, const std::vector<double>& patterns) {
    if (points.size() < 3) return points;
    for (int step = 0; step < 600; ++step) {
        Points movement(points.size());
        const auto push = [&](size_t a, size_t b, DoubleVector2 delta, double force) {
            movement[a].x += delta.x * force; movement[a].y += delta.y * force;
            movement[b].x -= delta.x * force; movement[b].y -= delta.y * force;
        };
        for (size_t e = 0; e < topology.edges.size(); ++e) {
            auto [a, b] = topology.edges[e];
            const DoubleVector2 delta{points[b].x - points[a].x, points[b].y - points[a].y};
            const double d = std::hypot(delta.x, delta.y);
            const double target = (halfSizes[a] + halfSizes[b]) * d / std::max(std::abs(delta.x), std::abs(delta.y))
                + patterns[e] + 24;
            push(a, b, delta, 0.06 * (d - target) / d);
        }
        for (size_t v = 0; v < points.size(); ++v) {
            for (size_t u = 0; u < v; ++u) {
                const DoubleVector2 delta{points[v].x - points[u].x, points[v].y - points[u].y};
                const double d = std::hypot(delta.x, delta.y);
                const double target = 1.42 * (halfSizes[v] + halfSizes[u] + 20);
                if (d < target) push(v, u, delta, 0.4 * (target - d) / d);
            }
            for (auto [a, b] : topology.edges) {
                if (v == a || v == b) continue;
                const DoubleVector2 ab{points[b].x - points[a].x, points[b].y - points[a].y};
                const double t = std::clamp(((points[v].x - points[a].x) * ab.x
                    + (points[v].y - points[a].y) * ab.y) / (ab.x * ab.x + ab.y * ab.y), 0.0, 1.0);
                const DoubleVector2 delta{points[v].x - points[a].x - t * ab.x,
                    points[v].y - points[a].y - t * ab.y};
                const double d = std::hypot(delta.x, delta.y), target = 1.42 * halfSizes[v] + 28;
                if (d <= 1e-8 || d >= target) continue;
                const double force = 0.8 * (target - d) / d;
                push(v, a, delta, force * (1 - t)); push(v, b, delta, force * t);
            }
        }
        for (auto& delta : movement) {
            const double d = std::hypot(delta.x, delta.y);
            if (d > 24) { delta.x *= 24 / d; delta.y *= 24 / d; }
        }
        planar::limitDisplacements(points, topology.edges, movement);
        auto candidate = points;
        for (size_t i = 0; i < points.size(); ++i) { candidate[i].x += movement[i].x; candidate[i].y += movement[i].y; }
        if (planar::validDrawing(candidate, topology.edges)) points = std::move(candidate);
    }
    return points;
}
}

void TemplateEditor::updateTopology() const {
    std::vector<int> ids;
    std::vector<std::pair<int, int>> edges;
    for (const auto& node : document_["nodes"]) ids.push_back(node["id"].get<int>());
    for (const auto& edge : document_["symmetrical_edges"]) {
        const int a = edge["ID_first"], b = edge["ID_second"];
        edges.emplace_back(std::min(a, b), std::max(a, b));
    }
    std::sort(ids.begin(), ids.end());
    std::sort(edges.begin(), edges.end());
    const Json signature = Json::array({ids, edges});
    if (signature == topologySignature_) return;
    topologySignature_ = signature;
    topology_.reset(); topologyWarning_.clear();
    std::vector<Identifiable> identified;
    std::vector<std::pair<Identifiable, Identifiable>> connections;
    for (int id : ids) identified.emplace_back(id);
    for (auto [a, b] : edges) connections.emplace_back(a, b);
    try { topology_ = planar::makeTopology(std::move(identified), connections); }
    catch (const std::exception& error) { topologyWarning_ = error.what(); }
}

const std::string& TemplateEditor::topologyWarning() const {
    updateTopology();
    return topologyWarning_;
}

double TemplateEditor::patternLength(const Json& edge) {
    double length = 0;
    for (const char* key : {"minWallLength", "minPassWidth", "maxWallLength", "maxPassWidth"})
        length += 2 * edge.at("data").at("passes").at(key).get<double>();
    return length;
}

bool TemplateEditor::autoPlace() {
    updateTopology();
    if (!topologyWarning_.empty()) { error_ = topologyWarning_; return false; }
    if (topology_->ids.empty()) { error_.clear(); return true; }
    try {
        std::vector<double> halfSizes, patterns;
        std::map<std::pair<int, int>, double> edgePatterns;
        for (const auto& edge : document_["symmetrical_edges"]) {
            const int a = edge["ID_first"], b = edge["ID_second"];
            edgePatterns[{std::min(a, b), std::max(a, b)}] = patternLength(edge);
        }
        for (auto id : topology_->ids) {
            const auto& entry = *node(id.getID());
            halfSizes.push_back(halfSize(entry));
        }
        for (auto [a, b] : topology_->edges)
            patterns.push_back(edgePatterns.at({topology_->ids[a].getID(), topology_->ids[b].getID()}));

        // Prefer a symmetric rectangular diagram; the generator's certified
        // planar construction remains a fallback for irregular topologies.
        Points points;
        double bestScore = std::numeric_limits<double>::infinity();
        for (const auto& seed : {rectangularDrawing(*topology_), topology_->gridDrawing}) {
            for (const auto& drawing : {alignRows(seed), seed}) {
                if (drawing.empty() || !planar::validDrawing(drawing, topology_->edges)) continue;
                auto candidate = spacedDrawing(drawing, *topology_, halfSizes, patterns);
                const double score = diagramScore(candidate, topology_->edges);
                if (score < bestScore) { bestScore = score; points = std::move(candidate); }
            }
        }
        if (!points.empty()) {
            auto refined = relaxDiagram(points, *topology_, halfSizes, patterns);
            for (auto& p : refined) { p.x /= grid; p.y /= grid; }
            for (const auto& drawing : {alignRows(refined), refined}) {
                if (!planar::validDrawing(drawing, topology_->edges)) continue;
                auto candidate = spacedDrawing(drawing, *topology_, halfSizes, patterns);
                const double score = diagramScore(candidate, topology_->edges);
                if (score < bestScore) { bestScore = score; points = std::move(candidate); }
            }
        }
        if (points.empty()) {
            error_ = "Could not fit a spaced, grid-aligned layout within the canvas coordinate limits.";
            return false;
        }
        auto candidate = document_;
        std::map<int, size_t> indices;
        for (size_t i = 0; i < topology_->ids.size(); ++i) indices[topology_->ids[i].getID()] = i;
        for (auto& entry : candidate["nodes"]) {
            const auto p = points[indices.at(entry["id"].get<int>())];
            entry["visualizer"]["x"] = p.x;
            entry["visualizer"]["y"] = p.y;
        }
        return replace(std::move(candidate), "Auto place nodes");
    } catch (const std::exception& error) {
        error_ = error.what();
        return false;
    }
}
