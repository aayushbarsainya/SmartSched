#include "CpuStress.h"
#include <chrono>
#include <atomic>
#include <thread>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif

namespace {
    inline void busyForMs(int ms) {
        using namespace std::chrono;
        auto start = high_resolution_clock::now();
        auto endTime = start + milliseconds(ms);
        volatile std::uint64_t sink = 0;
        while (high_resolution_clock::now() < endTime) {
            // simple CPU work
            sink += 0x9e3779b97f4a7c15ULL;
            sink ^= (sink << 13);
            sink ^= (sink >> 7);
            sink ^= (sink << 17);
        }
    }

#ifdef _WIN32
    DWORD_PTR getAllCoresMask() {
        SYSTEM_INFO si{};
        GetSystemInfo(&si);
        DWORD_PTR mask = 0;
        for (DWORD i = 0; i < si.dwNumberOfProcessors && i < (sizeof(DWORD_PTR) * 8); ++i) {
            mask |= (DWORD_PTR(1) << i);
        }
        return mask ? mask : 1;
    }
#endif
}

namespace CpuStress {
    double runCpuTimeline(const std::vector<TimeSlice>& timeline, int msPerTick, bool singleCore) {
        if (timeline.empty()) return 0.0;
        if (msPerTick <= 0) msPerTick = 50;

#ifdef _WIN32
        HANDLE hProc = GetCurrentProcess();
        DWORD_PTR originalMask = 0;
        DWORD_PTR systemMask = 0;
        GetProcessAffinityMask(hProc, &originalMask, &systemMask);
        if (singleCore) {
            // Pin to CPU 1 (not CPU 0) to avoid contention with system tasks
            // CPU 0 often handles system interrupts and has higher background load
            DWORD_PTR cpu1 = (1ULL << 1);  // Core 1 instead of Core 0
            SetProcessAffinityMask(hProc, cpu1);
        } else {
            // Allow all cores
            SetProcessAffinityMask(hProc, getAllCoresMask());
        }
#endif

        using namespace std::chrono;
        auto t0 = high_resolution_clock::now();

        // Replay each slice as active busy time
        for (const auto& ts : timeline) {
            int durationUnits = ts.endTime - ts.startTime;
            if (durationUnits <= 0) continue;
            int ms = durationUnits * msPerTick;
            busyForMs(ms);
        }

        auto t1 = high_resolution_clock::now();
        duration<double> diff = t1 - t0;

#ifdef _WIN32
        // Restore original affinity
        if (originalMask) {
            SetProcessAffinityMask(hProc, originalMask);
        }
#endif
        return diff.count();
    }

    double runCpuTimeline(const std::vector<TimeSlice>& timeline, int msPerTick, bool singleCore, int perSwitchOverheadMs) {
        if (timeline.empty()) return 0.0;
        if (msPerTick <= 0) msPerTick = 50;
        if (perSwitchOverheadMs < 0) perSwitchOverheadMs = 0;

#ifdef _WIN32
        HANDLE hProc = GetCurrentProcess();
        DWORD_PTR originalMask = 0;
        DWORD_PTR systemMask = 0;
        GetProcessAffinityMask(hProc, &originalMask, &systemMask);
        if (singleCore) {
            // Pin to CPU 1 (not CPU 0) to avoid contention with system tasks
            // CPU 0 often handles system interrupts and has higher background load
            DWORD_PTR cpu1 = (1ULL << 1);  // Core 1 instead of Core 0
            SetProcessAffinityMask(hProc, cpu1);
        } else {
            // Allow all cores
            SetProcessAffinityMask(hProc, getAllCoresMask());
        }
#endif

        using namespace std::chrono;
        auto t0 = high_resolution_clock::now();

        // Replay each slice as active busy time
        int prevPid = timeline.front().pid;
        for (size_t i = 0; i < timeline.size(); ++i) {
            const auto& ts = timeline[i];
            int durationUnits = ts.endTime - ts.startTime;
            if (durationUnits > 0) {
                int ms = durationUnits * msPerTick;
                busyForMs(ms);
            }
            // Apply synthetic context-switch overhead when PID changes to simulate cache/TLB effects
            if ((int)ts.pid != prevPid) {
                if (perSwitchOverheadMs > 0) busyForMs(perSwitchOverheadMs);
            }
            prevPid = ts.pid;
        }

        auto t1 = high_resolution_clock::now();
        duration<double> diff = t1 - t0;

#ifdef _WIN32
        // Restore original affinity
        if (originalMask) {
            SetProcessAffinityMask(hProc, originalMask);
        }
#endif
        return diff.count();
    }

    double runCpuTimeline(const std::vector<TimeSlice>& timeline, int msPerTick, int cpuCore, int workMultiplier) {
        if (timeline.empty()) return 0.0;
        if (msPerTick <= 0) msPerTick = 100;
        if (cpuCore < 0 || cpuCore > 6) cpuCore = 0;
        if (workMultiplier <= 0) workMultiplier = 1;
        if (workMultiplier > 10) workMultiplier = 10;

#ifdef _WIN32
        HANDLE hProc = GetCurrentProcess();
        DWORD_PTR originalMask = 0;
        DWORD_PTR systemMask = 0;
        GetProcessAffinityMask(hProc, &originalMask, &systemMask);
        
        if (cpuCore > 0 && cpuCore <= 6) {
            // Pin to specific core (1-6, not CPU 0)
            DWORD_PTR coreMask = (1ULL << cpuCore);
            SetProcessAffinityMask(hProc, coreMask);
        } else {
            // Allow all cores
            SetProcessAffinityMask(hProc, getAllCoresMask());
        }
#endif

        using namespace std::chrono;
        auto t0 = high_resolution_clock::now();

        // Replay each slice as active busy time, with work multiplier
        for (const auto& ts : timeline) {
            int durationUnits = ts.endTime - ts.startTime;
            if (durationUnits <= 0) continue;
            int ms = durationUnits * msPerTick * workMultiplier;  // Apply multiplier
            busyForMs(ms);
        }

        auto t1 = high_resolution_clock::now();
        duration<double> diff = t1 - t0;

#ifdef _WIN32
        // Restore original affinity
        if (originalMask) {
            SetProcessAffinityMask(hProc, originalMask);
        }
#endif
        return diff.count();
    }
}


