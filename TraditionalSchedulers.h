#pragma once

#include "Scheduler.h"

class FCFS_Scheduler : public SchedulerBase {
public:
    ScheduleResult run(const std::vector<Process>& processes) override;
    std::string name() const override { return "FCFS"; }
};

class SJF_Scheduler : public SchedulerBase {
public:
    ScheduleResult run(const std::vector<Process>& processes) override; // non-preemptive SJF
    std::string name() const override { return "SJF"; }
};

class RR_Scheduler : public SchedulerBase {
public:
    explicit RR_Scheduler(int timeQuantum);
    ScheduleResult run(const std::vector<Process>& processes) override;
private:
    int quantum;
public:
    std::string name() const override { return "Round Robin"; }
};


