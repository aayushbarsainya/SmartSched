#pragma once

#include <string>
#include <vector>
#include "Process.h"

namespace RealTasks {
    // Measure elapsed time in seconds for executing a system command.
    // Returns negative value on failure.
    double measureTaskTime(const std::string& command);

    // Render a generated image using SDL2 and measure elapsed time in seconds.
    // width, height: image/window size; frames: number of frames to render.
    // Returns negative value on failure.
    double renderImageTask(int width, int height, int frames);

    // Save dataset to file in format: PID arrival burst
    bool saveDataset(const std::string& filename, const std::vector<Process>& processes);

    // Load dataset from file; PID is read or assigned sequentially if absent.
    std::vector<Process> loadProcessesFromFile(const std::string& filename);
}


