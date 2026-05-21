#pragma once
#include <string>

namespace ImageTasks {
    // Load a BMP image and perform CPU-bound scaling blits 'frames' times.
    // Returns elapsed seconds (wall time). Uses software surfaces (no window).
    double renderImageBMP(const std::string& bmpPath, int frames, int minScalePct = 50, int maxScalePct = 150);
}


