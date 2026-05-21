#pragma once

#include <string>
#include <iostream>

struct Process {
    int pid;              // Process ID
    int arrivalTime;      // Time at which process arrives
    int burstTime;        // CPU burst time
    int priority;         // Priority (optional)
    int remainingTime;    // For RR/Preemptive algorithms
    int completionTime;
    int waitingTime;
    int turnaroundTime;

    Process()
        : pid(0), arrivalTime(0), burstTime(0), priority(0), remainingTime(0),
          completionTime(0), waitingTime(0), turnaroundTime(0) {}

    Process(int id, int at, int bt, int prio = 0)
        : pid(id), arrivalTime(at), burstTime(bt), priority(prio),
          remainingTime(bt), completionTime(0), waitingTime(0), turnaroundTime(0) {}
};

// Friend function demonstration: allow streaming Process without exposing internals elsewhere
inline std::ostream& operator<<(std::ostream& os, const Process& p) {
    os << "P" << p.pid << "(AT=" << p.arrivalTime << ", BT=" << p.burstTime
       << ", PR=" << p.priority << ")";
    return os;
}


