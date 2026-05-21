#include <algorithm>
#include <random>
#include <unordered_map>
#include "AISchedulers.h"
#include "Utils.h"
#include "Metrics.h"

// Q-Learning based scheduler
QLearningScheduler::QLearningScheduler(int maxBurstCategory, int trainingEpisodes,
                                       double alpha, double gamma, double epsilon)
    : burstCategories(maxBurstCategory), episodes(trainingEpisodes),
      learningRate(alpha), discount(gamma), exploration(epsilon) {}

int QLearningScheduler::getBurstCategory(int burst) const {
    // Categorize burst into discrete buckets 1..burstCategories
    if (burst <= 0) return 0;
    int capped = std::min(burst, burstCategories);
    return capped - 1; // 0-based
}

ScheduleResult QLearningScheduler::run(const std::vector<Process>& processes) {
    // Build a small Q-table: state = burstCategory, action = index among ready set by sorted pid
    // For simplicity, we treat actions as choosing the index within the ready list at decision time.
    std::vector<Process> base = Utils::cloneWithRemaining(processes);
    std::sort(base.begin(), base.end(), [](const Process& a, const Process& b){
        if (a.arrivalTime == b.arrivalTime) return a.pid < b.pid;
        return a.arrivalTime < b.arrivalTime;
    });

    // Q-table as map: (stateCategory, actionIndex) -> Q value
    std::unordered_map<long long, double> Q;
    auto key = [&](int s, int a){ return (static_cast<long long>(s) << 32) | (unsigned int)a; };

    std::mt19937 rng(42);
    std::uniform_real_distribution<double> uni(0.0, 1.0);

    auto simulate = [&](bool useEpsilonGreedy){
        std::vector<Process> procs = base;
        int time = 0;
        std::vector<TimeSlice> timeline;
        std::vector<int> readyIndices; // store indices into procs

        auto refreshReady = [&](int t){
            readyIndices.clear();
            for (int i = 0; i < (int)procs.size(); ++i) {
                if (procs[i].arrivalTime <= t && procs[i].remainingTime > 0) readyIndices.push_back(i);
            }
            std::sort(readyIndices.begin(), readyIndices.end(), [&](int i, int j){ return procs[i].pid < procs[j].pid; });
        };

        if (!procs.empty()) time = std::max(0, procs.front().arrivalTime);
        while (true) {
            bool allDone = true;
            for (const auto& p : procs) if (p.remainingTime > 0) { allDone = false; break; }
            if (allDone) break;

            refreshReady(time);
            if (readyIndices.empty()) {
                // jump to next arrival
                int nextArrival = 1e9;
                for (const auto& p : procs) if (p.remainingTime > 0) nextArrival = std::min(nextArrival, p.arrivalTime);
                time = std::max(time, nextArrival);
                continue;
            }
            // state: min burst among ready as proxy
            int minBurst = 1e9;
            for (int i : readyIndices) minBurst = std::min(minBurst, procs[i].remainingTime);
            int state = getBurstCategory(minBurst);

            int action = 0;
            if (useEpsilonGreedy && uni(rng) < exploration) {
                std::uniform_int_distribution<int> uniIdx(0, (int)readyIndices.size()-1);
                action = uniIdx(rng);
            } else {
                // pick argmax Q[state, action]
                double bestQ = -1e18; int bestA = 0;
                for (int a = 0; a < (int)readyIndices.size(); ++a) {
                    double qv = Q.count(key(state, a)) ? Q[key(state, a)] : 0.0;
                    if (qv > bestQ) { bestQ = qv; bestA = a; }
                }
                action = bestA;
            }

            int chosenIdx = readyIndices[action];
            // Execute for 1 unit time to allow preemption-like decisions
            TimeSlice ts{procs[chosenIdx].pid, time, time + 1};
            timeline.push_back(ts);
            time += 1;
            procs[chosenIdx].remainingTime -= 1;

            // reward: negative waiting time for others in ready set during this tick
            int totalWaitingInc = 0;
            for (int i : readyIndices) if (i != chosenIdx) totalWaitingInc += 1;
            double reward = -static_cast<double>(totalWaitingInc);

            // next state
            refreshReady(time);
            int nextMin = 1e9; for (int i : readyIndices) nextMin = std::min(nextMin, procs[i].remainingTime);
            int nextState = getBurstCategory(nextMin == 1e9 ? 0 : nextMin);

            // Q update
            double oldQ = Q.count(key(state, action)) ? Q[key(state, action)] : 0.0;
            double bestNext = 0.0;
            for (int a = 0; a < (int)readyIndices.size(); ++a) {
                double qv = Q.count(key(nextState, a)) ? Q[key(nextState, a)] : 0.0;
                if (a == 0 || qv > bestNext) bestNext = qv;
            }
            double newQ = oldQ + learningRate * (reward + discount * bestNext - oldQ);
            Q[key(state, action)] = newQ;
        }
        return timeline;
    };

    // Train
    for (int e = 0; e < episodes; ++e) {
        (void)simulate(true);
    }

    // Exploit to generate final schedule
    auto timeline = simulate(false);
    ScheduleResult result;
    result.algorithmName = name();
    result.timeline = std::move(timeline);
    result.metrics = Utils::computeMetrics(base, result.timeline);
    // Update finalProcesses with computed times
    MetricsInternal::finalizeTimes(base, result.timeline);
    result.finalProcesses = base;
    return result;
}

