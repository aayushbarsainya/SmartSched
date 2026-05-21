#pragma once

#include <vector>
#include <string>
#include "Scheduler.h"
#include "Process.h"

namespace Utils {
    void printGanttChart(const std::vector<TimeSlice>& timeline);
    void printMetrics(const ScheduleResult& result);
    void printComparisonTable(const std::vector<ScheduleResult>& results);
    // Overloaded print to demonstrate function overloading
    void printComparisonTable(const std::vector<IScheduler*>& schedulers,
                              const std::vector<ScheduleResult>& results);

    // Common helper used by schedulers to compute metrics after timeline is built
    ScheduleMetrics computeMetrics(const std::vector<Process>& processes,
                                   const std::vector<TimeSlice>& timeline);

    // Make deep copy with remainingTime initialized to burstTime
    std::vector<Process> cloneWithRemaining(const std::vector<Process>& processes);

    // Analyze task characteristics and recommend best scheduler
    void recommendScheduler(const std::vector<Process>& processes,
                           const std::vector<IScheduler*>& schedulers,
                           const std::vector<ScheduleResult>& results);

    // Print final overall summary matrix with all schedulers and key metrics
    void printOverallMatrix(const std::vector<IScheduler*>& schedulers,
                           const std::vector<ScheduleResult>& results);
}


