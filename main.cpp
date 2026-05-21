#include <iostream>
#include <vector>
#include <limits>
#include "TraditionalSchedulers.h"
#include "AISchedulers.h"
#include "Utils.h"
#include "ProcessManager.h"
#include "RealTaskManager.h"
#include "Visualization.h"
#include "custom_tasks/Tasks.h"
#include <chrono>
#include "image_tasks/ImageTasks.h"
#include "cpu_stress/CpuStress.h"
#include <iomanip>

static std::vector<Process> readSampleOrInput() {
    std::vector<Process> processes;
    std::cout << "Enter number of processes (or 0 to use sample): ";
    int n; if (!(std::cin >> n)) { std::cin.clear(); std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n'); n = 0; }
    if (n <= 0) {
        // Sample data: 5 processes
        processes = {
            Process(1, 0, 7, 1),
            Process(2, 2, 4, 3),
            Process(3, 4, 1, 2),
            Process(4, 5, 4, 1),
            Process(5, 6, 6, 2)
        };
        return processes;
    }
    processes.reserve(n);
    for (int i = 0; i < n; ++i) {
        int pid = i + 1;
        int at, bt, pr;
        std::cout << "P" << pid << " arrivalTime burstTime priority: ";
        std::cin >> at >> bt >> pr;
        processes.emplace_back(pid, at, bt, pr);
    }
    return processes;
}

int main() {
    std::cout << "Enter 1 for sample data, 2 for real task measurement, 3 to load from file, 4 for custom tasks, 5 for image render (BMP), 6 to replay CPU load: ";
    int mode = 1; std::cin >> mode;

    // Use ProcessManager (encapsulation + overloading) to build process list
    ProcessManager pm;
    if (mode == 1) {
        auto input = readSampleOrInput();
        for (const auto& p : input) pm.addProcess(p);
    } else if (mode == 2) {
        const int n = 10; // Fixed number of diverse tasks
        int currentTime = 0;
        
        // Pre-defined diverse workloads
        struct TaskConfig {
            int width, height, frames;
            const char* description;
        };
        
        const TaskConfig configs[] = {
            {1920, 1080, 120, "4K Heavy"},      // Heavy workload
            {1280, 720, 60,  "720p Medium"},    // Medium workload
            {800, 600, 180,  "Long Animation"}, // Many frames
            {1920, 1080, 45, "1080p Quick"},   // High res, fewer frames
            {640, 480, 240,  "Long Render"},   // Longest render
            {1440, 900, 90,  "Wide Format"},   // Wide screen
            {1024, 768, 150, "Extended"},      // Extended render
            {1600, 900, 75,  "HD+ Render"},    // HD+ resolution
            {800, 600, 200,  "Bulk Frames"},   // Bulk processing
            {1920, 1080, 60, "Standard HD"}    // Standard HD
        };
        
        std::cout << "Processing " << n << " diverse rendering tasks automatically...\n";
        for (int i = 1; i <= n; ++i) {
            const auto& config = configs[i-1];
            std::cout << "Task " << i << ": " << config.description << " (" 
                      << config.width << "x" << config.height << ", " 
                      << config.frames << " frames)\n";
            std::cout << "[Rendering " << config.width << "x" << config.height 
                      << " for " << config.frames << " frames]\n";
            double seconds = RealTasks::renderImageTask(config.width, config.height, config.frames);
            if (seconds < 0) {
                std::cout << "Render failed, recording absolute time anyway.\n";
                seconds = -seconds;
            }
            // Scale up the ticks to make differences more noticeable
            int burstTicks = static_cast<int>(seconds * 10 + 0.5);
            if (burstTicks <= 0) burstTicks = 1;
            pm.addProcess(i, currentTime, burstTicks, 0);
            std::cout << "P" << i << " completed in " << seconds << "s (ticks=" << burstTicks << ")\n";
            currentTime += 1; // simple staggered arrivals
        }
        auto processes = pm.list();
        if (RealTasks::saveDataset("real_data.txt", processes)) {
            std::cout << "Dataset saved to real_data.txt\n";
        } else {
            std::cout << "Failed to save dataset.\n";
        }
    } else if (mode == 3) {
        std::cout << "Enter dataset filename (default real_data.txt): ";
        std::string fn;
        std::cin >> fn;
        if (fn.empty()) fn = "real_data.txt";
        auto loaded = RealTasks::loadProcessesFromFile(fn);
        if (loaded.empty()) {
            std::cout << "No processes loaded from file. Falling back to sample.\n";
            auto input = readSampleOrInput();
            for (const auto& p : input) pm.addProcess(p);
        } else {
            for (const auto& p : loaded) pm.addProcess(p);
        }
    } else if (mode == 4) {
        // Custom tasks mode: collect k and run built-in CPU tasks from custom_tasks/
        int k = 0;
        std::cout << "Enter value of k: ";
        std::cin >> k;
        if (!std::cin) { std::cin.clear(); std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n'); k = 10000; }
        struct TaskDef { const char* name; std::uint64_t (*fn)(int); };
        std::vector<TaskDef> tasks = {
            { "kth Prime",      &CustomTasks::kthPrime },
            { "Fibonacci(k)",   &CustomTasks::fibonacciK },
            { "Factorial(k)",   &CustomTasks::factorialK },
            { "kth Palindrome", &CustomTasks::kthPalindrome },
        };
        int currentTime = 0;
        std::cout << "Running " << tasks.size() << " custom tasks with k=" << k << "...\n";
        for (size_t i = 0; i < tasks.size(); ++i) {
            const auto& t = tasks[i];
            auto start = std::chrono::high_resolution_clock::now();
            std::uint64_t result = t.fn(k);
            auto end = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double> diff = end - start;
            double seconds = diff.count();
            std::cout << "Task " << (i+1) << " [" << t.name << "] result=" << result
                      << " time=" << seconds << "s\n";
            int burstTicks = static_cast<int>(seconds * 10 + 0.5);
            if (burstTicks <= 0) burstTicks = 1;
            pm.addProcess(static_cast<int>(i+1), currentTime, burstTicks, 0);
            currentTime += 1;
        }
    } else if (mode == 5) {
        // Image render mode (BMP without SDL_image). We will render the same image many times,
        // create multiple tasks with staggered arrivals to get a good performance matrix.
        std::cout << "Enter BMP image path: ";
        std::string path; std::cin >> path;
        int frames = 60;
        std::cout << "Enter frames per task (default 60): ";
        if (!(std::cin >> frames)) {
            std::cin.clear();
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            frames = 60;
        }
        int numTasks = 8;
        std::cout << "Enter number of tasks to spawn (default 8): ";
        if (!(std::cin >> numTasks) || numTasks <= 0) {
            std::cin.clear();
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            numTasks = 8;
        }

        std::cout << "Running " << numTasks << " image tasks on " << path << "...\n";
        int currentTime = 0;
        for (int i = 1; i <= numTasks; ++i) {
            // Vary scales to diversify cost a bit
            int minPct = 60 + (i % 3) * 10;   // 60, 70, 80
            int maxPct = 140 + (i % 3) * 10;  // 140, 150, 160
            double seconds = ImageTasks::renderImageBMP(path, frames, minPct, maxPct);
            if (seconds <= 0) {
                std::cout << "Failed to process BMP (or 0s), using small default time.\n";
                seconds = 0.1;
            }
            int burstTicks = static_cast<int>(seconds * 10 + 0.5);
            if (burstTicks <= 0) burstTicks = 1;
            pm.addProcess(i, currentTime, burstTicks, 0);
            std::cout << "Task " << i << " seconds=" << seconds << " ticks=" << burstTicks << "\n";
            // Stagger arrivals to create interesting schedules
            currentTime += (i % 2 == 0) ? 2 : 1;
        }
    } else if (mode == 6) {
        // Ensure we have at least some processes; if none yet, use sample
        if (pm.list().empty()) {
            auto input = readSampleOrInput();
            for (const auto& p : input) pm.addProcess(p);
        }
    } else {
        auto input = readSampleOrInput();
        for (const auto& p : input) pm.addProcess(p);
    }

    // Set up schedulers polymorphically
    FCFS_Scheduler fcfs; SJF_Scheduler sjf; RR_Scheduler rr(2);
    QLearningScheduler ql(5, 200, 0.5, 0.9, 0.1); NNLiketScheduler nn(-1.0, 0.1, -0.5, 100);
    std::vector<IScheduler*> schedulers{ &fcfs, &sjf, &rr, &ql, &nn };

    std::vector<ScheduleResult> results;
    for (auto* s : schedulers) {
        // demonstrate virtual function dispatch and overriding
        auto res = s->run(pm.list());
        results.push_back(res);
    }

    bool showComparisonTable = true;  // Default: show all schedulers' results
    int replayOnlyIdx = -1;            // If >= 0, only show this scheduler in replay mode

    if (mode == 6) {
        std::cout << "\n=== CPU Load Replay Mode ===\n";
        std::cout << "This mode replays the scheduler's timeline as real CPU load.\n";
        std::cout << "You have CPU cores 0-6 available.\n\n";
        std::cout << "IMPORTANT: For clearest pattern visualization:\n";
        std::cout << "  Pin to SINGLE CPU (modes 1-6) for stable, consistent patterns\n";
        std::cout << "  Multi-core (mode 0) = random CPU assignment each run\n\n";
        std::cout << "Recommended setup:\n";
        std::cout << "  1. Choose single CPU pin (1-6) - avoids random OS scheduling\n";
        std::cout << "  2. Use work multiplier 2-4x to make pattern more visible\n";
        std::cout << "  3. Use msPerTick 100-200 for smooth graphs\n\n";
        std::cout << "Run 1) single scheduler or 2) compare all? ";
        int runMode = 1; std::cin >> runMode;
        int msPerTick = 100;  // Increased default for better visibility
        std::cout << "Milliseconds per tick (default 100, range 10-500): ";
        if (!(std::cin >> msPerTick) || msPerTick <= 0) { msPerTick = 100; }
        msPerTick = std::max(10, std::min(500, msPerTick));
        
        int coreMode = 0;
        std::cout << "\nCore affinity modes:\n";
        std::cout << "  0=all-cores (random assignment, inconsistent)\n";
        std::cout << "  1=pin to CPU 1  2=pin to CPU 2  3=pin to CPU 3\n";
        std::cout << "  4=pin to CPU 4  5=pin to CPU 5  6=pin to CPU 6\n";
        std::cout << "Select (0-6, default 1 for stable visualization): ";
        if (!(std::cin >> coreMode)) { coreMode = 1; }
        coreMode = std::max(0, std::min(6, coreMode));
        
        int workMultiplier = 1;
        std::cout << "Work multiplier (1=normal, 2-4 for more visible patterns, default 1): ";
        if (!(std::cin >> workMultiplier)) { workMultiplier = 1; }
        workMultiplier = std::max(1, std::min(10, workMultiplier));
        
        bool singleCore = (coreMode != 0);
        
        int csOverhead = 0;
        std::cout << "Synthetic per-switch overhead ms (0..10, default 0): ";
        if (!(std::cin >> csOverhead) || csOverhead < 0) csOverhead = 0;
        csOverhead = std::min(10, csOverhead);

        if (runMode == 1) {
            std::cout << "\nAvailable schedulers:\n";
            for (size_t i = 0; i < schedulers.size(); ++i) {
                std::string name = (schedulers[i]) ? schedulers[i]->name() : results[i].algorithmName;
                std::cout << "  " << i << ": " << name << "\n";
            }
            std::cout << "\nSelect scheduler index to replay: ";
            int idx = 0; 
            if (!(std::cin >> idx)) { std::cin.clear(); std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n'); idx = 0; }
            if (idx < 0 || idx >= (int)results.size()) idx = 0;
            
            replayOnlyIdx = idx;
            std::string name = (idx < (int)schedulers.size() && schedulers[idx]) ? schedulers[idx]->name() : results[idx].algorithmName;
            std::cout << "\n>>> Replaying only " << name << " <<<\n";
            std::cout << ">>> Open Task Manager now to observe CPU usage <<<\n";
            std::cout << ">>> Press Enter to start replay...\n";
            std::cin.ignore();
            
            std::string coreDesc = (coreMode == 0) ? "all-cores" : ("CPU " + std::to_string(coreMode));
            std::cout << "\nReplaying CPU load (msPerTick=" << msPerTick 
                      << ", core=" << coreDesc
                      << ", multiplier=" << workMultiplier << "x"
                      << ", overhead=" << csOverhead << "ms)...\n";
            
            double secs = CpuStress::runCpuTimeline(results[idx].timeline, msPerTick, coreMode, workMultiplier);
            
            std::cout << "\n>>> Replay completed in " << std::fixed << std::setprecision(3) << secs << " seconds <<<\n";
            std::cout << ">>> Check Task Manager Performance graph above <<<\n";
            std::cout << "\nScheduler: " << name << "\n";
            std::cout << "Timeline slices: " << results[idx].timeline.size() << "\n";
            
            // Count context switches for this scheduler
            int cs = 0;
            int prevPid = results[idx].timeline.empty() ? -1 : results[idx].timeline.front().pid;
            for (const auto& ts : results[idx].timeline) {
                if (ts.pid != prevPid) cs++;
                prevPid = ts.pid;
            }
            std::cout << "Context switches: " << cs << "\n";
            std::cout << "Total makespan: " << (results[idx].timeline.empty() ? 0 : results[idx].timeline.back().endTime) << " ticks\n";
            
            showComparisonTable = false;  // Only show this scheduler's results
        } else {
            // Compare all schedulers
            std::cout << "\n=== Comparing all schedulers (sequential replay) ===\n";
            std::cout << ">>> Press Enter to start...\n";
            std::cin.ignore();
            
            std::string coreDesc = (coreMode == 0) ? "all-cores" : ("CPU " + std::to_string(coreMode));
            std::cout << "\n=== CPU Replay Comparison ===\n";
            std::cout << "msPerTick=" << msPerTick << ", core=" << coreDesc 
                      << ", multiplier=" << workMultiplier << "x\n\n";
            
            for (size_t i = 0; i < results.size(); ++i) {
                std::string name = (i < schedulers.size() && schedulers[i]) ? schedulers[i]->name() : results[i].algorithmName;
                std::cout << "--- Replaying " << name << " ---\n";
                
                // Count context switches
                int cs = 0;
                int prevPid = results[i].timeline.empty() ? -1 : results[i].timeline.front().pid;
                for (const auto& ts : results[i].timeline) {
                    if (ts.pid != prevPid) cs++;
                    prevPid = ts.pid;
                }
                
                std::cout << "Open Task Manager to observe...\n";
                double secs = CpuStress::runCpuTimeline(results[i].timeline, msPerTick, coreMode, workMultiplier);
                
                std::cout << "  " << name << " | slices=" << results[i].timeline.size()
                          << " cs=" << cs
                          << " replay=" << std::fixed << std::setprecision(3) << secs << "s\n\n";
            }
            std::cout << "=== Comparison completed ===\n\n";
            showComparisonTable = true;  // Show all schedulers' results after replay
        }
    }

    // Only print metrics/comparisons if NOT in single-scheduler replay mode
    if (showComparisonTable) {
        if (replayOnlyIdx >= 0) {
            // Single replay: show only selected scheduler
            std::cout << "\n=== Metrics for selected scheduler ===\n";
            Utils::printMetrics(results[replayOnlyIdx]);
        } else {
            // Normal mode or compare-all replay: show all schedulers
            for (const auto& r : results) Utils::printMetrics(r);
            Utils::printComparisonTable(schedulers, results);
            Utils::recommendScheduler(pm.list(), schedulers, results);
            Utils::printOverallMatrix(schedulers, results);
            Viz::runInteractiveVisualization(schedulers, results, 1200, 700);
        }
    } else if (replayOnlyIdx >= 0) {
        // Single replay mode: show only metrics for that scheduler
        std::cout << "\n=== Detailed Metrics ===\n";
        Utils::printMetrics(results[replayOnlyIdx]);
    }
    return 0;
}


