// sample/pattern_control.cpp

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <iostream>
#include <limits>
#include <string>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

namespace fs = std::filesystem;

// --- Furuno includes ---
// Adjust include paths as needed for your build system!
namespace Furuno {
#include "nndll.h"
#include <lasterror.h>
}

// If you want to refer to lastSweepAngle, declare it extern
extern Furuno::INT16 lastSweepAngle;

struct RangeGainStep {
    uint16_t range;
    uint16_t gain;
    uint16_t repeat;  // number of sweeps at this setting
};

struct ControlStep {
    uint16_t range;
    uint16_t gain;
    uint16_t pulse_length;     // S1, S2, M1, M2, M3, L 
    uint16_t rezBoost;         // 0, 1, 2, 3
    uint16_t rotation_speed;   // Lo, Hi, Auto
    uint16_t interpolation;    // 2048, 4096, 8192, 1024, 720, 5=OFF
    uint16_t repeat;           // number of sweeps at this setting
};

std::atomic<bool> patternActive{false};

// Returns a unique directory name based on baseDir + suffix.
static std::string getUniqueDirName(const std::string& baseDir, const std::string& suffix) {
    int idx = 0;
    std::string candidate;
    do {
        candidate = baseDir + suffix + (idx > 0 ? std::to_string(idx) : "");
        ++idx;
    } while (fs::exists(candidate));
    return candidate;
}

// Extracted helper to rename the two output folders.
static void renameOutputFolders(size_t patternSize,
                                size_t threshold,
                                const std::string& framesDir,
                                const std::string& dataDir,
                                bool recreateDirs) {
    if (patternSize == 0) {
        return;
    }

    // choose suffix based on size
    const std::string suffix = (patternSize > threshold)
                               ? "_calibration"
                               : "_pattern";

    // build unique new names
    std::string newFrames = getUniqueDirName(framesDir, suffix);
    std::string newData   = getUniqueDirName(dataDir,   suffix);

    // perform the renames (if the originals exist)
    if (fs::exists(framesDir)) {
        fs::rename(framesDir, newFrames);
        std::printf("[PatternControl] Renamed '%s' → '%s'\n",
                    framesDir.c_str(), newFrames.c_str());
    } else {
        std::printf("[PatternControl] Frames directory '%s' does not exist.\n",
                    framesDir.c_str());
    }

    if (fs::exists(dataDir)) {
        fs::rename(dataDir, newData);
        std::printf("[PatternControl] Renamed '%s' → '%s'\n",
                    dataDir.c_str(), newData.c_str());
    } else {
        std::printf("[PatternControl] Data directory '%s' does not exist.\n",
                    dataDir.c_str());
    }

    if (recreateDirs) {
        std::error_code ec;
        if (!fs::exists(framesDir)) {
            fs::create_directories(framesDir, ec);
            if (ec) {
                std::printf("[PatternControl] Failed to recreate '%s': %s\n",
                            framesDir.c_str(), ec.message().c_str());
            }
        }
        ec.clear();
        if (!fs::exists(dataDir)) {
            fs::create_directories(dataDir, ec);
            if (ec) {
                std::printf("[PatternControl] Failed to recreate '%s': %s\n",
                            dataDir.c_str(), ec.message().c_str());
            }
        }
    }
}

