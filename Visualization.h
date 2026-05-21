#pragma once
#include <vector>
#include "Scheduler.h"

namespace Viz {
    // Launches an SDL2 window with interactive Gantt render
    // Keys: A/D pan, W/S or mousewheel zoom, Q/E switch scheduler, Esc quit
    void runInteractiveVisualization(const std::vector<IScheduler*>& schedulers,
                                     const std::vector<ScheduleResult>& results,
                                     int width = 1000, int height = 600);
}


