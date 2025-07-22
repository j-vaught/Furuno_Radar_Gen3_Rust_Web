// sample/pattern_control.cpp

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

// Add new params to struct
struct ControlStep {
    uint16_t range;
    uint16_t gain;
    uint16_t pulse_length;     // S1, S2, M1, M2, M3, L 
    uint16_t rezBoost;         // 0, 1, 2, 3
    uint16_t rotation_speed;   // Lo, Hi, Auto
    uint16_t interpolation;    // 2048, 4096, 8192, 1024, 720
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
static void renameOutputFolders(size_t patternSize, size_t threshold, const std::string& framesDir, const std::string& dataDir) {
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
}

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

    constexpr size_t PATTERN_CALIBRATION_THRESHOLD = 2000; // decided based on 100 gains, 5 ranges (500 patterns)

    renameOutputFolders(pattern.size(),
                        PATTERN_CALIBRATION_THRESHOLD,
                        /* framesDir */ "frames",
                        /* dataDir   */ "data");

}

// Run Pattern for added parameters
void RunPattern(const std::vector<ControlStep>& pattern, int numRepeats /* = -1 */) {
    patternActive = true;
    int totalRepeats = (numRepeats == -1) ? INT32_MAX : numRepeats;

    uint16_t lastRange = 0xFFFF, lastGain = 0xFFFF, lastPulse = 0xFFFF;
    uint16_t lastRez = 0xFFFF, lastRot = 0xFFFF, lastInterp = 0xFFFF;
    int radarNo = 0;
    uint16_t rangeUnit = 0;
    uint16_t gainMode = 0;
    uint16_t autoData = 10;

    for (int repeat = 0; repeat < totalRepeats && patternActive; ++repeat) {
        for (size_t i = 0; i < pattern.size() && patternActive; ++i) {
            const auto& step = pattern[i];

            bool needRange  = (step.range         != lastRange);
            bool needGain   = (step.gain          != lastGain);
            bool needPulse  = (step.pulse_length  != lastPulse);
            bool needRez    = (step.rezBoost      != lastRez);
            bool needRot    = (step.rotation_speed!= lastRot);
            bool needInterp = (step.interpolation != lastInterp);

            if (needRange) {
                int ret = Furuno::RmcSetRange(radarNo, step.range, rangeUnit);
                printf("[PatternControl] Set Range: %d (ret=%d)\n", step.range, ret);
                lastRange = step.range;
            }
            if (needGain) {
                int ret = Furuno::RmcSetGAIN(radarNo, gainMode, step.gain, autoData);
                printf("[PatternControl] Set Gain: %d (ret=%d)\n", step.gain, ret);
                lastGain = step.gain;
            }
            if (needPulse) {
                // pulse, range, unit, pictureNo need to be filled in from your context
                Furuno::RmcSetPulseWidth(radarNo,
                                        step.pulse_length,         // 0-5   (S1…L)
                                        step.range,         // 0-(RMC_RANGE_MAX-1)
                                        rangeUnit,          // 0=NM,1=KM,2=SM,3=KYD
                                        0);
                lastPulse = step.pulse_length;
            }

            if (needRez) {
                // 0-3 map to OFF / LOW / MID / HIGH (RezBoost)
                Furuno::RmcSetBearingResolutionSelect(radarNo, step.rezBoost);
                lastRez = step.rezBoost;
            }

            if (needRot) {
                // highRotationMode = 0 (normal) / 1 (fast) on most sensors
                // step.rotation_speed = 0 (Lo) / 1 (Hi) / 2 (Auto)
                uint16_t highMode = (step.rotation_speed == 1) ? 1 : 0;
                Furuno::RmcSetAntennaRevolution(radarNo, step.rotation_speed, highMode);
                lastRot = step.rotation_speed;
            }

            if (needInterp) {
                // inputs: 5(0ff), 0-4 (interpolation codes)
                if (step.interpolation == 5) {
                    Furuno::RmSetEchoInterpolationFunc(0); // OFF
                } else {
                    Furuno::RmSetEchoInterpolationFunc(2); // ON (auto)... status: 0 = OFF, 1 = ON only when you are expanding, 2 = ON always (even when totals match)
                    Furuno::RmSetEchoSweepTotal(step.interpolation); // code: 0 = 2 048, 1 = 4 096, 2 = 8 192, 3 = 1 024, 4 = 720
                } 
                lastInterp = step.interpolation;
            }


            // Repeat sweeps logic (unchanged)
            int sweeps = 0;
            int prevAngle = lastSweepAngle;
            while (sweeps < step.repeat && patternActive) {
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
                int currAngle = lastSweepAngle;
                if ((prevAngle > 5000) && (currAngle < 3000)) {
                    ++sweeps;
                    printf("Sweep complete (%d/%d)\n", sweeps, step.repeat);
                }
                prevAngle = currAngle;
            }
        }
    }
    patternActive = false;
    int ret = Furuno::RmcSetTxStby(radarNo, 1, 0, 0, 0);

    constexpr size_t PATTERN_CALIBRATION_THRESHOLD = 2000;
    renameOutputFolders(pattern.size(), PATTERN_CALIBRATION_THRESHOLD, "frames", "data");
}

void StopPattern() {
    patternActive = false;
}

// Pattern generator helper
std::vector<RangeGainStep> GenerateRampPattern(const std::vector<uint16_t>& ranges, uint16_t gainStart, uint16_t gainEnd, uint16_t repeatCount) {
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

// Updated pattern generator for added functions
std::vector<ControlStep> GeneratePattern(
    const std::vector<uint16_t>& ranges,
    const std::vector<uint16_t>& pulse_lengths,
    const std::vector<uint16_t>& rezBoosts,
    const std::vector<uint16_t>& rotation_speeds,
    const std::vector<uint16_t>& interpolations,
    uint16_t gainStart,
    uint16_t gainEnd,
    uint16_t repeatCount
) {
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
    std::cout << "[PatternControl] Generated ramp pattern with " << pattern.size() << " steps.\n";
    return pattern;
}