void RunRangeGainPattern(const std::vector<RangeGainStep>& pattern, int numRepeats /* = -1 */) {
    patternActive = true;
    int totalRepeats = (numRepeats == -1) ? INT32_MAX : numRepeats;

    uint16_t lastRange = std::numeric_limits<uint16_t>::max();
    uint16_t lastGain  = std::numeric_limits<uint16_t>::max();

    constexpr int radarNo = 0;         // Default radar. If you want to set via web, add to RangeGainStep
    constexpr uint16_t rangeUnit = 0;  // 0 = NM. You can change as needed.
    constexpr uint16_t gainMode = 0;   // 0 = MANUAL
    constexpr uint16_t autoData = 10;  // only used if gainMode=1

    for (int repeat = 0; repeat < totalRepeats && patternActive; ++repeat) {
        for (size_t i = 0; i < pattern.size() && patternActive; ++i) {
            const auto& step = pattern[i];

            const bool needRange = (step.range != lastRange);
            const bool needGain  = (step.gain  != lastGain);

            if (needRange) {
                int ret = Furuno::RmcSetRange(radarNo, step.range, rangeUnit);
                std::printf("[PatternControl] Set Range: %d (ret=%d)\n", step.range, ret);
                lastRange = step.range;
            }
            if (needGain) {
                int ret = Furuno::RmcSetGAIN(radarNo, gainMode, step.gain, autoData);
                std::printf("[PatternControl] Set Gain: %d (ret=%d)\n", step.gain, ret);
                lastGain = step.gain;
            }

            int sweeps = 0;
            int prevAngle = lastSweepAngle;

            while (sweeps < step.repeat && patternActive) {
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
                int currAngle = lastSweepAngle;

                // Detect wrap-around: previous angle near max, current near 0
                if ((prevAngle > 6000) && (currAngle < 1000)) {
                    ++sweeps;
                    std::printf("Sweep complete (%d/%d)\n", sweeps, step.repeat);
                }
                prevAngle = currAngle;
            }
        }
    }
    patternActive = false;

    Furuno::RmcSetTxStby(radarNo, 1, 0, 0, 0); // Set STBY mode

    constexpr size_t PATTERN_CALIBRATION_THRESHOLD = 2000; // decided based on 100 gains, 5 ranges (500 patterns)
    renameOutputFolders(pattern.size(),
                        PATTERN_CALIBRATION_THRESHOLD,
                        /* framesDir */ "frames",
                        /* dataDir   */ "data",
                        /* recreateDirs */ false);
}

namespace {

enum class PatternRunResult { Completed, Stopped };

PatternRunResult ExecuteControlPattern(const std::vector<ControlStep>& pattern,
                                       int numRepeats) {
    if (pattern.empty()) {
        return PatternRunResult::Completed;
    }

    uint16_t lastRange  = std::numeric_limits<uint16_t>::max();
    uint16_t lastGain   = std::numeric_limits<uint16_t>::max();
    uint16_t lastPulse  = std::numeric_limits<uint16_t>::max();
    uint16_t lastRez    = std::numeric_limits<uint16_t>::max();
    uint16_t lastRot    = std::numeric_limits<uint16_t>::max();
    uint16_t lastInterp = std::numeric_limits<uint16_t>::max();

    constexpr int radarNo = 0;
    constexpr uint16_t rangeUnit = 0;
    constexpr uint16_t gainMode = 0;
    constexpr uint16_t autoData = 10;

    const int totalRepeats = (numRepeats == -1) ? INT32_MAX : numRepeats;

    for (int repeat = 0; repeat < totalRepeats && patternActive; ++repeat) {
        for (size_t i = 0; i < pattern.size() && patternActive; ++i) {
            const auto& step = pattern[i];

            const bool needRange  = (step.range         != lastRange);
            const bool needGain   = (step.gain          != lastGain);
            const bool needPulse  = (step.pulse_length  != lastPulse);
            const bool needRez    = (step.rezBoost      != lastRez);
            const bool needRot    = (step.rotation_speed!= lastRot);
            const bool needInterp = (step.interpolation != lastInterp);

            if (needRange) {
                int ret = Furuno::RmcSetRange(radarNo, step.range, rangeUnit);
                std::printf("[PatternControl] Set Range: %d (ret=%d)\n", step.range, ret);
                lastRange = step.range;
            }
            if (needGain) {
                int ret = Furuno::RmcSetGAIN(radarNo, gainMode, step.gain, autoData);
                std::printf("[PatternControl] Set Gain: %d (ret=%d)\n", step.gain, ret);
                lastGain = step.gain;
            }
            if (needPulse) {
                Furuno::RmcSetPulseWidth(radarNo,
                                         step.pulse_length,
                                         step.range,
                                         rangeUnit,
                                         0);
                lastPulse = step.pulse_length;
            }
            if (needRez) {
                Furuno::RmcSetBearingResolutionSelect(radarNo, step.rezBoost);
                lastRez = step.rezBoost;
            }
            if (needRot) {
                const uint16_t highMode = (step.rotation_speed == 1) ? 1 : 0;
                Furuno::RmcSetAntennaRevolution(radarNo, step.rotation_speed, highMode);
                lastRot = step.rotation_speed;
            }
            if (needInterp) {
                if (step.interpolation == 5) {
                    Furuno::RmSetEchoInterpolationFunc(0); // OFF
                } else {
                    Furuno::RmSetEchoInterpolationFunc(2); // ON (auto)
                    Furuno::RmSetEchoSweepTotal(step.interpolation);
                }
                lastInterp = step.interpolation;
            }

            int sweeps = 0;
            int prevAngle = lastSweepAngle;
            while (sweeps < step.repeat && patternActive) {
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
                int currAngle = lastSweepAngle;
                if ((prevAngle > 5000) && (currAngle < 3000)) {
                    ++sweeps;
                    std::printf("Sweep complete (%d/%d)\n", sweeps, step.repeat);
                }
                prevAngle = currAngle;
            }
        }
    }

    return patternActive ? PatternRunResult::Completed
                         : PatternRunResult::Stopped;
}

void FinalizeControlPattern(size_t patternSize,
                            bool driveToStandby,
                            bool recreateDirs) {
    constexpr int radarNo = 0;

    if (driveToStandby) {
        Furuno::RmcSetTxStby(radarNo, 1, 0, 0, 0);
    }

    constexpr size_t PATTERN_CALIBRATION_THRESHOLD = 2000;
    renameOutputFolders(patternSize,
                        PATTERN_CALIBRATION_THRESHOLD,
                        "frames",
                        "data",
                        recreateDirs);

    patternActive = false;
}

} // namespace

