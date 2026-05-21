#pragma once

#include <vector>
#include "Process.h"

// Encapsulation example: manages a private collection of processes
class ProcessManager {
public:
    void addProcess(const Process& p);                 // overload 1
    void addProcess(int pid, int at, int bt, int pr);  // overload 2
    const std::vector<Process>& list() const;          // read-only access
    std::vector<Process> getClone() const;             // deep copy with remainingTime setup
private:
    std::vector<Process> processes; // kept private
};


