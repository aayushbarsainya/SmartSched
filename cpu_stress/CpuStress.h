#pragma once
#include <vector>
#include "../Scheduler.h"

namespace CpuStress {
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
}


