#pragma once

#include <vector>
#include <string>
#include "Process.h"

struct TimeSlice {
    int pid;
    int startTime;
    int endTime;
};

struct ScheduleMetrics {
    double averageWaitingTime = 0.0;
    double averageTurnaroundTime = 0.0;
    double cpuUtilizationPercent = 0.0; // 0..100
    double throughput = 0.0;            // processes per unit time
    // Fairness metrics (higher is better for Jain index, ideal = 1.0)
    double jainFairnessSlowdown = 0.0;  // Jain's index over per-process slowdown
    double averageSlowdown = 0.0;       // avg(turnaround / max(1, burst))
    int contextSwitches = 0;            // Number of context switches
    double contextSwitchOverhead = 0.0; // Total overhead in ticks (if any)
};

struct ScheduleResult {
    std::string algorithmName;
    std::vector<TimeSlice> timeline; // For Gantt chart
    ScheduleMetrics metrics;
    std::vector<Process> finalProcesses; // With computed times
};

class IScheduler {
public:
    virtual ~IScheduler() = default;
    virtual ScheduleResult run(const std::vector<Process>& processes) = 0;
    virtual std::string name() const = 0;
};

// Abstract base providing common helpers and a virtual name() to demonstrate inheritance
class SchedulerBase : public IScheduler {
public:
    ~SchedulerBase() override = default;
    virtual std::string name() const override = 0;
};


