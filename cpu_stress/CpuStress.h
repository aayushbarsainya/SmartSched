#pragma once
#include <vector>
#include <string>
#include "../Scheduler.h"

namespace CpuStress {
    struct KernelSchedStats {
        double execRuntimeMs = 0.0;
        long totalSwitches = 0;
        long voluntarySwitches = 0;
        long involuntarySwitches = 0;
        int policy = 0;
        int priority = 0;
        bool available = false;
    };

    // Reads real Linux kernel scheduler statistics from /proc/self/sched (if available on Linux/WSL2)
    KernelSchedStats getKernelSchedStats();

    // Replays a timeline as CPU load. Each time unit becomes msPerTick milliseconds.
    // If singleCore==true, process affinity is set to one core during the run.
    // Returns total wall time in seconds for the replay.
    double runCpuTimeline(const std::vector<TimeSlice>& timeline, int msPerTick, bool singleCore);

    // Overload with synthetic per-switch overhead (in milliseconds) applied between adjacent slices.
    // This helps quantify differences between schedulers with many context switches.
    double runCpuTimeline(const std::vector<TimeSlice>& timeline, int msPerTick, bool singleCore, int perSwitchOverheadMs);
    
    // Overload with specific CPU core selection and work multiplier
    // cpuCore: 0=all-cores, 1-6=pin to specific core (ignore CPU 0)
    // workMultiplier: multiply the duration by this factor (1=normal, 2=double, etc)
    double runCpuTimeline(const std::vector<TimeSlice>& timeline, int msPerTick, int cpuCore, int workMultiplier);

    // Set thread affinity to specific core (0 = all cores, >0 = specific core)
    void setThreadAffinity(int cpuCore);
}
