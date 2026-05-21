#include "ProcessManager.h"
#include "Utils.h"

void ProcessManager::addProcess(const Process& p) {
    processes.push_back(p);
}

void ProcessManager::addProcess(int pid, int at, int bt, int pr) {
    processes.emplace_back(pid, at, bt, pr);
}

const std::vector<Process>& ProcessManager::list() const {
    return processes;
}

std::vector<Process> ProcessManager::getClone() const {
    return Utils::cloneWithRemaining(processes);
}


