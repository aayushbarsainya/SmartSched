#include <iostream>
#include <iomanip>
#include <algorithm>
#include <string>
#include "Utils.h"
#include "Metrics.h"

using std::cout;
using std::endl;

namespace Utils {

    void printGanttChart(const std::vector<TimeSlice>& timeline) {
        if (timeline.empty()) {
            cout << "[Empty timeline]\n";
            return;
        }
        // Top row: boxes with process ids
        for (const auto& ts : timeline) {
            cout << "| P" << ts.pid << " ";
        }
        cout << "|\n";
        // Bottom row: time markers
        int last = timeline.front().startTime;
        cout << last;
        for (const auto& ts : timeline) {
            cout << std::setw(5) << ts.endTime;
        }
        cout << "\n";
    }

    void printMetrics(const ScheduleResult& result) {
        cout << "Algorithm: " << result.algorithmName << "\n";
        printGanttChart(result.timeline);
        cout << std::fixed << std::setprecision(2);
        cout << "Avg Waiting Time    : " << result.metrics.averageWaitingTime << "\n";
        cout << "Avg Turnaround Time : " << result.metrics.averageTurnaroundTime << "\n";
        cout << "Context Switches    : " << result.metrics.contextSwitches << "\n";
        if (result.metrics.contextSwitchOverhead > 0.0) {
            cout << "Context Switch OVH  : " << result.metrics.contextSwitchOverhead << " ticks\n";
        }
        cout << "\n";
        cout << "Avg Slowdown        : " << result.metrics.averageSlowdown << "\n";
        cout << "Fairness (Jain on Slowdown, 0..1, higher=more fair): " 
             << result.metrics.jainFairnessSlowdown << "\n\n";
    }

    void printComparisonTable(const std::vector<ScheduleResult>& results) {
        if (results.empty()) return;
        
        // Get all unique process IDs from the first result
        std::vector<int> processIds;
        for (const auto& p : results[0].finalProcesses) {
            processIds.push_back(p.pid);
        }
        std::sort(processIds.begin(), processIds.end());
        
        // Print header
        cout << std::left << std::setw(12) << "Process";
        for (const auto& r : results) {
            cout << std::setw(20) << r.algorithmName;
        }
        cout << "\n";
        
        // Print separator
        int separatorLen = 12 + 20 * static_cast<int>(results.size());
        cout << std::string(separatorLen, '-') << "\n";
        
        cout << std::fixed << std::setprecision(2);
        
        // Print per-process metrics
        for (int pid : processIds) {
            cout << std::left << std::setw(12) << ("P" + std::to_string(pid));
            
            for (const auto& r : results) {
                // Find the process in this result
                const Process* proc = nullptr;
                for (const auto& p : r.finalProcesses) {
                    if (p.pid == pid) {
                        proc = &p;
                        break;
                    }
                }
                
                if (proc) {
                    std::string metrics = "WT:" + std::to_string(proc->waitingTime) + 
                                        " TT:" + std::to_string(proc->turnaroundTime);
                    cout << std::setw(20) << metrics;
                } else {
                    cout << std::setw(20) << "N/A";
                }
            }
            cout << "\n";
        }
        cout << "\n";

        // Print fairness summary line for each scheduler
        cout << "Fairness (Jain on Slowdown, 0..1, higher=better)\n";
        for (const auto& r : results) {
            cout << std::setw(20) << r.algorithmName << ": "
                 << std::fixed << std::setprecision(3) << r.metrics.jainFairnessSlowdown << "\n";
        }
        cout << "\n";
    }

