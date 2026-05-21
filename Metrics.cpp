#include <unordered_map>
#include "Metrics.h"

namespace MetricsInternal {
    void finalizeTimes(std::vector<Process>& processes,
                        const std::vector<TimeSlice>& timeline) {
        // Map pid to last completion time
        std::unordered_map<int, int> pidToCompletion;
        for (const auto& ts : timeline) {
            pidToCompletion[ts.pid] = ts.endTime;
        }
        for (auto& p : processes) {
            auto it = pidToCompletion.find(p.pid);
            if (it != pidToCompletion.end()) {
                p.completionTime = it->second;
                p.turnaroundTime = p.completionTime - p.arrivalTime;
                p.waitingTime = p.turnaroundTime - p.burstTime;
                if (p.waitingTime < 0) p.waitingTime = 0; // guard due to idle jumps
            }
        }
    }
}


