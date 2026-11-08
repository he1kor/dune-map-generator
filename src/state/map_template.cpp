#include "map_template.h"
#include "planar_symmetry.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <random>
#include <stdexcept>
#include <utility>
#include <vector>
#include <tuple>

std::shared_ptr<const planar::Reflection> MapTemplate::reflectionForGeneration(const PositionMagnetMap& magnets,
    double width, double height) const {
    if(symmetryMode==SymmetryMode::None)return nullptr;
    auto topology=embeddingTopology;
    if(!topology)topology=planar::makeTopology(*zoneGraph,nodeBoundary);
    std::string mismatch;
    auto accept=[&](const planar::Reflection& reflection) {
        auto mirror=[&](Identifiable id) {
            const auto index=std::lower_bound(topology->ids.begin(),topology->ids.end(),id)-topology->ids.begin();
            return topology->ids[reflection.partner[index]];
        };
        for(auto id:topology->ids) {
            const auto other=mirror(id);
            const auto a=zoneGraph->getValue(id),b=zoneGraph->getValue(other);
            auto resources=[](const auto& node) {
                std::vector<std::tuple<double,double,int>> result;
                for(const auto& row:node.resources)result.emplace_back(row.minimalThreshold,row.maximalThreshold,int(row.resource));
                std::sort(result.begin(),result.end());return result;
            };
            if(a.getRadius()!=b.getRadius()||a.getSusceptibility()!=b.getSusceptibility()
                ||a.octaveWeights!=b.octaveWeights||resources(a)!=resources(b)) {
                mismatch="zones "+std::to_string(id.getID())+" and "+std::to_string(other.getID());return false;
            }
        }
        auto symmetric=[](const auto& edge) {
            return std::tuple{edge.minPassWidth,edge.maxPassWidth,edge.minWallLength,edge.maxWallLength,
                edge.blendDistance,edge.geometry.preferredCenterDistance,edge.geometry.distanceWeight};
        };
        for(const auto& [pair,edge]:zoneGraph->getSymEdges()) {
            const auto other=zoneGraph->tryGetSymEdge(mirror(pair.first),mirror(pair.second));
            if(!other||symmetric(edge)!=symmetric(*other)) { mismatch="paired connection settings";return false; }
        }
        for(const auto& [pair,edge]:zoneGraph->getAsymEdges()) {
            const auto& edges=zoneGraph->getAsymEdges();
            const auto other=edges.find({mirror(pair.first),mirror(pair.second)});
            const auto value=other==edges.end()?BasicAsymConnection{}:other->second;
            if(std::tuple{edge.intakeDistance,edge.areaGuaranteed,edge.bonusValue}
                !=std::tuple{value.intakeDistance,value.areaGuaranteed,value.bonusValue}) {
                mismatch="paired directed connection rules";return false;
            }
        }
        return true;
    };
    // Every symmetry mode uses generation data as graph labels. IDs, canvas
    // positions and target coordinates do not define interchangeable zone types.
    // Face-only mode still permits independent/asymmetric position targets.
    const PositionMagnetMap unconstrained;
    try { return planar::findReflection(*topology,symmetryMode==SymmetryMode::Faces?unconstrained:magnets,
        width,height,symmetryMode==SymmetryMode::Complete,accept); }
    catch(const planar::ConflictingSymmetrySettings&) {
        throw planar::ConflictingSymmetrySettings("Symmetry requires matching zone and connection data. Check "+mismatch+".");
    }
}

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

    if(symmetryMode==SymmetryMode::Complete) {
        // Square maps also admit both diagonal cell-to-cell reflections. Consider
        // rigid rotations aligning every possible constrained magnet axis to
        // that grid, rather than rounding an arbitrary sampled angle.
        std::vector<double> angles;
        auto add=[&](double normalAngle) {
            const int count=width==height?8:4;
            for(int direction=0;direction<count;++direction) {
                double angle=std::fmod(direction*turn/count-normalAngle+turn,turn);
                angles.push_back(angle);
            }
        };
        add(0);
        for(const auto& [id,p]:offsets) {
            if(std::hypot(p.x,p.y)>1e-9)add(std::atan2(p.y,p.x)+std::numbers::pi/2);
            for(const auto& [other,q]:offsets)if(id<other&&std::hypot(p.x-q.x,p.y-q.y)>1e-9)
                add(std::atan2(p.y-q.y,p.x-q.x));
        }
        std::sort(angles.begin(),angles.end());
        angles.erase(std::unique(angles.begin(),angles.end(),[](double a,double b){return std::abs(a-b)<1e-9;}),angles.end());
        std::vector<PositionMagnetMap> fitting;
        std::string settingsError;
        for(double angle:angles)if(fits(angle)) {
            auto candidate=positionMagnets;
            for(const auto& [id,p]:offsets)candidate.at(id).position={
                std::clamp(halfWidth+std::cos(angle)*p.x-std::sin(angle)*p.y,.5,width-.5)/width,
                std::clamp(halfHeight+std::sin(angle)*p.x+std::cos(angle)*p.y,.5,height-.5)/height};
            try { reflectionForGeneration(candidate,width,height);fitting.push_back(std::move(candidate)); }
            catch(const planar::ConflictingSymmetryMagnets&) {}
            catch(const planar::ConflictingSymmetrySettings& error) { settingsError=error.what(); }
        }
        if(fitting.empty()&&!settingsError.empty())throw planar::ConflictingSymmetrySettings(settingsError);
        if(fitting.empty())throw planar::ConflictingSymmetryMagnets("Complete symmetry cannot fit the magnet arrangement on a grid reflection axis (horizontal/vertical, or either diagonal on square maps). Enlarge the magnet frame or move targets closer to its center.");
        std::mt19937_64 random(seed^0x9e3779b97f4a7c15ULL);
        return fitting[static_cast<size_t>((random()>>11)*0x1.0p-53*fitting.size())];
    }

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