    // Overload that also prints runtime-provided names via polymorphic schedulers vector
    void printComparisonTable(const std::vector<IScheduler*>& schedulers,
                              const std::vector<ScheduleResult>& results) {
        if (results.empty()) return;
        
        // Get all unique process IDs from the first result
        std::vector<int> processIds;
        for (const auto& p : results[0].finalProcesses) {
            processIds.push_back(p.pid);
        }
        std::sort(processIds.begin(), processIds.end());
        
        // Print header
        cout << std::left << std::setw(12) << "Process";
        for (size_t i = 0; i < results.size(); ++i) {
            std::string algoname = (i < schedulers.size() && schedulers[i]) ? schedulers[i]->name() : results[i].algorithmName;
            cout << std::setw(20) << algoname;
        }
        cout << "\n";
        
        // Print separator
        int separatorLen = 12 + 20 * static_cast<int>(results.size());
        cout << std::string(separatorLen, '-') << "\n";
        
        cout << std::fixed << std::setprecision(2);
        
        // Print per-process metrics
        for (int pid : processIds) {
            cout << std::left << std::setw(12) << ("P" + std::to_string(pid));
            
            for (const auto& r : results) {
                // Find the process in this result
                const Process* proc = nullptr;
                for (const auto& p : r.finalProcesses) {
                    if (p.pid == pid) {
                        proc = &p;
                        break;
                    }
                }
                
                if (proc) {
                    std::string metrics = "WT:" + std::to_string(proc->waitingTime) + 
                                        " TT:" + std::to_string(proc->turnaroundTime);
                    cout << std::setw(20) << metrics;
                } else {
                    cout << std::setw(20) << "N/A";
                }
            }
            cout << "\n";
        }
        cout << "\n";

        // Print fairness summary line for each scheduler
        cout << "Fairness (Jain on Slowdown, 0..1, higher=better)\n";
        for (size_t i = 0; i < results.size(); ++i) {
            std::string name = (i < schedulers.size() && schedulers[i])
                ? schedulers[i]->name() : results[i].algorithmName;
            cout << std::setw(20) << name << ": "
                 << std::fixed << std::setprecision(3) << results[i].metrics.jainFairnessSlowdown << "\n";
        }
        cout << "\n";
    }

    ScheduleMetrics computeMetrics(const std::vector<Process>& processes,
                                   const std::vector<TimeSlice>& timeline) {
        std::vector<Process> procs = processes;
        MetricsInternal::finalizeTimes(procs, timeline);

        double totalWaiting = 0.0;
        double totalTurnaround = 0.0;
        double totalSlowdown = 0.0;
        double sumSlow = 0.0;
        double sumSlowSq = 0.0;
        int nNonZero = 0;
        for (const auto& p : procs) {
            totalWaiting += p.waitingTime;
            totalTurnaround += p.turnaroundTime;
            // slowdown = turnaround / burst (avoid divide-by-zero)
            int denom = std::max(1, p.burstTime);
            double slowdown = static_cast<double>(p.turnaroundTime) / static_cast<double>(denom);
            totalSlowdown += slowdown;
            sumSlow += slowdown;
            sumSlowSq += slowdown * slowdown;
            nNonZero++;
        }
        ScheduleMetrics m;
        const int n = static_cast<int>(procs.size());
        if (n > 0) {
            m.averageWaitingTime = totalWaiting / n;
            m.averageTurnaroundTime = totalTurnaround / n;
            m.averageSlowdown = (nNonZero > 0) ? (totalSlowdown / nNonZero) : 0.0;
        }
        int makespan = 0;
        if (!timeline.empty()) {
            makespan = timeline.back().endTime - timeline.front().startTime;
        }
        int busy = 0;
        for (const auto& ts : timeline) {
            busy += (ts.endTime - ts.startTime);
        }
        
        // Calculate context switches
        int contextSwitches = 0;
        if (timeline.size() > 1) {
            for (size_t i = 1; i < timeline.size(); ++i) {
                if (timeline[i].pid != timeline[i-1].pid) {
                    contextSwitches++;
                }
            }
        }
        m.contextSwitches = contextSwitches;
        
        // Context switch overhead: assume minimal overhead for now
        // Different schedulers can have different overheads
        // If we find significant variation across schedulers, we can scale this
        m.contextSwitchOverhead = 0.0; // Set to 0 for now
        
        if (makespan > 0) {
            m.cpuUtilizationPercent = (busy * 100.0) / makespan;
            m.throughput = n / static_cast<double>(makespan);
        }
        // Jain's fairness index over per-process slowdown values
        if (nNonZero > 0) {
            double numerator = (sumSlow * sumSlow);
            double denominator = static_cast<double>(nNonZero) * sumSlowSq;
            if (denominator > 0.0) {
                m.jainFairnessSlowdown = numerator / denominator; // in (0,1]
            } else {
                m.jainFairnessSlowdown = 0.0;
            }
        }
        return m;
    }

    std::vector<Process> cloneWithRemaining(const std::vector<Process>& processes) {
        std::vector<Process> copy = processes;
        for (auto& p : copy) p.remainingTime = p.burstTime;
        return copy;
    }

