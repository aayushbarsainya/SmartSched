#pragma once

#include <vector>
#include "Process.h"
#include "Scheduler.h"

namespace MetricsInternal {
    void finalizeTimes(std::vector<Process>& processes,
                        const std::vector<TimeSlice>& timeline);
}