// NN-like scheduler
NNLiketScheduler::NNLiketScheduler(double w1_, double w2_, double w3_, int trainIters)
    : w1(w1_), w2(w2_), w3(w3_), trainIterations(trainIters) {}

ScheduleResult NNLiketScheduler::run(const std::vector<Process>& processes) {
    // Optional naive training: reduce w1 (favor shorter bursts) if average waiting is high
    std::vector<Process> base = Utils::cloneWithRemaining(processes);
    std::sort(base.begin(), base.end(), [](const Process& a, const Process& b){
        if (a.arrivalTime == b.arrivalTime) return a.pid < b.pid;
        return a.arrivalTime < b.arrivalTime;
    });

    auto scheduleWithWeights = [&](double a, double b, double c){
        std::vector<Process> procs = base;
        std::vector<TimeSlice> timeline;
        int time = 0;
        if (!procs.empty()) time = std::max(0, procs.front().arrivalTime);
        while (true) {
            bool allDone = true;
            for (const auto& p : procs) if (p.remainingTime > 0) { allDone = false; break; }
            if (allDone) break;

            // build ready set
            std::vector<int> ready;
            for (int i = 0; i < (int)procs.size(); ++i) {
                if (procs[i].arrivalTime <= time && procs[i].remainingTime > 0) ready.push_back(i);
            }
            if (ready.empty()) {
                int nextArrival = 1e9;
                for (const auto& p : procs) if (p.remainingTime > 0) nextArrival = std::min(nextArrival, p.arrivalTime);
                time = std::max(time, nextArrival);
                continue;
            }
            // choose highest score (lower is better if w1 negative). We'll pick max score.
            int best = ready.front();
            double bestScore = a * procs[best].remainingTime + b * procs[best].arrivalTime + c * procs[best].priority;
            for (int i : ready) {
                double s = a * procs[i].remainingTime + b * procs[i].arrivalTime + c * procs[i].priority;
                if (s > bestScore) { bestScore = s; best = i; }
            }
            // run 1 unit to allow dynamic decisions
            TimeSlice ts{procs[best].pid, time, time + 1};
            timeline.push_back(ts);
            time += 1;
            procs[best].remainingTime -= 1;
        }
        return timeline;
    };

    // Simple training loop: try slight variations and keep the better set based on waiting time
    double a = w1, b = w2, c = w3;
    double bestAWT = 1e18;
    for (int it = 0; it < trainIterations; ++it) {
        auto tl = scheduleWithWeights(a, b, c);
        auto metrics = Utils::computeMetrics(base, tl);
        if (metrics.averageWaitingTime < bestAWT) {
            bestAWT = metrics.averageWaitingTime;
            w1 = a; w2 = b; w3 = c;
        }
        // tweak
        a *= 1.0 + ((it % 2 == 0) ? -0.05 : 0.03);
        b *= 1.0 + ((it % 3 == 0) ? -0.02 : 0.02);
        c *= 1.0 + ((it % 5 == 0) ? -0.01 : 0.01);
    }

    auto finalTimeline = scheduleWithWeights(w1, w2, w3);
    ScheduleResult result;
    result.algorithmName = name();
    result.timeline = std::move(finalTimeline);
    result.metrics = Utils::computeMetrics(base, result.timeline);
    // Update finalProcesses with computed times
    MetricsInternal::finalizeTimes(base, result.timeline);
    result.finalProcesses = base;
    return result;
}


