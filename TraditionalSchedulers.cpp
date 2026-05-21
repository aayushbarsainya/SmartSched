#include <algorithm>
#include <queue>
#include "TraditionalSchedulers.h"
#include "Utils.h"
#include "Metrics.h"

// FCFS: sort by arrival time, run to completion in that order
ScheduleResult FCFS_Scheduler::run(const std::vector<Process>& processes) {
    std::vector<Process> procs = Utils::cloneWithRemaining(processes);
    std::sort(procs.begin(), procs.end(), [](const Process& a, const Process& b){
        if (a.arrivalTime == b.arrivalTime) return a.pid < b.pid;
        return a.arrivalTime < b.arrivalTime;
    });

    std::vector<TimeSlice> timeline;
    int time = 0;
    for (auto& p : procs) {
        if (time < p.arrivalTime) {
            time = p.arrivalTime; // idle until arrival
        }
        TimeSlice ts{p.pid, time, time + p.burstTime};
        timeline.push_back(ts);
        time += p.burstTime;
    }

    ScheduleResult result;
    result.algorithmName = name();
    result.timeline = std::move(timeline);
    result.metrics = Utils::computeMetrics(procs, result.timeline);
    // Update finalProcesses with computed times
    MetricsInternal::finalizeTimes(procs, result.timeline);
    result.finalProcesses = procs;
    return result;
}

// Non-preemptive SJF: at each availability, pick ready with shortest burst
ScheduleResult SJF_Scheduler::run(const std::vector<Process>& processes) {
    std::vector<Process> procs = Utils::cloneWithRemaining(processes);
    std::sort(procs.begin(), procs.end(), [](const Process& a, const Process& b){
        if (a.arrivalTime == b.arrivalTime) return a.pid < b.pid;
        return a.arrivalTime < b.arrivalTime;
    });

    std::vector<TimeSlice> timeline;
    int time = 0;
    int idx = 0;
    auto cmp = [](const Process* lhs, const Process* rhs){
        if (lhs->burstTime == rhs->burstTime) return lhs->pid > rhs->pid; // min-heap
        return lhs->burstTime > rhs->burstTime;
    };
    std::priority_queue<const Process*, std::vector<const Process*>, decltype(cmp)> ready(cmp);

    while (idx < (int)procs.size() || !ready.empty()) {
        while (idx < (int)procs.size() && procs[idx].arrivalTime <= time) {
            ready.push(&procs[idx]);
            idx++;
        }
        if (ready.empty()) {
            // jump to next arrival
            if (idx < (int)procs.size()) time = std::max(time, procs[idx].arrivalTime);
            continue;
        }
        const Process* next = ready.top(); ready.pop();
        TimeSlice ts{next->pid, time, time + next->burstTime};
        timeline.push_back(ts);
        time += next->burstTime;
    }

    ScheduleResult result;
    result.algorithmName = name();
    result.timeline = std::move(timeline);
    result.metrics = Utils::computeMetrics(procs, result.timeline);
    // Update finalProcesses with computed times
    MetricsInternal::finalizeTimes(procs, result.timeline);
    result.finalProcesses = procs;
    return result;
}

// Round Robin with fixed quantum
RR_Scheduler::RR_Scheduler(int timeQuantum) : quantum(timeQuantum) {}

ScheduleResult RR_Scheduler::run(const std::vector<Process>& processes) {
    std::vector<Process> procs = Utils::cloneWithRemaining(processes);
    std::sort(procs.begin(), procs.end(), [](const Process& a, const Process& b){
        if (a.arrivalTime == b.arrivalTime) return a.pid < b.pid;
        return a.arrivalTime < b.arrivalTime;
    });

    std::vector<TimeSlice> timeline;
    std::queue<int> q; // indices of procs
    int time = 0;
    int idx = 0;

    auto enqueueArrivals = [&](int currentTime){
        while (idx < (int)procs.size() && procs[idx].arrivalTime <= currentTime) {
            q.push(idx);
            idx++;
        }
    };

    if (!procs.empty()) time = std::max(0, procs.front().arrivalTime);
    enqueueArrivals(time);

    while (!q.empty() || idx < (int)procs.size()) {
        if (q.empty()) {
            time = std::max(time, procs[idx].arrivalTime);
            enqueueArrivals(time);
            continue;
        }
        int i = q.front(); q.pop();
        int run = std::min(quantum, procs[i].remainingTime);
        TimeSlice ts{procs[i].pid, time, time + run};
        timeline.push_back(ts);
        time += run;
        procs[i].remainingTime -= run;
        enqueueArrivals(time);
        if (procs[i].remainingTime > 0) {
            q.push(i);
        }
    }

    ScheduleResult result;
    result.algorithmName = name();
    result.timeline = std::move(timeline);
    result.metrics = Utils::computeMetrics(procs, result.timeline);
    // Update finalProcesses with computed times
    MetricsInternal::finalizeTimes(procs, result.timeline);
    result.finalProcesses = procs;
    return result;
}