    void recommendScheduler(const std::vector<Process>& processes,
                           const std::vector<IScheduler*>& schedulers,
                           const std::vector<ScheduleResult>& results) {
        if (results.empty() || schedulers.empty()) return;

        cout << "\n=== SCHEDULER RECOMMENDATION ===\n";
        
        // Analyze task characteristics
        int totalBurst = 0;
        int minBurst = 1e9, maxBurst = 0;
        int minArrival = 1e9, maxArrival = 0;
        double avgBurst = 0.0;
        double burstVariance = 0.0;
        
        for (const auto& p : processes) {
            totalBurst += p.burstTime;
            minBurst = std::min(minBurst, p.burstTime);
            maxBurst = std::max(maxBurst, p.burstTime);
            minArrival = std::min(minArrival, p.arrivalTime);
            maxArrival = std::max(maxArrival, p.arrivalTime);
        }
        
        if (!processes.empty()) {
            avgBurst = totalBurst / static_cast<double>(processes.size());
            for (const auto& p : processes) {
                double diff = p.burstTime - avgBurst;
                burstVariance += diff * diff;
            }
            burstVariance /= processes.size();
        }
        
        double burstRange = maxBurst - minBurst;
        bool hasShortTasks = minBurst <= 3;
        bool hasLongTasks = maxBurst >= 10;
        bool hasMixedBursts = burstRange > avgBurst * 0.5;
        bool hasStaggeredArrivals = (maxArrival - minArrival) > processes.size() * 2;
        
        // Classify task type
        std::string taskType;
        if (hasShortTasks && !hasLongTasks) {
            taskType = "Short Tasks (all tasks <= 3 units)";
        } else if (hasLongTasks && !hasShortTasks) {
            taskType = "Long Tasks (all tasks >= 10 units)";
        } else if (hasMixedBursts) {
            taskType = "Mixed Workload (wide range of task sizes)";
        } else {
            taskType = "Uniform Workload (similar task sizes)";
        }
        
        cout << "Task Characteristics:\n";
        cout << "  Type: " << taskType << "\n";
        cout << "  Number of processes: " << processes.size() << "\n";
        cout << "  Average burst time: " << std::fixed << std::setprecision(2) << avgBurst << "\n";
        cout << "  Burst time range: " << minBurst << " - " << maxBurst << "\n";
        cout << "  Arrival time range: " << minArrival << " - " << maxArrival << "\n";
        cout << "  Staggered arrivals: " << (hasStaggeredArrivals ? "Yes" : "No") << "\n\n";
        
        // Find best scheduler for different metrics
        int bestWaitingIdx = 0, bestTurnaroundIdx = 0;
        double bestWaiting = 1e18, bestTurnaround = 1e18;
        
        for (size_t i = 0; i < results.size(); ++i) {
            if (results[i].metrics.averageWaitingTime < bestWaiting) {
                bestWaiting = results[i].metrics.averageWaitingTime;
                bestWaitingIdx = i;
            }
            if (results[i].metrics.averageTurnaroundTime < bestTurnaround) {
                bestTurnaround = results[i].metrics.averageTurnaroundTime;
                bestTurnaroundIdx = i;
            }
        }
        
        std::string bestWaitingName = (bestWaitingIdx < schedulers.size() && schedulers[bestWaitingIdx]) 
            ? schedulers[bestWaitingIdx]->name() : results[bestWaitingIdx].algorithmName;
        std::string bestTurnaroundName = (bestTurnaroundIdx < schedulers.size() && schedulers[bestTurnaroundIdx]) 
            ? schedulers[bestTurnaroundIdx]->name() : results[bestTurnaroundIdx].algorithmName;
        
        cout << "Performance Analysis:\n";
        cout << "  Best Average Waiting Time: " << bestWaitingName 
             << " (" << std::fixed << std::setprecision(2) << bestWaiting << ")\n";
        cout << "  Best Average Turnaround Time: " << bestTurnaroundName 
             << " (" << std::fixed << std::setprecision(2) << bestTurnaround << ")\n\n";
        
        // Make recommendations based on task type
        cout << "Recommendations:\n";
        
        if (hasShortTasks && !hasLongTasks) {
            cout << "  ✓ For short tasks, SJF or NN-like AI typically perform well\n";
            cout << "  ✓ Round Robin may cause overhead for very short tasks\n";
        } else if (hasLongTasks && !hasShortTasks) {
            cout << "  ✓ For long tasks, FCFS or Q-Learning AI can be effective\n";
            cout << "  ✓ Round Robin provides fairness for long-running tasks\n";
        } else if (hasMixedBursts) {
            cout << "  ✓ For mixed workloads, SJF or AI schedulers adapt better\n";
            cout << "  ✓ Round Robin ensures fairness across different task sizes\n";
        } else {
            cout << "  ✓ For uniform workloads, FCFS is simple and effective\n";
            cout << "  ✓ All schedulers perform similarly on uniform tasks\n";
        }
        
        if (hasStaggeredArrivals) {
            cout << "  ✓ With staggered arrivals, preemptive schedulers (RR, AI) handle better\n";
        }
        
        // Overall recommendation
        cout << "\n  → RECOMMENDED: " << bestWaitingName;
        if (bestWaitingIdx != bestTurnaroundIdx) {
            cout << " (best waiting time) or " << bestTurnaroundName << " (best turnaround time)";
        }
        cout << "\n";
        
        // Additional insights
        cout << "\nAdditional Insights:\n";
        for (size_t i = 0; i < results.size(); ++i) {
            std::string name = (i < schedulers.size() && schedulers[i]) 
                ? schedulers[i]->name() : results[i].algorithmName;
            double waiting = results[i].metrics.averageWaitingTime;
            double turnaround = results[i].metrics.averageTurnaroundTime;
            
            if (waiting == bestWaiting && turnaround == bestTurnaround) {
                cout << "  ★ " << name << " is optimal for both metrics\n";
            } else if (waiting == bestWaiting) {
                cout << "  • " << name << " minimizes waiting time\n";
            } else if (turnaround == bestTurnaround) {
                cout << "  • " << name << " minimizes turnaround time\n";
            }
        }
        cout << "\n";
    }