void RunPattern(const std::vector<ControlStep>& pattern, int numRepeats /* = -1 */) {
    patternActive = true;
    ExecuteControlPattern(pattern, numRepeats);
    FinalizeControlPattern(pattern.size(),
                           /*driveToStandby=*/true,
                           /*recreateDirs=*/false);
}

void StopPattern() {
    patternActive = false;
}

void RunPatternSequence(const std::vector<std::pair<std::vector<ControlStep>, int>>& sequence) {
    if (sequence.empty()) {
        return;
    }

    bool continueRunning = true;
    for (size_t idx = 0; idx < sequence.size() && continueRunning; ++idx) {
        const bool isLast = (idx + 1 == sequence.size());
        const auto& [steps, repeats] = sequence[idx];

        patternActive = true;
        const PatternRunResult result = ExecuteControlPattern(steps, repeats);
        const bool stopped = (result == PatternRunResult::Stopped);

        const bool driveToStandby = stopped || isLast;
        const bool recreateDirs = !driveToStandby;
        FinalizeControlPattern(steps.size(), driveToStandby, recreateDirs);

        continueRunning = !stopped;
    }
}

// Pattern generator helper
std::vector<RangeGainStep> GenerateRampPattern(const std::vector<uint16_t>& ranges,
                                               uint16_t gainStart,
                                               uint16_t gainEnd,
                                               uint16_t repeatCount) {
    std::vector<RangeGainStep> pattern;
    for (auto range : ranges) {
        for (uint16_t gain = gainStart; gain <= gainEnd; ++gain) {
            pattern.push_back(RangeGainStep{range, gain, repeatCount});
        }
    }
    std::cout << "[PatternControl] Generated ramp pattern with "
              << pattern.size() << " steps.\n";
    return pattern;
}

// Updated pattern generator for added functions
std::vector<ControlStep> GeneratePattern(const std::vector<uint16_t>& ranges,
                                         const std::vector<uint16_t>& pulse_lengths,
                                         const std::vector<uint16_t>& rezBoosts,
                                         const std::vector<uint16_t>& rotation_speeds,
                                         const std::vector<uint16_t>& interpolations,
                                         uint16_t gainStart,
                                         uint16_t gainEnd,
                                         uint16_t repeatCount) {
    std::vector<ControlStep> pattern;
    for (auto range : ranges) {
        for (auto pulse : pulse_lengths) {
            for (auto rez : rezBoosts) {
                for (auto rot : rotation_speeds) {
                    for (auto interp : interpolations) {
                        for (uint16_t gain = gainStart; gain <= gainEnd; ++gain) {
                            pattern.push_back(ControlStep{
                                range, gain, pulse, rez, rot, interp, repeatCount
                            });
                        }
                    }
                }
            }
        }
    }
    std::cout << "[PatternControl] Generated ramp pattern with "
              << pattern.size() << " steps.\n";
    return pattern;
}
