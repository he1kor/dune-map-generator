#include "generation.h"
#include "template_picker.h"
#include <chrono>
#include <fstream>
#include <iostream>
#include <set>
#include <stdexcept>
#include <thread>

using Clock = std::chrono::steady_clock;
using namespace std::chrono_literals;

static void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

static std::vector<int> mapData(const Generation& generation) {
    std::vector<int> data;
    for (int y = 0; y < generation.spiceMap.getHeight(); ++y) {
        for (int x = 0; x < generation.spiceMap.getWidth(); ++x) {
            data.push_back(generation.grid->getTileID(x, y).getID());
            data.push_back(static_cast<int>(generation.spiceMap.get(x, y)));
            data.push_back(generation.wallMask.get(x, y));
        }
    }
    return data;
}

static void finish(Generation& generation, bool expectPreviews = false) {
    const auto deadline = Clock::now() + 15s;
    float previousProgress = 0.0f;
    while (generation.isRunning()) {
        require(Clock::now() < deadline, "Background generation did not finish");
        generation.poll();
        require(generation.generationProgress >= previousProgress && generation.generationProgress <= 1.0f,
            "Progress moved backwards or exceeded 100%");
        previousProgress = generation.generationProgress;
        if (!expectPreviews) require(!generation.takePreview(), "Animation off still published a preview");
        std::this_thread::sleep_for(2ms);
    }
}

int main(int argc, char** argv) {
    try {
        require(argc == 2, "Pass a template path");
        nlohmann::json json;
        std::ifstream(argv[1]) >> json;
        TemplatePicker picker;
        require(picker.tryReadEdgeGraph(json), "Template was rejected");
        auto mapTemplate = picker.mapTemplate;

        Generation baseline;
        baseline.isSeedRandom = false;
        baseline.generate(mapTemplate, 64, 96);
        std::set<GenerationStage> stages;
        float previousProgress = 0.0f;
        baseline.runIteration([&](const Generation& state) {
            require(state.generationProgress >= previousProgress && state.generationProgress < 1.0f,
                "Synchronous progress is invalid");
            previousProgress = state.generationProgress;
            stages.insert(state.generationStage);
            return true;
        });
        require(stages.size() == 8 && baseline.generationProgress == 1.0f, "A generation phase was not reported");
        const auto expected = mapData(baseline);

        Generation generation;
        generation.isSeedRandom = false;
        require(!generation.isAnimationEnabled(), "Animation should be opt-in");
        require(generation.startAsync(mapTemplate, 64, 96), "Could not launch worker");
        require(!generation.startAsync(mapTemplate, 64, 96), "Overlapping generation was allowed");
        finish(generation);
        require(generation.is_done && generation.generationError.empty() && mapData(generation) == expected,
            "Background generation differs from synchronous generation");

        const auto previousGrid = generation.grid;
        generation.setAnimate(true);
        require(generation.startAsync(mapTemplate, 64, 96), "Could not restart worker");
        std::unique_ptr<GenerationPreview> heldFrame;
        std::vector<DoubleVector2> heldNodes;
        int layoutFrames = 0, zoneFrames = 0;
        const auto started = Clock::now();
        previousProgress = 0.0f;
        while (generation.isRunning()) {
            require(Clock::now() - started < 15s, "Animated generation did not finish");
            generation.poll();
            require(generation.generationProgress >= previousProgress, "Animated progress moved backwards");
            previousProgress = generation.generationProgress;
            if (generation.isRunning()) require(generation.grid == previousGrid, "Worker mutated the UI's completed grid");
            if (auto frame = generation.takePreview()) {
                require(frame->width == 64 && frame->height == 96, "Preview has incorrect dimensions");
                if (frame->zones.empty()) {
                    ++layoutFrames;
                    require(!frame->nodes.empty() && !frame->edges.empty(), "Layout preview is empty");
                    if (!heldFrame) {
                        heldNodes = frame->nodes;
                        heldFrame = std::move(frame);
                    }
                } else {
                    ++zoneFrames;
                    require(frame->zones.size() == 64 * 96, "Zone preview is incomplete");
                }
            }
            std::this_thread::sleep_for(2ms);
        }
        require(layoutFrames > 2 && zoneFrames > 2, "Layout or zone expansion was not animated");
        require(Clock::now() - started >= (layoutFrames + zoneFrames - 2) * 25ms,
            "Preview publishing exceeded the animation frame cap");
        for (size_t i = 0; i < heldNodes.size(); ++i)
            require(heldFrame->nodes[i].x == heldNodes[i].x && heldFrame->nodes[i].y == heldNodes[i].y,
                "Worker changed an already published snapshot");
        require(generation.is_done && mapData(generation) == expected, "Animation changed seeded output");

        require(generation.startAsync(mapTemplate, 64, 96), "Could not launch toggle run");
        const auto toggleDeadline = Clock::now() + 5s;
        bool sawPreview = false;
        while (!sawPreview) {
            require(Clock::now() < toggleDeadline, "No preview before disabling animation");
            generation.poll();
            sawPreview = static_cast<bool>(generation.takePreview());
            std::this_thread::sleep_for(2ms);
        }
        generation.setAnimate(false);
        finish(generation);
        require(generation.is_done && mapData(generation) == expected, "Disabling animation broke generation");

        const auto completedGrid = generation.grid;
        auto invalid = std::make_shared<MapTemplate>(*mapTemplate);
        invalid->octaves.push_back({1.0, 2.0, 3.0}); // Deliberate noise/weight count mismatch inside worker.
        require(generation.startAsync(invalid, 64, 96), "Could not launch failure case");
        finish(generation);
        require(!generation.is_done && !generation.generationError.empty() && generation.grid == completedGrid,
            "Worker error was not reported or damaged the previous result");
        require(generation.startAsync(mapTemplate, 64, 96), "Could not recover after worker error");
        finish(generation);
        require(generation.is_done && generation.generationError.empty() && mapData(generation) == expected,
            "Generation did not recover after worker error");

        {
            Generation closing;
            closing.setAnimate(true);
            require(closing.startAsync(mapTemplate, 128, 128), "Could not launch shutdown case");
            // Destruction must cancel and join even while the worker is pacing a preview.
            std::this_thread::sleep_for(50ms);
        }
        // Exercise the synchronous API again after accepting background buffers.
        generation.generate(mapTemplate, 64, 96);
        generation.runIteration();
        require(mapData(generation) == expected, "Async completion broke synchronous reuse");
        std::cout << "Async/animated output identical; " << layoutFrames << " layout and " << zoneFrames
            << " zone frames; progress, snapshots, toggle, restart, error recovery and shutdown passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