    void printOverallMatrix(const std::vector<IScheduler*>& schedulers,
                           const std::vector<ScheduleResult>& results) {
        if (results.empty()) return;

        cout << "\n=== FINAL OVERALL PERFORMANCE MATRIX ===\n\n";
        
        // Print header
        cout << std::left << std::setw(20) << "Scheduler"
             << std::setw(18) << "Avg Waiting Time"
             << std::setw(20) << "Avg Turnaround Time"
             << std::setw(18) << "Context Switches"
             << std::setw(15) << "Best For" << "\n";
        
        // Print separator
        cout << std::string(20 + 18 + 20 + 18 + 15, '=') << "\n";
        
        cout << std::fixed << std::setprecision(2);
        
        // Find best values for highlighting
        double bestWaiting = 1e18, bestTurnaround = 1e18;
        for (const auto& r : results) {
            if (r.metrics.averageWaitingTime < bestWaiting) {
                bestWaiting = r.metrics.averageWaitingTime;
            }
            if (r.metrics.averageTurnaroundTime < bestTurnaround) {
                bestTurnaround = r.metrics.averageTurnaroundTime;
            }
        }
        
        // Print each scheduler's metrics
        for (size_t i = 0; i < results.size(); ++i) {
            const auto& r = results[i];
            std::string name = (i < schedulers.size() && schedulers[i]) 
                ? schedulers[i]->name() : r.algorithmName;
            
            double waiting = r.metrics.averageWaitingTime;
            double turnaround = r.metrics.averageTurnaroundTime;
            
            // Determine what this scheduler is best for
            std::string bestFor = "";
            if (waiting == bestWaiting && turnaround == bestTurnaround) {
                bestFor = "Both Metrics";
            } else if (waiting == bestWaiting) {
                bestFor = "Waiting Time";
            } else if (turnaround == bestTurnaround) {
                bestFor = "Turnaround Time";
            } else {
                bestFor = "-";
            }
            
            cout << std::left << std::setw(20) << name
                 << std::setw(18) << waiting
                 << std::setw(20) << turnaround
                 << std::setw(18) << r.metrics.contextSwitches
                 << std::setw(15) << bestFor << "\n";
        }
        
        cout << std::string(20 + 18 + 20 + 15, '=') << "\n";
        
        // Print summary statistics
        cout << "\nSummary:\n";
        cout << "  Best Average Waiting Time: " << std::fixed << std::setprecision(2) << bestWaiting << "\n";
        cout << "  Best Average Turnaround Time: " << std::fixed << std::setprecision(2) << bestTurnaround << "\n";
        
        // Count how many schedulers are optimal
        int optimalForWaiting = 0, optimalForTurnaround = 0, optimalForBoth = 0;
        for (const auto& r : results) {
            if (r.metrics.averageWaitingTime == bestWaiting && r.metrics.averageTurnaroundTime == bestTurnaround) {
                optimalForBoth++;
            } else if (r.metrics.averageWaitingTime == bestWaiting) {
                optimalForWaiting++;
            } else if (r.metrics.averageTurnaroundTime == bestTurnaround) {
                optimalForTurnaround++;
            }
        }
        
        cout << "  Schedulers optimal for waiting time: " << optimalForWaiting + optimalForBoth << "\n";
        cout << "  Schedulers optimal for turnaround time: " << optimalForTurnaround + optimalForBoth << "\n";
        cout << "  Schedulers optimal for both: " << optimalForBoth << "\n";
        cout << "\n";
    }
}


