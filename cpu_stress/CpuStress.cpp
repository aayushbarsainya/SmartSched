#include "CpuStress.h"
#include <chrono>
#include <atomic>
#include <thread>
#include <fstream>
#include <sstream>
#include <iostream>

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#else
#include <sched.h>
#include <unistd.h>
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
#else
    void applyLinuxAffinity(int cpuCore, bool singleCore) {
        cpu_set_t set;
        CPU_ZERO(&set);
        long numCores = sysconf(_SC_NPROCESSORS_ONLN);
        if (numCores <= 0) numCores = 1;

        if (cpuCore > 0) {
            CPU_SET(cpuCore % numCores, &set);
        } else if (singleCore) {
            int core = (numCores > 1) ? 1 : 0;
            CPU_SET(core, &set);
        } else {
            for (int i = 0; i < numCores; ++i) {
                CPU_SET(i, &set);
            }
        }
        sched_setaffinity(0, sizeof(cpu_set_t), &set);
    }
#endif
}

namespace CpuStress {

    KernelSchedStats getKernelSchedStats() {
        KernelSchedStats stats;
#ifndef _WIN32
        std::ifstream f("/proc/self/sched");
        if (f.is_open()) {
            std::string line;
            while (std::getline(f, line)) {
                std::istringstream iss(line);
                std::string key, colon;
                if (iss >> key >> colon) {
                    if (key == "se.sum_exec_runtime") {
                        iss >> stats.execRuntimeMs;
                    } else if (key == "nr_switches") {
                        iss >> stats.totalSwitches;
                    } else if (key == "nr_voluntary_switches") {
                        iss >> stats.voluntarySwitches;
                    } else if (key == "nr_involuntary_switches") {
                        iss >> stats.involuntarySwitches;
                    } else if (key == "policy") {
                        iss >> stats.policy;
                    } else if (key == "prio") {
                        iss >> stats.priority;
                    }
                }
            }
            stats.available = true;
        }
#endif
        return stats;
    }

    double runCpuTimeline(const std::vector<TimeSlice>& timeline, int msPerTick, bool singleCore) {
        if (timeline.empty()) return 0.0;
        if (msPerTick <= 0) msPerTick = 50;

#ifdef _WIN32
        HANDLE hProc = GetCurrentProcess();
        DWORD_PTR originalMask = 0;
        DWORD_PTR systemMask = 0;
        GetProcessAffinityMask(hProc, &originalMask, &systemMask);
        if (singleCore) {
            DWORD_PTR cpu1 = (1ULL << 1);
            SetProcessAffinityMask(hProc, cpu1);
        } else {
            SetProcessAffinityMask(hProc, getAllCoresMask());
        }
#else
        cpu_set_t origSet;
        bool hasOrig = (sched_getaffinity(0, sizeof(cpu_set_t), &origSet) == 0);
        applyLinuxAffinity(0, singleCore);
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
        if (originalMask) {
            SetProcessAffinityMask(hProc, originalMask);
        }
#else
        if (hasOrig) {
            sched_setaffinity(0, sizeof(cpu_set_t), &origSet);
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
            DWORD_PTR cpu1 = (1ULL << 1);
            SetProcessAffinityMask(hProc, cpu1);
        } else {
            SetProcessAffinityMask(hProc, getAllCoresMask());
        }
#else
        cpu_set_t origSet;
        bool hasOrig = (sched_getaffinity(0, sizeof(cpu_set_t), &origSet) == 0);
        applyLinuxAffinity(0, singleCore);
#endif

        using namespace std::chrono;
        auto t0 = high_resolution_clock::now();

        int prevPid = timeline.front().pid;
        for (size_t i = 0; i < timeline.size(); ++i) {
            const auto& ts = timeline[i];
            int durationUnits = ts.endTime - ts.startTime;
            if (durationUnits > 0) {
                int ms = durationUnits * msPerTick;
                busyForMs(ms);
            }
            if ((int)ts.pid != prevPid) {
                if (perSwitchOverheadMs > 0) busyForMs(perSwitchOverheadMs);
            }
            prevPid = ts.pid;
        }

        auto t1 = high_resolution_clock::now();
        duration<double> diff = t1 - t0;

#ifdef _WIN32
        if (originalMask) {
            SetProcessAffinityMask(hProc, originalMask);
        }
#else
        if (hasOrig) {
            sched_setaffinity(0, sizeof(cpu_set_t), &origSet);
        }
#endif
        return diff.count();
    }

    double runCpuTimeline(const std::vector<TimeSlice>& timeline, int msPerTick, int cpuCore, int workMultiplier) {
        if (timeline.empty()) return 0.0;
        if (msPerTick <= 0) msPerTick = 100;
        if (cpuCore < 0 || cpuCore > 64) cpuCore = 0;
        if (workMultiplier <= 0) workMultiplier = 1;
        if (workMultiplier > 10) workMultiplier = 10;

#ifdef _WIN32
        HANDLE hProc = GetCurrentProcess();
        DWORD_PTR originalMask = 0;
        DWORD_PTR systemMask = 0;
        GetProcessAffinityMask(hProc, &originalMask, &systemMask);
        
        if (cpuCore > 0) {
            DWORD_PTR coreMask = (1ULL << (cpuCore % 64));
            SetProcessAffinityMask(hProc, coreMask);
        } else {
            SetProcessAffinityMask(hProc, getAllCoresMask());
        }
#else
        cpu_set_t origSet;
        bool hasOrig = (sched_getaffinity(0, sizeof(cpu_set_t), &origSet) == 0);
        applyLinuxAffinity(cpuCore, false);
#endif

        using namespace std::chrono;
        auto t0 = high_resolution_clock::now();

        for (const auto& ts : timeline) {
            int durationUnits = ts.endTime - ts.startTime;
            if (durationUnits <= 0) continue;
            int ms = durationUnits * msPerTick * workMultiplier;
            busyForMs(ms);
        }

        auto t1 = high_resolution_clock::now();
        duration<double> diff = t1 - t0;

#ifdef _WIN32
        if (originalMask) {
            SetProcessAffinityMask(hProc, originalMask);
        }
#else
        if (hasOrig) {
            sched_setaffinity(0, sizeof(cpu_set_t), &origSet);
        }
#endif
        return diff.count();
    }

    void setThreadAffinity(int cpuCore) {
#ifdef _WIN32
        HANDLE hThread = GetCurrentThread();
        if (cpuCore > 0) {
            DWORD_PTR mask = (1ULL << (cpuCore % 64));
            SetThreadAffinityMask(hThread, mask);
        } else {
            SetThreadAffinityMask(hThread, getAllCoresMask());
        }
#else
        applyLinuxAffinity(cpuCore, false);
#endif
    }
}
