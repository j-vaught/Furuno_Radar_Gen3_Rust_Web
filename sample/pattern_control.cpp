#include <vector>
#include <atomic>
#include <thread>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <iostream>
namespace fs = std::filesystem;

// --- Furuno includes ---
// Adjust include paths as needed for your build system!
namespace Furuno {
    #include "nndll.h"
    #include <lasterror.h>
}

// If you want to refer to lastSweepAngle, declare it extern
extern Furuno::INT16 lastSweepAngle;

// Struct and global flag
struct RangeGainStep {
    uint16_t range;
    uint16_t gain;
    uint16_t repeat;  // number of sweeps at this setting
};

std::atomic<bool> patternActive{false};

void RunRangeGainPattern(const std::vector<RangeGainStep>& pattern, int numRepeats /* = -1 */) {
    patternActive = true;
    int totalRepeats = (numRepeats == -1) ? INT32_MAX : numRepeats;

    uint16_t lastRange = 0xFFFF, lastGain = 0xFFFF;
    int radarNo = 0;         // Default radar. If you want to set via web, add to RangeGainStep
    uint16_t rangeUnit = 0;  // 0 = NM. You can change as needed.
    uint16_t gainMode = 0;   // 0 = MANUAL
    uint16_t autoData = 10;  // only used if gainMode=1

    for (int repeat = 0; repeat < totalRepeats && patternActive; ++repeat) {
        for (size_t i = 0; i < pattern.size() && patternActive; ++i) {
            const auto& step = pattern[i];

            bool needRange = (step.range != lastRange);
            bool needGain  = (step.gain  != lastGain);

            // Only change range if needed
            if (needRange) {
                int ret = Furuno::RmcSetRange(radarNo, step.range, rangeUnit);
                printf("[PatternControl] Set Range: %d (ret=%d)\n", step.range, ret);
                lastRange = step.range;
            }
            // Only change gain if needed
            if (needGain) {
                int ret = Furuno::RmcSetGAIN(radarNo, gainMode, step.gain, autoData);
                printf("[PatternControl] Set Gain: %d (ret=%d)\n", step.gain, ret);
                lastGain = step.gain;
            }

            // Now collect N sweeps at this value
            int sweeps = 0;
            int prevAngle = lastSweepAngle;

            while (sweeps < step.repeat && patternActive) {
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
                int currAngle = lastSweepAngle;

                // Detect wrap-around: previous angle near max, current near 0
                if ((prevAngle > 6000) && (currAngle < 1000)) {
                    ++sweeps;
                    printf("Sweep complete (%d/%d)\n", sweeps, step.repeat); // Optional debug
                }
                prevAngle = currAngle;
            }
        }
    }
    patternActive = false;

    // furuno put to STBY mode
    int ret = Furuno::RmcSetTxStby(radarNo, 1, 0, 0, 0); // Set STBY mode

    // Rename 'frames' and 'data' directories after pattern run.
    // If pattern.size() > PATTERN_CALIBRATION_THRESHOLD, append "_calibration" (with increment if needed).
    // Otherwise, append "_pattern" (with increment if needed).

    constexpr size_t PATTERN_CALIBRATION_THRESHOLD = 450; // decided based on 100 gains, 5 ranges (500 patterns)

    std::string suffix = (pattern.size() > PATTERN_CALIBRATION_THRESHOLD) ? "_calibration" : "_pattern";
    std::string framesDir = "frames";
    std::string dataDir = "data";
    int suffixIndex = 0;

    // Helper lambda to generate a unique directory name
    auto getUniqueDirName = [](const std::string& baseDir, const std::string& suffix) -> std::string {
        int suffixIndex = 0;
        std::string newDir;
        do {
            newDir = baseDir + suffix + (suffixIndex > 0 ? std::to_string(suffixIndex) : "");
            ++suffixIndex;
        } while (fs::exists(newDir));
        return newDir;
    };

    std::string newFramesDir = getUniqueDirName(framesDir, suffix);
    std::string newDataDir = getUniqueDirName(dataDir, suffix);

    // Rename directories
    if (fs::exists(framesDir)) {
        fs::rename(framesDir, newFramesDir);
        printf("[PatternControl] Renamed '%s' to '%s'\n", framesDir.c_str(), newFramesDir.c_str());
    } else {
        printf("[PatternControl] Frames directory '%s' does not exist.\n", framesDir.c_str());
    }

    if (fs::exists(dataDir)) {
        fs::rename(dataDir, newDataDir);
        printf("[PatternControl] Renamed '%s' to '%s'\n", dataDir.c_str(), newDataDir.c_str());
    } else {
        printf("[PatternControl] Data directory '%s' does not exist.\n", dataDir.c_str());
    }
    
    printf("[PatternControl] Pattern done (repeats=%d)\n", numRepeats);

}

void StopPattern() {
    patternActive = false;
}


// --- New-style (int-args) ramp pattern builder ---
static std::vector<RangeGainStep> GenerateRampPattern(const std::vector<uint16_t>& ranges, uint16_t gainStart, uint16_t gainEnd, uint16_t repeatCount) {
    std::vector<RangeGainStep> pattern;
    for (auto range : ranges) {
        for (uint16_t gain = gainStart; gain <= gainEnd; ++gain) {
            pattern.push_back(RangeGainStep{range, gain, repeatCount});
        }
    }
    // add a print here so i can see if it made it here
    std::cout << "[PatternControl] Generated ramp pattern with " << pattern.size() << " steps.\n";
    return pattern;
}

void RunRangeGainResolutionRampPattern(const std::vector<uint16_t>& ranges, uint16_t gainStart, uint16_t gainEnd, uint16_t repeatCount) {
    constexpr int radarNo = 0;
    Furuno::RmSetEchoSweepTotal(2);
    Furuno::RmcSetACSEA(0,0,0,0,0);
    Furuno::RmcSetACRAIN(0,0,0,0,0,0);

    auto pattern = GenerateRampPattern(ranges, gainStart, gainEnd, repeatCount);

    for (int rezBoost = 0; rezBoost <= 3; ++rezBoost) {
        Furuno::RmSetEchoInterpolationFunc(1);
        Furuno::RmcSetBearingResolutionSelect(radarNo, static_cast<uint16_t>(rezBoost));
        Furuno::RmcSetTxStby(radarNo, 2,0,0,0); // TX
        RunRangeGainPattern(pattern, 1);

        Furuno::RmSetEchoInterpolationFunc(0);
        Furuno::RmcSetTxStby(radarNo, 2,0,0,0); // TX
        RunRangeGainPattern(pattern, 1);
    }
    Furuno::RmcSetTxStby(radarNo, 1,0,0,0); // STBY
    std::puts("[Done] Calibration [rezboost, range, gain, interp]");
}


