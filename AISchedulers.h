#pragma once

#include "Scheduler.h"
#include <vector>

// Simplified Q-Learning scheduler
class QLearningScheduler : public SchedulerBase {
public:
    QLearningScheduler(int maxBurstCategory = 5, int trainingEpisodes = 200,
                       double alpha = 0.5, double gamma = 0.9, double epsilon = 0.1);
    ScheduleResult run(const std::vector<Process>& processes) override;
    std::string name() const override { return "Q-Learning AI"; }
private:
    int burstCategories;
    int episodes;
    double learningRate;
    double discount;
    double exploration;

    int getBurstCategory(int burst) const;
};

// Simplified NN-like scoring scheduler
class NNLiketScheduler : public SchedulerBase {
public:
    NNLiketScheduler(double w1 = -1.0, double w2 = 0.1, double w3 = -0.5, int trainIters = 100);
    ScheduleResult run(const std::vector<Process>& processes) override;
    std::string name() const override { return "NN-like AI"; }
private:
    double w1, w2, w3;
    int trainIterations;
};


