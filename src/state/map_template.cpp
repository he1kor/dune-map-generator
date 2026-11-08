#include "map_template.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <random>
#include <stdexcept>
#include <utility>
#include <vector>

PositionMagnetMap MapTemplate::magnetsForGeneration(std::uint64_t seed, int width, int height) const {
    if (!rotatePositionMagnets || positionMagnets.empty()) return positionMagnets;
    if (width < 2 || height < 2) throw std::invalid_argument("Magnet rotation needs map bounds at least 2 cells wide/high");

    // Rotate in map cells, so rectangles also preserve distances. Match the
    // embedding's half-cell inset for targets on the exact map boundary.
    const double halfWidth = width / 2.0, halfHeight = height / 2.0;
    const double limitX = halfWidth - 0.5, limitY = halfHeight - 0.5;
    constexpr double turn = 2 * std::numbers::pi;
    std::vector<std::pair<Identifiable, DoubleVector2>> offsets;
    std::vector<double> cuts{0, turn};
    auto addCrossings = [&](double radius, double phase, double limit) {
        if (radius <= limit) return;
        const double alpha = std::acos(limit / radius);
        for (double boundary : {alpha, -alpha, std::numbers::pi + alpha, std::numbers::pi - alpha}) {
            double angle = std::fmod(boundary - phase, turn);
            if (angle < 0) angle += turn;
            cuts.push_back(angle);
        }
    };
    for (const auto& [id, magnet] : positionMagnets) {
        const auto p = magnet.position;
        // Outside targets stay inactive, even if a rotation would bring them in.
        if (!(p.x >= 0 && p.x <= 1 && p.y >= 0 && p.y <= 1)) continue;
        const DoubleVector2 offset{std::clamp(p.x * width, 0.5, width - 0.5) - halfWidth,
            std::clamp(p.y * height, 0.5, height - 0.5) - halfHeight};
        offsets.emplace_back(id, offset);
        const double radius = std::hypot(offset.x, offset.y), phase = std::atan2(offset.y, offset.x);
        addCrossings(radius, phase, limitX);
        addCrossings(radius, phase - std::numbers::pi / 2, limitY);
    }
    if (offsets.empty()) return positionMagnets;
    auto fits = [&](double angle) {
        const double cosine = std::cos(angle), sine = std::sin(angle);
        for (const auto& [id, p] : offsets)
            if (std::abs(cosine * p.x - sine * p.y) > limitX + 1e-9
                || std::abs(sine * p.x + cosine * p.y) > limitY + 1e-9) return false;
        return true;
    };

    // Every boundary crossing splits the circle into intervals on which either
    // the entire group fits or it does not. Sample the fitting angular lengths,
    // rather than clipping/scaling individual targets or retrying indefinitely.
    std::sort(cuts.begin(), cuts.end());
    cuts.erase(std::unique(cuts.begin(), cuts.end(), [](double a, double b) { return std::abs(a - b) < 1e-12; }), cuts.end());
    std::vector<std::pair<double, double>> intervals;
    double total = 0;
    for (size_t i = 1; i < cuts.size(); ++i) if (fits((cuts[i - 1] + cuts[i]) / 2)) {
        intervals.emplace_back(cuts[i - 1], cuts[i]);
        total += cuts[i] - cuts[i - 1];
    }
    // A separate stream must not consume the growth/resource generator's RNG.
    std::mt19937_64 random(seed ^ 0x9e3779b97f4a7c15ULL);
    const double sample = (random() >> 11) * 0x1.0p-53;
    double angle = 0;
    if (total > 1e-12) {
        double remaining = sample * total;
        for (auto [start, end] : intervals) {
            angle = start + std::min(remaining, end - start);
            if (remaining < end - start) break;
            remaining -= end - start;
        }
    } else {
        // Exact corner arrangements can have only discrete fitting rotations.
        std::vector<double> fitting;
        for (double cut : cuts) if (cut < turn - 1e-12 && fits(cut)) fitting.push_back(cut);
        if (!fitting.empty()) angle = fitting[static_cast<size_t>(sample * fitting.size())];
    }
    auto result = positionMagnets;
    const double cosine = std::cos(angle), sine = std::sin(angle);
    for (const auto& [id, p] : offsets)
        result.at(id).position = {
            std::clamp(halfWidth + cosine * p.x - sine * p.y, 0.5, width - 0.5) / width,
            std::clamp(halfHeight + sine * p.x + cosine * p.y, 0.5, height - 0.5) / height};
    return result;
}
