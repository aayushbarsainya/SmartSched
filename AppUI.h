#pragma once
#include <vector>
#include <memory>
#include "Process.h"
#include "Scheduler.h"

namespace AppUI {
    // Launches the full interactive SmartSched Graphical User Interface
    // Returns 0 on normal exit, non-zero on failure.
    int run();
}
