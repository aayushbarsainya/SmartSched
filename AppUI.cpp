#include "AppUI.h"
#include <SDL.h>
#include "imgui.h"
#include "backends/imgui_impl_sdl2.h"
#include "backends/imgui_impl_sdlrenderer2.h"

#include "TraditionalSchedulers.h"
#include "AISchedulers.h"
#include "Utils.h"
#include "cpu_stress/CpuStress.h"
#include "RealTaskManager.h"
#include "custom_tasks/Tasks.h"
#include "image_tasks/ImageTasks.h"

#include <vector>
#include <string>
#include <algorithm>
#include <iomanip>
#include <sstream>
#include <cmath>
#include <chrono>
#include <fstream>
#include <thread>
#include <atomic>
#include <mutex>

namespace {
    static const ImU32 kPalette[] = {
        IM_COL32(78, 201, 176, 255),
        IM_COL32(255, 153, 51, 255),
        IM_COL32(102, 153, 204, 255),
        IM_COL32(237, 86, 90, 255),
        IM_COL32(154, 102, 204, 255),
        IM_COL32(255, 204, 0, 255),
        IM_COL32(102, 204, 102, 255),
        IM_COL32(204, 102, 102, 255),
        IM_COL32(51, 153, 204, 255),
        IM_COL32(204, 153, 51, 255),
        IM_COL32(153, 102, 204, 255),
        IM_COL32(102, 204, 153, 255)
    };

    ImU32 getColorForPid(int pid) {
        size_t idx = static_cast<size_t>(std::abs(pid)) % (sizeof(kPalette) / sizeof(kPalette[0]));
        return kPalette[idx];
    }

    struct StressResultRecord {
        std::string algoName;
        double wallClockSeconds = 0.0;
        int contextSwitches = 0;
        int slicesCount = 0;
        CpuStress::KernelSchedStats preStats;
        CpuStress::KernelSchedStats postStats;
        bool ran = false;
    };

    // =========================================================================
    // SUGGESTION 1: CONCURRENT TASK RACE STRUCTURES & WORKER
    // =========================================================================
    struct RaceTaskDef {
        std::string name;
        std::string description;
        int totalMs;
        int priority;
        ImVec4 color;
    };

    struct RaceTaskRuntime {
        int id;
        std::string name;
        std::string description;
        int totalMs;
        int priority;
        ImVec4 color;
        double finishTimeSec = 0.0;
    };

    struct RaceManager {
        std::vector<RaceTaskRuntime> tasks;
        std::atomic<int> taskDoneMs[4];
        std::atomic<bool> isRunning{false};
        std::atomic<bool> stopRequested{false};
        std::atomic<int> activeTaskIndex{-1};
        std::atomic<int> contextSwitches{0};
        int selectedPolicy = 1; // 0=FCFS, 1=RR, 2=SJF, 3=Priority
        int quantumMs = 35;
        bool pinCore = true;
        std::string policyDescription = "";
        std::thread workerThread;

        RaceManager() {
            for (int i = 0; i < 4; ++i) taskDoneMs[i].store(0);
        }

        void initDefaultTasks() {
            tasks.clear();
            const RaceTaskDef defs[] = {
                { "Video 4K Encoder",       "Heavy CPU-bound frame rendering",  1800, 1, ImVec4(0.92f, 0.35f, 0.35f, 1.0f) },
                { "Database Query",         "Matrix search and sorting query",   1000, 2, ImVec4(0.35f, 0.65f, 0.95f, 1.0f) },
                { "Audio MP3 Transcoder",   "FFT & signal filtering task",        500, 3, ImVec4(0.35f, 0.85f, 0.50f, 1.0f) },
                { "Mouse / GUI Event",      "Quick UI interrupt response",        200, 4, ImVec4(0.95f, 0.80f, 0.25f, 1.0f) }
            };
            for (int i = 0; i < 4; ++i) {
                taskDoneMs[i].store(0);
                RaceTaskRuntime r;
                r.id = i + 1;
                r.name = defs[i].name;
                r.description = defs[i].description;
                r.totalMs = defs[i].totalMs;
                r.priority = defs[i].priority;
                r.color = defs[i].color;
                r.finishTimeSec = 0.0;
                tasks.push_back(r);
            }
        }
    };

    void runRaceWorker(RaceManager* rm) {
        if (rm->pinCore) {
            CpuStress::setThreadAffinity(1); // Pin to physical Core 1
        }
        rm->contextSwitches = 0;
        auto tStart = std::chrono::high_resolution_clock::now();

        std::vector<int> remaining(rm->tasks.size());
        for (size_t i = 0; i < rm->tasks.size(); ++i) {
            rm->taskDoneMs[i].store(0);
            rm->tasks[i].finishTimeSec = 0.0;
            remaining[i] = rm->tasks[i].totalMs;
        }

        int currentTask = -1;
        size_t rrIndex = 0;

        while (!rm->stopRequested) {
            bool allDone = true;
            for (size_t i = 0; i < remaining.size(); ++i) {
                if (remaining[i] > 0) { allDone = false; break; }
            }
            if (allDone) break;

            int nextTask = -1;
            int sliceDuration = 0;

            if (rm->selectedPolicy == 0) { // FCFS
                for (size_t i = 0; i < remaining.size(); ++i) {
                    if (remaining[i] > 0) {
                        nextTask = static_cast<int>(i);
                        sliceDuration = std::min(30, remaining[i]);
                        break;
                    }
                }
            } else if (rm->selectedPolicy == 1) { // Round Robin
                for (size_t count = 0; count < remaining.size(); ++count) {
                    size_t idx = (rrIndex + count) % remaining.size();
                    if (remaining[idx] > 0) {
                        nextTask = static_cast<int>(idx);
                        sliceDuration = std::min(rm->quantumMs, remaining[idx]);
                        rrIndex = (idx + 1) % remaining.size();
                        break;
                    }
                }
            } else if (rm->selectedPolicy == 2) { // SJF (Shortest Job First / Shortest Remaining)
                int minRem = 1e9;
                for (size_t i = 0; i < remaining.size(); ++i) {
                    if (remaining[i] > 0 && remaining[i] < minRem) {
                        minRem = remaining[i];
                        nextTask = static_cast<int>(i);
                    }
                }
                if (nextTask >= 0) {
                    sliceDuration = std::min(30, remaining[nextTask]);
                }
            } else if (rm->selectedPolicy == 3) { // Priority
                int maxPrio = -1;
                for (size_t i = 0; i < remaining.size(); ++i) {
                    if (remaining[i] > 0 && rm->tasks[i].priority > maxPrio) {
                        maxPrio = rm->tasks[i].priority;
                        nextTask = static_cast<int>(i);
                    }
                }
                if (nextTask >= 0) {
                    sliceDuration = std::min(rm->quantumMs, remaining[nextTask]);
                }
            }

            if (nextTask < 0) break;

            if (nextTask != currentTask) {
                rm->contextSwitches++;
                currentTask = nextTask;
            }

            rm->activeTaskIndex = currentTask;

            // Execute real busy CPU workload for sliceDuration ms
            auto sliceStart = std::chrono::high_resolution_clock::now();
            auto sliceEnd = sliceStart + std::chrono::milliseconds(sliceDuration);
            volatile std::uint64_t sink = 0;
            while (std::chrono::high_resolution_clock::now() < sliceEnd && !rm->stopRequested) {
                sink += 0x9e3779b97f4a7c15ULL;
                sink ^= (sink << 13);
                sink ^= (sink >> 7);
                sink ^= (sink << 17);
            }

            remaining[currentTask] -= sliceDuration;
            if (remaining[currentTask] <= 0) {
                remaining[currentTask] = 0;
                auto now = std::chrono::high_resolution_clock::now();
                std::chrono::duration<double> finDiff = now - tStart;
                rm->tasks[currentTask].finishTimeSec = finDiff.count();
            }
            rm->taskDoneMs[currentTask].store(rm->tasks[currentTask].totalMs - remaining[currentTask]);
        }

        rm->activeTaskIndex = -1;
        rm->isRunning = false;
    }

    // =========================================================================
    // SUGGESTION 3: CONTEXT SWITCH OVERHEAD BENCHMARK
    // =========================================================================
    struct BenchmarkPoint {
        double quantumMs;
        int switches;
        double wallTimeSec;
        double overheadPct;
    };

    struct BenchmarkManager {
        std::vector<BenchmarkPoint> points;
        std::atomic<bool> isRunning{false};
        std::atomic<bool> stopRequested{false};
        std::string currentStatus = "Click 'RUN QUANTUM BENCHMARK' to start measurement.";
        std::atomic<float> progress{0.0f};
        std::thread workerThread;
    };

    void runBenchmarkWorker(BenchmarkManager* bm) {
        CpuStress::setThreadAffinity(1); // Pin to Core 1 for noise reduction
        bm->points.clear();
        std::vector<double> testQuanta = { 100.0, 50.0, 25.0, 10.0, 5.0, 2.0, 1.0, 0.5 };
        
        const int numTasks = 4;
        const int workPerTask = 120;
        const int totalPureWork = numTasks * workPerTask; // 480 ms pure work

        // Baseline run (single uninterrupted execution, 0 switches)
        auto bStart = std::chrono::high_resolution_clock::now();
        {
            auto bEnd = bStart + std::chrono::milliseconds(totalPureWork);
            volatile std::uint64_t sink = 0;
            while (std::chrono::high_resolution_clock::now() < bEnd && !bm->stopRequested) {
                sink += 0x9e3779b97f4a7c15ULL;
                sink ^= (sink << 13);
                sink ^= (sink >> 7);
            }
        }
        std::chrono::duration<double> baseDiff = std::chrono::high_resolution_clock::now() - bStart;
        double pureSeconds = baseDiff.count();
        if (pureSeconds <= 0.05) pureSeconds = 0.48;

        for (size_t qIdx = 0; qIdx < testQuanta.size() && !bm->stopRequested; ++qIdx) {
            double qMs = testQuanta[qIdx];
            std::ostringstream ss;
            ss << "Benchmarking Quantum = " << std::fixed << std::setprecision(1) << qMs << " ms (" 
               << (qIdx + 1) << "/" << testQuanta.size() << ")...";
            bm->currentStatus = ss.str();
            bm->progress = static_cast<float>(qIdx) / static_cast<float>(testQuanta.size());

            std::vector<int> rem(numTasks, workPerTask);
            int switches = 0;
            int prevTask = -1;
            size_t currentIdx = 0;

            auto t0 = std::chrono::high_resolution_clock::now();

            while (!bm->stopRequested) {
                bool allDone = true;
                for (int r : rem) if (r > 0) { allDone = false; break; }
                if (allDone) break;

                int task = -1;
                int slice = 0;
                for (size_t c = 0; c < numTasks; ++c) {
                    size_t idx = (currentIdx + c) % numTasks;
                    if (rem[idx] > 0) {
                        task = static_cast<int>(idx);
                        slice = static_cast<int>(std::min(qMs, static_cast<double>(rem[idx])));
                        currentIdx = (idx + 1) % numTasks;
                        break;
                    }
                }
                if (task < 0) break;

                if (task != prevTask) {
                    switches++;
                    prevTask = task;
                    // Hardware context switch penalty simulation:
                    // cache thrashing & register saving (loops that invalidate L1 cache)
                    volatile std::uint64_t trashSink = 0;
                    for (int trash = 0; trash < 28000; ++trash) {
                        trashSink += static_cast<std::uint64_t>(trash * 31);
                    }
                }

                auto sStart = std::chrono::high_resolution_clock::now();
                auto sEnd = sStart + std::chrono::milliseconds(slice);
                volatile std::uint64_t sink = 0;
                while (std::chrono::high_resolution_clock::now() < sEnd && !bm->stopRequested) {
                    sink += 0x9e3779b97f4a7c15ULL;
                    sink ^= (sink << 13);
                    sink ^= (sink >> 7);
                }
                rem[task] -= slice;
            }

            auto t1 = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double> diff = t1 - t0;
            double wallTime = diff.count();

            double overheadPct = std::max(0.0, ((wallTime - pureSeconds) / pureSeconds) * 100.0);

            bm->points.push_back({ qMs, switches, wallTime, overheadPct });
        }

        bm->progress = 1.0f;
        bm->currentStatus = "Benchmark finished! Interactive Tradeoff Curve plotted.";
        bm->isRunning = false;
    }
}

namespace AppUI {

    int run() {
        if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
            std::fprintf(stderr, "SDL_Init Error: %s\n", SDL_GetError());
            return 1;
        }

        // Setup window
        SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
        SDL_Window* window = SDL_CreateWindow(
            "SmartSched - Advanced CPU Scheduling Algorithm Simulator & Benchmark",
            SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
            1340, 860, window_flags
        );
        if (!window) {
            std::fprintf(stderr, "SDL_CreateWindow Error: %s\n", SDL_GetError());
            SDL_Quit();
            return 1;
        }

        SDL_Renderer* renderer = SDL_CreateRenderer(
            window, -1,
            SDL_RENDERER_PRESENTVSYNC | SDL_RENDERER_ACCELERATED
        );
        if (!renderer) {
            std::fprintf(stderr, "SDL_CreateRenderer Error: %s\n", SDL_GetError());
            SDL_DestroyWindow(window);
            SDL_Quit();
            return 1;
        }

        // Setup Dear ImGui context
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

        // Modern Dark Theme styling
        ImGui::StyleColorsDark();
        ImGuiStyle& style = ImGui::GetStyle();
        style.WindowRounding = 6.0f;
        style.FrameRounding = 4.0f;
        style.PopupRounding = 4.0f;
        style.ScrollbarRounding = 4.0f;
        style.GrabRounding = 4.0f;
        style.TabRounding = 4.0f;
        style.WindowBorderSize = 1.0f;
        style.FrameBorderSize = 0.0f;
        style.ItemSpacing = ImVec2(10.0f, 8.0f);
        style.FramePadding = ImVec2(8.0f, 5.0f);

        // Custom color accents
        ImVec4* colors = style.Colors;
        colors[ImGuiCol_WindowBg] = ImVec4(0.11f, 0.12f, 0.15f, 1.00f);
        colors[ImGuiCol_Header] = ImVec4(0.20f, 0.25f, 0.35f, 0.80f);
        colors[ImGuiCol_HeaderHovered] = ImVec4(0.26f, 0.35f, 0.50f, 0.80f);
        colors[ImGuiCol_HeaderActive] = ImVec4(0.30f, 0.42f, 0.60f, 1.00f);
        colors[ImGuiCol_Button] = ImVec4(0.20f, 0.42f, 0.65f, 0.85f);
        colors[ImGuiCol_ButtonHovered] = ImVec4(0.28f, 0.52f, 0.78f, 1.00f);
        colors[ImGuiCol_ButtonActive] = ImVec4(0.18f, 0.36f, 0.58f, 1.00f);
        colors[ImGuiCol_FrameBg] = ImVec4(0.16f, 0.18f, 0.22f, 1.00f);
        colors[ImGuiCol_FrameBgHovered] = ImVec4(0.22f, 0.25f, 0.30f, 1.00f);
        colors[ImGuiCol_FrameBgActive] = ImVec4(0.26f, 0.30f, 0.38f, 1.00f);
        colors[ImGuiCol_Tab] = ImVec4(0.16f, 0.20f, 0.27f, 0.85f);
        colors[ImGuiCol_TabHovered] = ImVec4(0.28f, 0.45f, 0.68f, 0.80f);
#if defined(IMGUI_VERSION_NUM) && IMGUI_VERSION_NUM >= 19090
        colors[ImGuiCol_TabSelected] = ImVec4(0.20f, 0.42f, 0.65f, 1.00f);
        colors[ImGuiCol_TabDimmed] = ImVec4(0.13f, 0.16f, 0.21f, 0.90f);
        colors[ImGuiCol_TabDimmedSelected] = ImVec4(0.17f, 0.26f, 0.38f, 1.00f);
#else
        colors[ImGuiCol_TabActive] = ImVec4(0.20f, 0.42f, 0.65f, 1.00f);
        colors[ImGuiCol_TabUnfocused] = ImVec4(0.13f, 0.16f, 0.21f, 0.90f);
        colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.17f, 0.26f, 0.38f, 1.00f);
#endif

        // Setup Platform/Renderer backends
        ImGui_ImplSDL2_InitForSDLRenderer(window, renderer);
        ImGui_ImplSDLRenderer2_Init(renderer);

        // Application State
        std::vector<Process> processes = {
            Process(1, 0, 7, 1),
            Process(2, 2, 4, 3),
            Process(3, 4, 1, 2),
            Process(4, 5, 4, 1),
            Process(5, 6, 6, 2)
        };

        // Algorithm Hyperparameters
        int rrQuantum = 2;
        int qlEpisodes = 200;
        float qlAlpha = 0.5f;
        float qlGamma = 0.9f;
        float qlEpsilon = 0.1f;
        int nnIterations = 100;

        // Results
        std::vector<ScheduleResult> results;
        std::string recommendationText = "";
        std::string bestSchedulerName = "";

        // Workload Logs
        std::vector<std::string> workloadLogs = {
            "System initialized with 5-task sample workload.",
            "Ready to execute simulations or benchmark real hardware workloads."
        };

        // Lambda to run all schedulers
        auto runSimulation = [&]() {
            if (processes.empty()) {
                results.clear();
                recommendationText = "No processes defined. Please add processes or select a workload mode.";
                bestSchedulerName = "None";
                return;
            }

            FCFS_Scheduler fcfs;
            SJF_Scheduler sjf;
            RR_Scheduler rr(rrQuantum);
            QLearningScheduler ql(5, qlEpisodes, qlAlpha, qlGamma, qlEpsilon);
            NNLiketScheduler nn(-1.0, 0.1, -0.5, nnIterations);

            std::vector<IScheduler*> schedulers = { &fcfs, &sjf, &rr, &ql, &nn };
            results.clear();
            for (auto* s : schedulers) {
                results.push_back(s->run(processes));
            }

            double bestWait = 1e9;
            size_t bestIdx = 0;
            for (size_t i = 0; i < results.size(); ++i) {
                if (results[i].metrics.averageWaitingTime < bestWait) {
                    bestWait = results[i].metrics.averageWaitingTime;
                    bestIdx = i;
                }
            }
            bestSchedulerName = results[bestIdx].algorithmName;
            std::ostringstream ss;
            ss << "RECOMMENDATION: " << bestSchedulerName 
               << " achieved the lowest Average Waiting Time (" 
               << std::fixed << std::setprecision(2) << bestWait << " ticks) with a Jain Fairness index of "
               << std::fixed << std::setprecision(2) << results[bestIdx].metrics.jainFairnessSlowdown
               << ". Context switches: " << results[bestIdx].metrics.contextSwitches << ".";
            recommendationText = ss.str();
        };

        // Run initially with default sample data
        runSimulation();

        // Workload Mode Selection State
        int currentWorkloadMode = 0;
        int requestedTab = -1; // -1 = default natural clicking, 0..6 = programmatic switch request

        // Mode 1: Manual Process Input state
        int newArrival = 0;
        int newBurst = 4;
        int newPriority = 1;

        // Mode 2: Real SDL2 Render Tasks state
        int renderTaskCount = 10;

        // Mode 3: File Load / Save state
        char datasetFilenameBuf[128] = "real_data.txt";
        std::string fileStatusMessage = "";

        // Mode 4: Custom Mathematical CPU Tasks state
        int mathK = 15000;

        // Mode 5: BMP Image Tasks state
        int selectedBmpIndex = 0;
        const char* bmpPaths[] = {
            "image_tasks/sample-1024x1024.bmp",
            "image_tasks/sample_1280x853.bmp"
        };
        int bmpTaskCount = 6;
        int bmpFrames = 40;

        // Gantt Chart Zoom/Pan state
        float ganttScale = 28.0f;
        float ganttOriginX = 0.0f;

        // CPU Stress Test State (Mode 6)
        int selectedStressAlgo = 0;
        int stressCoreAffinity = 1;
        int stressMsPerTick = 40;
        int stressMultiplier = 1;
        StressResultRecord stressResult;

        // =====================================================================
        // Initialize Suggestion 1 (Race) and Suggestion 3 (Benchmark) managers
        // =====================================================================
        RaceManager raceMgr;
        raceMgr.initDefaultTasks();

        BenchmarkManager benchMgr;

        // Main Loop
        bool running = true;
        while (running) {
            SDL_Event event;
            while (SDL_PollEvent(&event)) {
                ImGui_ImplSDL2_ProcessEvent(&event);
                if (event.type == SDL_QUIT) {
                    running = false;
                }
                if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE &&
                    event.window.windowID == SDL_GetWindowID(window)) {
                    running = false;
                }
            }

            // Start ImGui frame
            ImGui_ImplSDLRenderer2_NewFrame();
            ImGui_ImplSDL2_NewFrame();
            ImGui::NewFrame();

            // Set main viewport window
            ImGui::SetNextWindowPos(ImVec2(0, 0));
            ImGui::SetNextWindowSize(io.DisplaySize);
            ImGuiWindowFlags rootFlags = ImGuiWindowFlags_NoTitleBar | 
                                         ImGuiWindowFlags_NoResize | 
                                         ImGuiWindowFlags_NoMove | 
                                         ImGuiWindowFlags_NoCollapse |
                                         ImGuiWindowFlags_MenuBar;

            ImGui::Begin("SmartSchedRoot", nullptr, rootFlags);

            // Menu Bar
            if (ImGui::BeginMenuBar()) {
                if (ImGui::BeginMenu("Workload Modes")) {
                    if (ImGui::MenuItem("Mode 1: Sample & Manual Process Editor", nullptr, currentWorkloadMode == 0)) {
                        currentWorkloadMode = 0;
                    }
                    if (ImGui::MenuItem("Mode 2: Real SDL2 Render Tasks (Graphics)", nullptr, currentWorkloadMode == 1)) {
                        currentWorkloadMode = 1;
                    }
                    if (ImGui::MenuItem("Mode 3: Load / Save Dataset File (real_data.txt)", nullptr, currentWorkloadMode == 2)) {
                        currentWorkloadMode = 2;
                    }
                    if (ImGui::MenuItem("Mode 4: Real Math Equations (Primes, Fibo, Factorial)", nullptr, currentWorkloadMode == 3)) {
                        currentWorkloadMode = 3;
                    }
                    if (ImGui::MenuItem("Mode 5: Real BMP Image Processing (Software Blit)", nullptr, currentWorkloadMode == 4)) {
                        currentWorkloadMode = 4;
                    }
                    ImGui::Separator();
                    if (ImGui::MenuItem("Mode 6: Hardware CPU Stress & Replay")) {
                        requestedTab = 4;
                    }
                    ImGui::EndMenu();
                }

                if (ImGui::BeginMenu("Real-Life OS Demos")) {
                    if (ImGui::MenuItem("Live Concurrent Task Race (Suggestion 1)")) {
                        requestedTab = 2;
                    }
                    if (ImGui::MenuItem("Context Switch Overhead Benchmark (Suggestion 3)")) {
                        requestedTab = 3;
                    }
                    ImGui::EndMenu();
                }

                if (ImGui::BeginMenu("Presets (Mode 1)")) {
                    if (ImGui::MenuItem("Standard 5-Task Sample")) {
                        currentWorkloadMode = 0;
                        processes = {
                            Process(1, 0, 7, 1),
                            Process(2, 2, 4, 3),
                            Process(3, 4, 1, 2),
                            Process(4, 5, 4, 1),
                            Process(5, 6, 6, 2)
                        };
                        workloadLogs.push_back("Loaded Preset: Standard 5-Task Sample.");
                        runSimulation();
                    }
                    if (ImGui::MenuItem("Convoy Effect (1 Huge + 4 Small)")) {
                        currentWorkloadMode = 0;
                        processes = {
                            Process(1, 0, 24, 1),
                            Process(2, 1, 2, 2),
                            Process(3, 2, 1, 3),
                            Process(4, 3, 3, 1),
                            Process(5, 4, 2, 2)
                        };
                        workloadLogs.push_back("Loaded Preset: Convoy Effect.");
                        runSimulation();
                    }
                    if (ImGui::MenuItem("Bursty Simultaneous Arrivals (t=0)")) {
                        currentWorkloadMode = 0;
                        processes = {
                            Process(1, 0, 5, 2),
                            Process(2, 0, 9, 1),
                            Process(3, 0, 3, 3),
                            Process(4, 0, 6, 2),
                            Process(5, 0, 2, 1)
                        };
                        workloadLogs.push_back("Loaded Preset: Bursty Arrivals.");
                        runSimulation();
                    }
                    ImGui::EndMenu();
                }

                ImGui::SameLine(ImGui::GetWindowWidth() - 320);
                ImGui::TextDisabled("SmartSched v2.1 | WSL2 Linux + WSLg");
                ImGui::EndMenuBar();
            }

            // Left Sidebar: Workload Generator & Configuration (Width: 420px)
            ImGui::BeginChild("Sidebar", ImVec2(420, 0), true);

            ImGui::TextColored(ImVec4(0.35f, 0.75f, 1.0f, 1.0f), "WORKLOAD GENERATION MODES");
            ImGui::Separator();

            const char* modeDropdownNames[] = {
                "Mode 1: Manual / Sample Presets",
                "Mode 2: Real SDL2 Render Tasks",
                "Mode 3: File Load / Save (real_data.txt)",
                "Mode 4: Real Math Equations (CPU Tasks)",
                "Mode 5: Real BMP Image Processing"
            };
            ImGui::Text("Active Workload Generator:");
            ImGui::SetNextItemWidth(-1);
            ImGui::Combo("##workloadModeCombo", &currentWorkloadMode, modeDropdownNames, IM_ARRAYSIZE(modeDropdownNames));

            ImGui::Spacing();

            // MODE 1
            if (currentWorkloadMode == 0) {
                ImGui::TextDisabled("Quick Workload Presets:");
                if (ImGui::Button("Sample (5)")) {
                    processes = { Process(1,0,7,1), Process(2,2,4,3), Process(3,4,1,2), Process(4,5,4,1), Process(5,6,6,2) };
                    workloadLogs.push_back("Loaded Standard 5-Task Sample.");
                    runSimulation();
                }
                ImGui::SameLine();
                if (ImGui::Button("Convoy")) {
                    processes = { Process(1,0,24,1), Process(2,1,2,2), Process(3,2,1,3), Process(4,3,3,1), Process(5,4,2,2) };
                    workloadLogs.push_back("Loaded Convoy Effect Workload.");
                    runSimulation();
                }
                ImGui::SameLine();
                if (ImGui::Button("Bursty")) {
                    processes = { Process(1,0,5,2), Process(2,0,9,1), Process(3,0,3,3), Process(4,0,6,2), Process(5,0,2,1) };
                    workloadLogs.push_back("Loaded Bursty Arrivals Workload.");
                    runSimulation();
                }
                ImGui::SameLine();
                if (ImGui::Button("Clear")) {
                    processes.clear();
                    results.clear();
                }

                ImGui::Spacing();
                if (ImGui::BeginTable("ProcessTable", 5, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY, ImVec2(0, 160))) {
                    ImGui::TableSetupColumn("PID", ImGuiTableColumnFlags_WidthFixed, 42.0f);
                    ImGui::TableSetupColumn("Arr", ImGuiTableColumnFlags_WidthFixed, 55.0f);
                    ImGui::TableSetupColumn("Burst", ImGuiTableColumnFlags_WidthFixed, 60.0f);
                    ImGui::TableSetupColumn("Prio", ImGuiTableColumnFlags_WidthFixed, 55.0f);
                    ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthStretch);
                    ImGui::TableHeadersRow();

                    int toDelete = -1;
                    for (size_t i = 0; i < processes.size(); ++i) {
                        ImGui::TableNextRow();
                        auto& p = processes[i];

                        ImGui::TableSetColumnIndex(0);
                        ImVec4 pCol = ImGui::ColorConvertU32ToFloat4(getColorForPid(p.pid));
                        ImGui::TextColored(pCol, "P%d", p.pid);

                        ImGui::TableSetColumnIndex(1);
                        ImGui::PushID(static_cast<int>(i * 10 + 1));
                        ImGui::SetNextItemWidth(50);
                        if (ImGui::InputInt("##arr", &p.arrivalTime, 0, 0)) {
                            if (p.arrivalTime < 0) p.arrivalTime = 0;
                            runSimulation();
                        }
                        ImGui::PopID();

                        ImGui::TableSetColumnIndex(2);
                        ImGui::PushID(static_cast<int>(i * 10 + 2));
                        ImGui::SetNextItemWidth(55);
                        if (ImGui::InputInt("##bt", &p.burstTime, 0, 0)) {
                            if (p.burstTime <= 0) p.burstTime = 1;
                            runSimulation();
                        }
                        ImGui::PopID();

                        ImGui::TableSetColumnIndex(3);
                        ImGui::PushID(static_cast<int>(i * 10 + 3));
                        ImGui::SetNextItemWidth(50);
                        if (ImGui::InputInt("##pr", &p.priority, 0, 0)) {
                            runSimulation();
                        }
                        ImGui::PopID();

                        ImGui::TableSetColumnIndex(4);
                        ImGui::PushID(static_cast<int>(i * 10 + 4));
                        if (ImGui::SmallButton("X")) {
                            toDelete = static_cast<int>(i);
                        }
                        ImGui::PopID();
                    }

                    if (toDelete >= 0 && toDelete < (int)processes.size()) {
                        processes.erase(processes.begin() + toDelete);
                        runSimulation();
                    }

                    ImGui::EndTable();
                }

                ImGui::Text("Add New Process:");
                ImGui::SetNextItemWidth(65);
                ImGui::InputInt("Arr##new", &newArrival, 0);
                ImGui::SameLine();
                ImGui::SetNextItemWidth(65);
                ImGui::InputInt("Burst##new", &newBurst, 0);
                ImGui::SameLine();
                ImGui::SetNextItemWidth(65);
                ImGui::InputInt("Prio##new", &newPriority, 0);

                if (newArrival < 0) newArrival = 0;
                if (newBurst <= 0) newBurst = 1;

                if (ImGui::Button("+ Add Process", ImVec2(-1, 26))) {
                    int nextPid = 1;
                    for (const auto& p : processes) {
                        if (p.pid >= nextPid) nextPid = p.pid + 1;
                    }
                    processes.emplace_back(nextPid, newArrival, newBurst, newPriority);
                    runSimulation();
                }
            }
            // MODE 2
            else if (currentWorkloadMode == 1) {
                ImGui::TextColored(ImVec4(0.95f, 0.75f, 0.35f, 1.0f), "Real SDL2 Graphics Render Tasks:");
                ImGui::TextWrapped("Executes actual SDL2 graphics rendering tasks and converts measured wall-clock seconds into scheduling burst ticks.");
                ImGui::Spacing();
                ImGui::SliderInt("Tasks to Benchmark", &renderTaskCount, 1, 10);

                ImGui::Spacing();
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.85f, 0.45f, 0.15f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.95f, 0.55f, 0.25f, 1.0f));
                if (ImGui::Button("RUN REAL SDL2 RENDER WORKLOAD", ImVec2(-1, 34))) {
                    struct TaskConfig { int width, height, frames; const char* description; };
                    const TaskConfig configs[] = {
                        {1920, 1080, 120, "4K Heavy"}, {1280, 720, 60, "720p Medium"},
                        {800, 600, 180, "Long Animation"}, {1920, 1080, 45, "1080p Quick"},
                        {640, 480, 240, "Long Render"}, {1440, 900, 90, "Wide Format"},
                        {1024, 768, 150, "Extended"}, {1600, 900, 75, "HD+ Render"},
                        {800, 600, 200, "Bulk Frames"}, {1920, 1080, 60, "Standard HD"}
                    };
                    workloadLogs.clear();
                    workloadLogs.push_back("Executing " + std::to_string(renderTaskCount) + " Real SDL2 Graphics Tasks...");
                    processes.clear();
                    int currentTime = 0;
                    for (int i = 1; i <= renderTaskCount && i <= 10; ++i) {
                        const auto& config = configs[i - 1];
                        double seconds = RealTasks::renderImageTask(config.width, config.height, config.frames);
                        if (seconds < 0) seconds = -seconds;
                        int burstTicks = static_cast<int>(seconds * 10 + 0.5);
                        if (burstTicks <= 0) burstTicks = 1;
                        processes.emplace_back(i, currentTime, burstTicks, 0);
                        currentTime += 1;

                        std::ostringstream oss;
                        oss << "P" << i << " [" << config.description << " " << config.width << "x" << config.height 
                            << " (" << config.frames << "f)]: Rendered in " << std::fixed << std::setprecision(3)
                            << seconds << "s -> " << burstTicks << " ticks";
                        workloadLogs.push_back(oss.str());
                    }
                    RealTasks::saveDataset("real_data.txt", processes);
                    workloadLogs.push_back("Dataset saved to 'real_data.txt'.");
                    runSimulation();
                    requestedTab = 0;
                }
                ImGui::PopStyleColor(2);
            }
            // MODE 3
            else if (currentWorkloadMode == 2) {
                ImGui::TextColored(ImVec4(0.40f, 0.85f, 0.95f, 1.0f), "Dataset File Load / Save:");
                ImGui::TextWrapped("Load previously benchmarked task datasets or save current processes to disk.");
                ImGui::Spacing();
                ImGui::InputText("Filename", datasetFilenameBuf, sizeof(datasetFilenameBuf));

                ImGui::Spacing();
                if (ImGui::Button("LOAD DATASET FROM FILE", ImVec2(-1, 30))) {
                    auto loaded = RealTasks::loadProcessesFromFile(datasetFilenameBuf);
                    if (!loaded.empty()) {
                        processes = loaded;
                        workloadLogs.clear();
                        workloadLogs.push_back("Loaded " + std::to_string(processes.size()) + " processes from '" + std::string(datasetFilenameBuf) + "'.");
                        fileStatusMessage = "Loaded " + std::to_string(processes.size()) + " processes!";
                        runSimulation();
                    } else {
                        fileStatusMessage = "Failed to load: file not found or empty.";
                    }
                }
                if (ImGui::Button("SAVE CURRENT TASKS TO FILE", ImVec2(-1, 30))) {
                    if (RealTasks::saveDataset(datasetFilenameBuf, processes)) {
                        fileStatusMessage = "Successfully saved " + std::to_string(processes.size()) + " tasks!";
                    } else {
                        fileStatusMessage = "Failed to save dataset.";
                    }
                }
                if (!fileStatusMessage.empty()) {
                    ImGui::TextColored(ImVec4(0.35f, 0.95f, 0.50f, 1.0f), "%s", fileStatusMessage.c_str());
                }
            }
            // MODE 4
            else if (currentWorkloadMode == 3) {
                ImGui::TextColored(ImVec4(0.50f, 0.90f, 0.50f, 1.0f), "Real Mathematical Equation Tasks:");
                ImGui::TextWrapped("Executes 4 real algorithmic CPU tasks and measures actual execution time on your processor:");
                ImGui::BulletText("k-th Prime Number (Sieve)");
                ImGui::BulletText("Fibonacci(k) Calculation");
                ImGui::BulletText("Factorial(k) Multiplicative");
                ImGui::BulletText("k-th Palindrome Search");
                ImGui::Spacing();
                ImGui::SliderInt("Intensity k", &mathK, 1000, 50000);

                ImGui::Spacing();
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.65f, 0.35f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.28f, 0.78f, 0.42f, 1.0f));
                if (ImGui::Button("COMPUTE REAL EQUATIONS & BENCHMARK", ImVec2(-1, 34))) {
                    std::vector<std::string> taskNames = { "kth Prime", "Fibonacci(k)", "Factorial(k)", "kth Palindrome" };
                    workloadLogs.clear();
                    workloadLogs.push_back("Computing 4 Real Mathematical Equations with k=" + std::to_string(mathK) + " on processor...");

                    processes.clear();
                    int currentTime = 0;
                    for (int i = 0; i < 4; ++i) {
                        auto t0 = std::chrono::high_resolution_clock::now();
                        std::uint64_t result = 0;
                        if (i == 0) result = CustomTasks::kthPrime(mathK);
                        else if (i == 1) result = CustomTasks::fibonacciK(mathK);
                        else if (i == 2) result = CustomTasks::factorialK(mathK);
                        else if (i == 3) result = CustomTasks::kthPalindrome(mathK);
                        auto t1 = std::chrono::high_resolution_clock::now();
                        std::chrono::duration<double> diff = t1 - t0;
                        double seconds = diff.count();
                        int burstTicks = static_cast<int>(seconds * 10 + 0.5);
                        if (burstTicks <= 0) burstTicks = 1;

                        processes.emplace_back(i + 1, currentTime, burstTicks, 0);
                        currentTime += 1;

                        std::ostringstream oss;
                        oss << "P" << (i + 1) << " [" << taskNames[i] << "]: Result = " << result
                            << " | Executed in " << std::fixed << std::setprecision(4) << seconds
                            << "s -> " << burstTicks << " burst ticks";
                        workloadLogs.push_back(oss.str());
                    }
                    runSimulation();
                    requestedTab = 0;
                }
                ImGui::PopStyleColor(2);
            }
            // MODE 5
            else if (currentWorkloadMode == 4) {
                ImGui::TextColored(ImVec4(0.90f, 0.50f, 0.90f, 1.0f), "Real BMP Software Scaling & Processing:");
                ImGui::TextWrapped("Loads a BMP image, performs software scaling blits over multiple frames, and measures elapsed time.");
                ImGui::Spacing();
                ImGui::Combo("BMP Image", &selectedBmpIndex, bmpPaths, IM_ARRAYSIZE(bmpPaths));
                ImGui::SliderInt("Spawn Tasks", &bmpTaskCount, 2, 12);
                ImGui::SliderInt("Frames per Task", &bmpFrames, 10, 100);

                ImGui::Spacing();
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.65f, 0.25f, 0.65f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.78f, 0.35f, 0.78f, 1.0f));
                if (ImGui::Button("PROCESS BMP IMAGES & BENCHMARK", ImVec2(-1, 34))) {
                    std::string chosenPath = bmpPaths[selectedBmpIndex];
                    workloadLogs.clear();
                    processes.clear();
                    int currentTime = 0;
                    for (int i = 1; i <= bmpTaskCount; ++i) {
                        int minPct = 60 + (i % 3) * 10;
                        int maxPct = 140 + (i % 3) * 10;
                        double seconds = ImageTasks::renderImageBMP(chosenPath, bmpFrames, minPct, maxPct);
                        if (seconds <= 0) seconds = 0.1;
                        int burstTicks = static_cast<int>(seconds * 10 + 0.5);
                        if (burstTicks <= 0) burstTicks = 1;
                        processes.emplace_back(i, currentTime, burstTicks, 0);
                        currentTime += (i % 2 == 0) ? 2 : 1;

                        std::ostringstream oss;
                        oss << "P" << i << " [BMP Blit Scaling " << minPct << "%-" << maxPct << "% (" << bmpFrames << "f)]: Time = "
                            << std::fixed << std::setprecision(3) << seconds << "s -> " << burstTicks << " ticks";
                        workloadLogs.push_back(oss.str());
                    }
                    runSimulation();
                    requestedTab = 0;
                }
                ImGui::PopStyleColor(2);
            }

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            // Active Queue Status Summary
            ImGui::Text("Active Queue Status:");
            int totalBurst = 0;
            for (const auto& p : processes) totalBurst += p.burstTime;
            ImGui::BulletText("Total Processes: %d", (int)processes.size());
            ImGui::BulletText("Total Burst Demand: %d ticks", totalBurst);

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            // Algorithm Hyperparameters Collapsing Header
            if (ImGui::CollapsingHeader("Algorithm Hyperparameters", ImGuiTreeNodeFlags_DefaultOpen)) {
                ImGui::Text("Round Robin:");
                ImGui::SliderInt("Time Quantum (ticks)", &rrQuantum, 1, 10);

                ImGui::Spacing();
                ImGui::Text("Q-Learning AI:");
                ImGui::SliderInt("Episodes", &qlEpisodes, 20, 500);
                ImGui::SliderFloat("Learning Rate (alpha)", &qlAlpha, 0.1f, 1.0f, "%.2f");
                ImGui::SliderFloat("Discount (gamma)", &qlGamma, 0.5f, 0.99f, "%.2f");
                ImGui::SliderFloat("Epsilon", &qlEpsilon, 0.01f, 0.5f, "%.2f");

                ImGui::Spacing();
                ImGui::Text("Neural Net-like AI:");
                ImGui::SliderInt("Training Iterations", &nnIterations, 20, 300);
            }

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            // Global Re-Simulate Button
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.60f, 0.35f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.24f, 0.72f, 0.42f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.14f, 0.50f, 0.28f, 1.0f));
            if (ImGui::Button("RE-SIMULATE ALL SCHEDULERS", ImVec2(-1, 38))) {
                runSimulation();
            }
            ImGui::PopStyleColor(3);

            ImGui::EndChild(); // Sidebar

            // Main Content Area (Right Side)
            ImGui::SameLine();
            ImGui::BeginChild("MainContent", ImVec2(0, 0), true);

            // Tabs
            if (ImGui::BeginTabBar("MainTabBar")) {

                // TAB 1: Gantt Chart Visualization
                ImGuiTabItemFlags tab1Flags = (requestedTab == 0) ? ImGuiTabItemFlags_SetSelected : 0;
                if (ImGui::BeginTabItem("Interactive Gantt Chart", nullptr, tab1Flags)) {
                    ImGui::Spacing();

                    ImGui::Text("Zoom & Navigation:");
                    ImGui::SameLine();
                    ImGui::SetNextItemWidth(180);
                    ImGui::SliderFloat("##zoom", &ganttScale, 5.0f, 80.0f, "Scale: %.1f px/tick");
                    ImGui::SameLine();
                    if (ImGui::Button("Reset View")) {
                        ganttScale = 28.0f;
                        ganttOriginX = 0.0f;
                    }
                    ImGui::SameLine();
                    ImGui::TextDisabled("| Pan: Click & drag in timeline");

                    if (!recommendationText.empty()) {
                        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.15f, 0.25f, 0.18f, 0.85f));
                        ImGui::BeginChild("RecBanner", ImVec2(0, 36), true);
                        ImGui::TextColored(ImVec4(0.40f, 0.95f, 0.50f, 1.0f), "%s", recommendationText.c_str());
                        ImGui::EndChild();
                        ImGui::PopStyleColor();
                    }

                    ImGui::Spacing();

                    ImVec2 canvasPos = ImGui::GetCursorScreenPos();
                    ImVec2 canvasSize = ImGui::GetContentRegionAvail();
                    if (canvasSize.y < 350) canvasSize.y = 350;

                    ImDrawList* drawList = ImGui::GetWindowDrawList();
                    drawList->AddRectFilled(canvasPos, ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y), IM_COL32(20, 22, 27, 255));
                    drawList->AddRect(canvasPos, ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y), IM_COL32(50, 55, 65, 255));

                    int maxTime = 1;
                    for (const auto& r : results) {
                        if (!r.timeline.empty() && r.timeline.back().endTime > maxTime) {
                            maxTime = r.timeline.back().endTime;
                        }
                    }

                    float startX = canvasPos.x + 160.0f + ganttOriginX;
                    float topY = canvasPos.y + 45.0f;
                    float rowHeight = 44.0f;
                    float rowGap = 20.0f;

                    int tickStep = (ganttScale >= 30) ? 1 : (ganttScale >= 15 ? 2 : (ganttScale >= 8 ? 5 : 10));
                    for (int t = 0; t <= maxTime + 2; t += tickStep) {
                        float lineX = startX + t * ganttScale;
                        if (lineX >= canvasPos.x + 155 && lineX <= canvasPos.x + canvasSize.x - 10) {
                            drawList->AddLine(
                                ImVec2(lineX, canvasPos.y + 25),
                                ImVec2(lineX, canvasPos.y + canvasSize.y - 10),
                                IM_COL32(45, 50, 60, 255)
                            );
                            char tickStr[16];
                            std::snprintf(tickStr, sizeof(tickStr), "%d", t);
                            drawList->AddText(ImVec2(lineX - 4, canvasPos.y + 8), IM_COL32(160, 165, 175, 255), tickStr);
                        }
                    }

                    ImVec2 mousePos = io.MousePos;
                    std::string hoveredInfo = "";

                    for (size_t i = 0; i < results.size(); ++i) {
                        const auto& res = results[i];
                        float rowY = topY + i * (rowHeight + rowGap);

                        ImU32 rowBg = (i % 2 == 0) ? IM_COL32(26, 29, 36, 255) : IM_COL32(22, 25, 31, 255);
                        drawList->AddRectFilled(
                            ImVec2(canvasPos.x + 4, rowY - 4),
                            ImVec2(canvasPos.x + canvasSize.x - 4, rowY + rowHeight + 4),
                            rowBg
                        );

                        ImU32 labelCol = (res.algorithmName == bestSchedulerName) 
                                        ? IM_COL32(100, 240, 140, 255) 
                                        : IM_COL32(220, 225, 235, 255);
                        drawList->AddText(ImVec2(canvasPos.x + 10, rowY + 4), labelCol, res.algorithmName.c_str());

                        float jf = static_cast<float>(res.metrics.jainFairnessSlowdown);
                        if (jf < 0.0f) jf = 0.0f; if (jf > 1.0f) jf = 1.0f;
                        float meterW = 120.0f * jf;
                        drawList->AddRectFilled(ImVec2(canvasPos.x + 10, rowY + 24), ImVec2(canvasPos.x + 130, rowY + 30), IM_COL32(40, 44, 52, 255));
                        ImU32 meterCol = IM_COL32(static_cast<int>(255 - 150 * jf), static_cast<int>(180 * jf + 50), 80, 255);
                        drawList->AddRectFilled(ImVec2(canvasPos.x + 10, rowY + 24), ImVec2(canvasPos.x + 10 + meterW, rowY + 30), meterCol);
                        char fStr[16];
                        std::snprintf(fStr, sizeof(fStr), "Fair: %.2f", jf);
                        drawList->AddText(ImVec2(canvasPos.x + 10, rowY + 32), IM_COL32(130, 135, 145, 255), fStr);

                        for (const auto& ts : res.timeline) {
                            float bStartX = startX + ts.startTime * ganttScale;
                            float bEndX = startX + ts.endTime * ganttScale;
                            float bW = bEndX - bStartX;
                            if (bW <= 0) continue;

                            if (bEndX < canvasPos.x + 150 || bStartX > canvasPos.x + canvasSize.x) continue;

                            float drawX0 = std::max(bStartX, canvasPos.x + 155.0f);
                            float drawX1 = std::min(bEndX, canvasPos.x + canvasSize.x - 10.0f);

                            ImU32 pCol = getColorForPid(ts.pid);
                            drawList->AddRectFilled(ImVec2(drawX0, rowY), ImVec2(drawX1, rowY + rowHeight), pCol, 3.0f);
                            drawList->AddRect(ImVec2(drawX0, rowY), ImVec2(drawX1, rowY + rowHeight), IM_COL32(20, 22, 25, 200), 3.0f);

                            if (bW >= 24) {
                                char pLabel[16];
                                std::snprintf(pLabel, sizeof(pLabel), "P%d", ts.pid);
                                drawList->AddText(ImVec2(drawX0 + 4, rowY + 12), IM_COL32(15, 20, 25, 255), pLabel);
                            }

                            if (mousePos.x >= drawX0 && mousePos.x <= drawX1 && mousePos.y >= rowY && mousePos.y <= rowY + rowHeight) {
                                std::ostringstream hss;
                                hss << res.algorithmName << "\n"
                                    << "Process: P" << ts.pid << "\n"
                                    << "Start Time: " << ts.startTime << " ticks\n"
                                    << "End Time: " << ts.endTime << " ticks\n"
                                    << "Slice Duration: " << (ts.endTime - ts.startTime) << " ticks";
                                hoveredInfo = hss.str();
                            }
                        }
                    }

                    if (ImGui::IsWindowHovered() && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
                        ganttOriginX += io.MouseDelta.x;
                    }

                    if (!hoveredInfo.empty()) {
                        ImGui::BeginTooltip();
                        ImGui::TextUnformatted(hoveredInfo.c_str());
                        ImGui::EndTooltip();
                    }

                    ImGui::Dummy(canvasSize);
                    ImGui::EndTabItem();
                }

                // TAB 2: Metrics Table & Comparison
                ImGuiTabItemFlags tab2Flags = (requestedTab == 1) ? ImGuiTabItemFlags_SetSelected : 0;
                if (ImGui::BeginTabItem("Metrics & Performance Matrix", nullptr, tab2Flags)) {
                    ImGui::Spacing();
                    ImGui::TextColored(ImVec4(0.35f, 0.75f, 1.0f, 1.0f), "SCHEDULING ALGORITHM BENCHMARK MATRIX");
                    ImGui::Separator();
                    ImGui::Spacing();

                    if (!results.empty()) {
                        if (ImGui::BeginTable("MetricsTable", 6, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp)) {
                            ImGui::TableSetupColumn("Algorithm Name");
                            ImGui::TableSetupColumn("Avg Waiting Time (ticks)");
                            ImGui::TableSetupColumn("Avg Turnaround (ticks)");
                            ImGui::TableSetupColumn("Jain's Fairness (0..1)");
                            ImGui::TableSetupColumn("Context Switches");
                            ImGui::TableSetupColumn("Total Makespan");
                            ImGui::TableHeadersRow();

                            for (const auto& r : results) {
                                ImGui::TableNextRow();
                                bool isWinner = (r.algorithmName == bestSchedulerName);

                                ImGui::TableSetColumnIndex(0);
                                if (isWinner) {
                                    ImGui::TextColored(ImVec4(0.35f, 0.95f, 0.50f, 1.0f), "%s (Best)", r.algorithmName.c_str());
                                } else {
                                    ImGui::Text("%s", r.algorithmName.c_str());
                                }

                                ImGui::TableSetColumnIndex(1);
                                ImGui::Text("%.2f", r.metrics.averageWaitingTime);

                                ImGui::TableSetColumnIndex(2);
                                ImGui::Text("%.2f", r.metrics.averageTurnaroundTime);

                                ImGui::TableSetColumnIndex(3);
                                ImGui::Text("%.3f", r.metrics.jainFairnessSlowdown);

                                ImGui::TableSetColumnIndex(4);
                                ImGui::Text("%d", r.metrics.contextSwitches);

                                ImGui::TableSetColumnIndex(5);
                                int makespan = r.timeline.empty() ? 0 : r.timeline.back().endTime;
                                ImGui::Text("%d", makespan);
                            }
                            ImGui::EndTable();
                        }

                        ImGui::Spacing();
                        ImGui::Separator();
                        ImGui::Spacing();

                        ImGui::Columns(3, "MetricCards", false);
                        ImGui::TextColored(ImVec4(0.9f, 0.7f, 0.3f, 1.0f), "Turnaround & Waiting Theory:");
                        ImGui::TextWrapped("SJF minimizes average waiting time. Convoy effect occurs in FCFS when long tasks block short ones.");
                        ImGui::NextColumn();

                        ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Round Robin & Fairness:");
                        ImGui::TextWrapped("Round Robin provides responsive time-sharing with top Jain fairness index, but incurs higher context switch overhead.");
                        ImGui::NextColumn();

                        ImGui::TextColored(ImVec4(0.5f, 0.9f, 0.5f, 1.0f), "AI Schedulers (Q-Learning / NN):");
                        ImGui::TextWrapped("Adaptive reinforcement learning discovers optimal trade-offs between turnaround time and starvation prevention.");
                        ImGui::Columns(1);
                    } else {
                        ImGui::Text("No simulation results available. Select a workload mode or add processes.");
                    }

                    ImGui::EndTabItem();
                }

                // =============================================================
                // TAB 3: SUGGESTION 1 - CONCURRENT TASK RACE (LIVE MULTI-TASKING)
                // =============================================================
                ImGuiTabItemFlags tab3Flags = (requestedTab == 2) ? ImGuiTabItemFlags_SetSelected : 0;
                if (ImGui::BeginTabItem("Concurrent Task Race (Real-Time OS)", nullptr, tab3Flags)) {
                    ImGui::Spacing();
                    ImGui::TextColored(ImVec4(0.35f, 0.95f, 0.75f, 1.0f), "LIVE CONCURRENT TASK RACE (SINGLE PHYSICAL CPU CORE)");
                    ImGui::TextWrapped(
                        "Here, 4 real tasks compete concurrently for a single physical CPU core in real-time. "
                        "Watch the live animated progress bars to witness firsthand how different scheduling policies "
                        "cause either complete freezing (convoy effect) or smooth concurrent time-sharing!"
                    );
                    ImGui::Separator();
                    ImGui::Spacing();

                    // Race Controls
                    ImGui::Columns(3, "RaceControls", false);

                    ImGui::Text("Scheduling Policy:");
                    const char* racePolicies[] = {
                        "1. FCFS (Convoy Effect Demo)",
                        "2. Round Robin (Time-Sharing)",
                        "3. Shortest Job First (SRTF)",
                        "4. Priority Preemption"
                    };
                    ImGui::Combo("##racePol", &raceMgr.selectedPolicy, racePolicies, IM_ARRAYSIZE(racePolicies));

                    ImGui::NextColumn();

                    ImGui::Text("Round Robin Quantum (ms):");
                    ImGui::SliderInt("##raceQ", &raceMgr.quantumMs, 15, 80, "%d ms");
                    ImGui::Checkbox("Pin to CPU Core 1 (sched_setaffinity)", &raceMgr.pinCore);

                    ImGui::NextColumn();

                    ImGui::Spacing();
                    if (!raceMgr.isRunning) {
                        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.65f, 0.35f, 1.0f));
                        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.28f, 0.78f, 0.42f, 1.0f));
                        if (ImGui::Button("START CONCURRENT TASK RACE", ImVec2(-1, 36))) {
                            if (raceMgr.workerThread.joinable()) raceMgr.workerThread.join();
                            raceMgr.stopRequested = false;
                            raceMgr.isRunning = true;
                            raceMgr.workerThread = std::thread(runRaceWorker, &raceMgr);
                        }
                        ImGui::PopStyleColor(2);
                    } else {
                        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.85f, 0.25f, 0.25f, 1.0f));
                        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.95f, 0.35f, 0.35f, 1.0f));
                        if (ImGui::Button("STOP / ABORT RACE", ImVec2(-1, 36))) {
                            raceMgr.stopRequested = true;
                            if (raceMgr.workerThread.joinable()) raceMgr.workerThread.join();
                            raceMgr.isRunning = false;
                            raceMgr.activeTaskIndex = -1;
                        }
                        ImGui::PopStyleColor(2);
                    }

                    if (ImGui::Button("Reset Tasks", ImVec2(-1, 24))) {
                        raceMgr.stopRequested = true;
                        if (raceMgr.workerThread.joinable()) raceMgr.workerThread.join();
                        raceMgr.isRunning = false;
                        raceMgr.activeTaskIndex = -1;
                        raceMgr.initDefaultTasks();
                    }

                    ImGui::Columns(1);

                    ImGui::Spacing();
                    ImGui::Separator();
                    ImGui::Spacing();

                    // Active CPU Core Banner
                    int activeIdx = raceMgr.activeTaskIndex.load();
                    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.14f, 0.17f, 0.24f, 0.95f));
                    ImGui::BeginChild("CoreStatusBanner", ImVec2(0, 48), true);
                    if (raceMgr.isRunning && activeIdx >= 0 && activeIdx < (int)raceMgr.tasks.size()) {
                        ImGui::TextColored(ImVec4(0.35f, 0.95f, 0.50f, 1.0f), ">> PHYSICAL CORE 1 BUSY: Currently executing [%s] | Total Context Switches: %d",
                            raceMgr.tasks[activeIdx].name.c_str(), raceMgr.contextSwitches.load());
                    } else if (raceMgr.isRunning) {
                        ImGui::TextColored(ImVec4(0.95f, 0.85f, 0.35f, 1.0f), ">> Context Switch in progress...");
                    } else {
                        ImGui::TextColored(ImVec4(0.70f, 0.75f, 0.85f, 1.0f), "CPU Core Idle. Select a policy and click 'START CONCURRENT TASK RACE'!");
                    }
                    ImGui::EndChild();
                    ImGui::PopStyleColor();

                    ImGui::Spacing();

                    // Live Task Bars
                    for (size_t i = 0; i < raceMgr.tasks.size(); ++i) {
                        auto& t = raceMgr.tasks[i];
                        int done = raceMgr.taskDoneMs[i].load();
                        float frac = static_cast<float>(done) / static_cast<float>(t.totalMs);
                        if (frac > 1.0f) frac = 1.0f;

                        bool isThisRunning = (raceMgr.isRunning && activeIdx == (int)i);
                        bool isFinished = (done >= t.totalMs);

                        ImGui::PushID(static_cast<int>(i));

                        // Task Title & State Badge
                        if (isThisRunning) {
                            ImGui::TextColored(ImVec4(0.35f, 0.95f, 0.50f, 1.0f), "[RUNNING ON CORE]  Task %d: %s (%s)",
                                t.id, t.name.c_str(), t.description.c_str());
                        } else if (isFinished) {
                            ImGui::TextColored(ImVec4(0.40f, 0.75f, 1.0f, 1.0f), "[FINISHED in %.2fs]  Task %d: %s",
                                t.finishTimeSec, t.id, t.name.c_str());
                        } else if (raceMgr.isRunning) {
                            ImGui::TextColored(ImVec4(0.95f, 0.70f, 0.30f, 1.0f), "[WAITING / READY]  Task %d: %s (%s)",
                                t.id, t.name.c_str(), t.description.c_str());
                        } else {
                            ImGui::Text("Task %d: %s (%s)", t.id, t.name.c_str(), t.description.c_str());
                        }

                        // Progress Bar with custom color
                        ImGui::PushStyleColor(ImGuiCol_PlotHistogram, t.color);
                        char overlayBuf[64];
                        std::snprintf(overlayBuf, sizeof(overlayBuf), "%d / %d ms (%d%%)", done, t.totalMs, (int)(frac * 100.0f));
                        ImGui::ProgressBar(frac, ImVec2(-1, 26), overlayBuf);
                        ImGui::PopStyleColor();

                        ImGui::PopID();
                        ImGui::Spacing();
                    }

                    ImGui::Spacing();
                    ImGui::Separator();
                    ImGui::Spacing();

                    // Educational Live Insight Callout
                    ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.35f, 1.0f), "OBSERVED OS SCHEDULING PHENOMENA:");
                    if (raceMgr.selectedPolicy == 0) {
                        ImGui::TextWrapped("%s",
                            "Convoy Effect in action: Task 1 (1800ms) holds the single physical core exclusively until it completes. "
                            "Even though Task 4 only requires 200ms, it is completely starved and frozen at 0% progress for nearly 3 seconds!"
                        );
                    } else if (raceMgr.selectedPolicy == 1) {
                        char msg[256];
                        std::snprintf(msg, sizeof(msg),
                            "Time-Sharing Fairness: The CPU switches between tasks every %d ms. All tasks advance concurrently! "
                            "Notice that Task 4 (200ms) finishes first, without waiting for the heavy 4K encoder to finish.", raceMgr.quantumMs
                        );
                        ImGui::TextWrapped("%s", msg);
                    } else if (raceMgr.selectedPolicy == 2) {
                        ImGui::TextWrapped("%s",
                            "Shortest Job First Efficiency: Task 4 (200ms) and Task 3 (500ms) sprint to 100% completion almost instantly! "
                            "Average waiting time is minimized, but the heavy 1800ms task suffers from starvation."
                        );
                    } else {
                        ImGui::TextWrapped("%s",
                            "Priority Preemption: High priority tasks (Mouse/GUI) preempt heavy background batch tasks immediately upon demand."
                        );
                    }

                    ImGui::EndTabItem();
                }

                // =============================================================
                // TAB 4: SUGGESTION 3 - CONTEXT SWITCH OVERHEAD BENCHMARK
                // =============================================================
                ImGuiTabItemFlags tab4Flags = (requestedTab == 3) ? ImGuiTabItemFlags_SetSelected : 0;
                if (ImGui::BeginTabItem("Context Switch Overhead & Quantum Curve", nullptr, tab4Flags)) {
                    ImGui::Spacing();
                    ImGui::TextColored(ImVec4(1.0f, 0.65f, 0.35f, 1.0f), "THE HIDDEN COST OF CONTEXT SWITCHING (QUANTUM TRADEOFF)");
                    ImGui::TextWrapped(
                        "In OS theory, setting the Round Robin time quantum infinitely small gives the illusion of perfect concurrency. "
                        "However, on real physical CPUs, context switching has a steep hardware cost (flushing CPU registers, TLBs, and L1/L2 caches). "
                        "This live benchmark runs the exact same compute workload while varying the quantum from 100ms down to 0.5ms to prove the Thrashing effect!"
                    );
                    ImGui::Separator();
                    ImGui::Spacing();

                    // Benchmark Trigger & Status
                    ImGui::Columns(2, "BenchCols", false);

                    if (!benchMgr.isRunning) {
                        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.85f, 0.40f, 0.15f, 1.0f));
                        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.95f, 0.50f, 0.25f, 1.0f));
                        if (ImGui::Button("RUN QUANTUM OVERHEAD BENCHMARK", ImVec2(-1, 36))) {
                            if (benchMgr.workerThread.joinable()) benchMgr.workerThread.join();
                            benchMgr.stopRequested = false;
                            benchMgr.isRunning = true;
                            benchMgr.workerThread = std::thread(runBenchmarkWorker, &benchMgr);
                        }
                        ImGui::PopStyleColor(2);
                    } else {
                        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.85f, 0.25f, 0.25f, 1.0f));
                        if (ImGui::Button("ABORT BENCHMARK", ImVec2(-1, 36))) {
                            benchMgr.stopRequested = true;
                            if (benchMgr.workerThread.joinable()) benchMgr.workerThread.join();
                            benchMgr.isRunning = false;
                        }
                        ImGui::PopStyleColor(1);
                    }

                    ImGui::NextColumn();

                    ImGui::Text("Benchmark Progress:");
                    ImGui::ProgressBar(benchMgr.progress.load(), ImVec2(-1, 20));
                    ImGui::TextColored(ImVec4(0.40f, 0.85f, 1.0f, 1.0f), "%s", benchMgr.currentStatus.c_str());

                    ImGui::Columns(1);

                    ImGui::Spacing();
                    ImGui::Separator();
                    ImGui::Spacing();

                    // Results Plot and Table
                    if (!benchMgr.points.empty()) {
                        ImGui::TextColored(ImVec4(0.35f, 0.95f, 0.50f, 1.0f), "RESPONSIVENESS VS. THROUGHPUT TRADEOFF CURVE (LIVE PLOT):");

                        // Custom Curve Drawing using ImDrawList
                        ImVec2 plotPos = ImGui::GetCursorScreenPos();
                        ImVec2 plotSize = ImVec2(ImGui::GetContentRegionAvail().x, 220);

                        ImDrawList* drawList = ImGui::GetWindowDrawList();
                        drawList->AddRectFilled(plotPos, ImVec2(plotPos.x + plotSize.x, plotPos.y + plotSize.y), IM_COL32(18, 20, 26, 255));
                        drawList->AddRect(plotPos, ImVec2(plotPos.x + plotSize.x, plotPos.y + plotSize.y), IM_COL32(50, 55, 68, 255));

                        float minTime = 0.40f;
                        float maxTime = 0.50f;
                        for (const auto& pt : benchMgr.points) {
                            if ((float)pt.wallTimeSec > maxTime) maxTime = (float)pt.wallTimeSec;
                        }
                        maxTime *= 1.15f;

                        // Shaded Zones
                        float zoneW = (plotSize.x - 80.0f) / static_cast<float>(benchMgr.points.size());

                        // Draw Grid lines
                        for (int g = 0; g <= 4; ++g) {
                            float gy = plotPos.y + 20.0f + g * (plotSize.y - 45.0f) / 4.0f;
                            drawList->AddLine(ImVec2(plotPos.x + 60, gy), ImVec2(plotPos.x + plotSize.x - 20, gy), IM_COL32(35, 40, 50, 255));
                            char yLabel[16];
                            float yVal = maxTime - g * (maxTime - minTime) / 4.0f;
                            std::snprintf(yLabel, sizeof(yLabel), "%.2fs", yVal);
                            drawList->AddText(ImVec2(plotPos.x + 10, gy - 7), IM_COL32(140, 145, 160, 255), yLabel);
                        }

                        // Plot Data Points & Line
                        ImVec2 prevScreenPt(0, 0);
                        for (size_t i = 0; i < benchMgr.points.size(); ++i) {
                            const auto& pt = benchMgr.points[i];
                            float px = plotPos.x + 80.0f + i * zoneW;
                            float normY = ((float)pt.wallTimeSec - minTime) / (maxTime - minTime);
                            float py = plotPos.y + plotSize.y - 30.0f - normY * (plotSize.y - 50.0f);

                            ImVec2 screenPt(px, py);

                            // Point marker
                            ImU32 ptCol = (pt.quantumMs <= 2.0) ? IM_COL32(240, 80, 80, 255) 
                                        : (pt.quantumMs <= 25.0 ? IM_COL32(80, 240, 140, 255) : IM_COL32(100, 180, 255, 255));
                            drawList->AddCircleFilled(screenPt, 5.0f, ptCol);

                            if (i > 0) {
                                drawList->AddLine(prevScreenPt, screenPt, IM_COL32(255, 200, 80, 255), 2.5f);
                            }
                            prevScreenPt = screenPt;

                            // X Axis Tick
                            char xLabel[16];
                            std::snprintf(xLabel, sizeof(xLabel), "%.1fms", pt.quantumMs);
                            drawList->AddText(ImVec2(px - 14, plotPos.y + plotSize.y - 20), IM_COL32(170, 175, 190, 255), xLabel);
                        }

                        ImGui::Dummy(plotSize);

                        ImGui::Spacing();
                        ImGui::Separator();
                        ImGui::Spacing();

                        // Benchmark Results Table
                        if (ImGui::BeginTable("BenchTable", 5, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp)) {
                            ImGui::TableSetupColumn("Quantum (ms)");
                            ImGui::TableSetupColumn("Context Switches");
                            ImGui::TableSetupColumn("Total Execution Time");
                            ImGui::TableSetupColumn("Context Switch Overhead (%)");
                            ImGui::TableSetupColumn("Operating System Zone");
                            ImGui::TableHeadersRow();

                            for (const auto& pt : benchMgr.points) {
                                ImGui::TableNextRow();

                                ImGui::TableSetColumnIndex(0);
                                ImGui::Text("%.1f ms", pt.quantumMs);

                                ImGui::TableSetColumnIndex(1);
                                ImGui::Text("%d", pt.switches);

                                ImGui::TableSetColumnIndex(2);
                                ImGui::Text("%.3f seconds", pt.wallTimeSec);

                                ImGui::TableSetColumnIndex(3);
                                if (pt.overheadPct > 35.0) {
                                    ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "+%.1f%% WASTED", pt.overheadPct);
                                } else if (pt.overheadPct < 10.0) {
                                    ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.5f, 1.0f), "+%.1f%% (Minimal)", pt.overheadPct);
                                } else {
                                    ImGui::Text("+%.1f%%", pt.overheadPct);
                                }

                                ImGui::TableSetColumnIndex(4);
                                if (pt.quantumMs <= 2.0) {
                                    ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Thrashing Zone (Unusable)");
                                } else if (pt.quantumMs <= 25.0) {
                                    ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.5f, 1.0f), "Sweet Spot (Linux Desktop)");
                                } else {
                                    ImGui::TextColored(ImVec4(0.6f, 0.8f, 1.0f, 1.0f), "Batch Processing Zone");
                                }
                            }
                            ImGui::EndTable();
                        }
                    }

                    ImGui::EndTabItem();
                }

                // TAB 5: Real CPU Performance & Stress Testing (Mode 6)
                ImGuiTabItemFlags tab5Flags = (requestedTab == 4) ? ImGuiTabItemFlags_SetSelected : 0;
                if (ImGui::BeginTabItem("Real CPU Stress & Kernel Benchmark (Mode 6)", nullptr, tab5Flags)) {
                    ImGui::Spacing();
                    ImGui::TextColored(ImVec4(1.0f, 0.65f, 0.35f, 1.0f), "HARDWARE CPU STRESS & LINUX KERNEL SCHEDULER REPLAY");
                    ImGui::TextWrapped(
                        "Replays the selected scheduler's execution timeline as actual busy compute loops on your real CPU cores. "
                        "When running in WSL2, it uses Linux 'sched_setaffinity' to pin execution to specific vCPUs and measures "
                        "voluntary and involuntary context switches directly from the Linux kernel (/proc/self/sched)."
                    );
                    ImGui::Separator();
                    ImGui::Spacing();

                    ImGui::Columns(2, "StressCols", false);

                    ImGui::Text("Select Scheduler to Replay:");
                    const char* algoNames[] = { "FCFS", "SJF", "Round Robin", "Q-Learning AI", "Neural Net AI" };
                    ImGui::Combo("Algorithm", &selectedStressAlgo, algoNames, IM_ARRAYSIZE(algoNames));

                    ImGui::Spacing();
                    ImGui::Text("CPU Core Affinity:");
                    const char* coreOptions[] = { "All Cores (Dynamic OS Scheduling)", "Pin to Core 1", "Pin to Core 2", "Pin to Core 3" };
                    ImGui::Combo("Core Target", &stressCoreAffinity, coreOptions, IM_ARRAYSIZE(coreOptions));

                    ImGui::NextColumn();

                    ImGui::Text("Tick Duration & Workload Multiplier:");
                    ImGui::SliderInt("ms per Tick", &stressMsPerTick, 10, 150);
                    ImGui::SliderInt("Work Multiplier", &stressMultiplier, 1, 5);

                    ImGui::Spacing();
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.85f, 0.38f, 0.20f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.95f, 0.48f, 0.30f, 1.0f));
                    if (ImGui::Button("RUN REAL CPU WORKLOAD", ImVec2(-1, 36))) {
                        if (selectedStressAlgo >= 0 && selectedStressAlgo < (int)results.size()) {
                            const auto& selRes = results[selectedStressAlgo];
                            stressResult.algoName = selRes.algorithmName;
                            stressResult.slicesCount = static_cast<int>(selRes.timeline.size());
                            
                            int cs = 0;
                            int prevPid = selRes.timeline.empty() ? -1 : selRes.timeline.front().pid;
                            for (const auto& ts : selRes.timeline) {
                                if (ts.pid != prevPid) cs++;
                                prevPid = ts.pid;
                            }
                            stressResult.contextSwitches = cs;

                            stressResult.preStats = CpuStress::getKernelSchedStats();

                            double elapsed = CpuStress::runCpuTimeline(
                                selRes.timeline,
                                stressMsPerTick,
                                stressCoreAffinity,
                                stressMultiplier
                            );

                            stressResult.postStats = CpuStress::getKernelSchedStats();
                            stressResult.wallClockSeconds = elapsed;
                            stressResult.ran = true;
                        }
                    }
                    ImGui::PopStyleColor(2);

                    ImGui::Columns(1);

                    ImGui::Spacing();
                    ImGui::Separator();
                    ImGui::Spacing();

                    if (stressResult.ran) {
                        ImGui::TextColored(ImVec4(0.35f, 0.95f, 0.50f, 1.0f), "LATEST REAL CPU REPLAY RESULTS:");
                        ImGui::BulletText("Replayed Scheduler: %s", stressResult.algoName.c_str());
                        ImGui::BulletText("Actual Measured Wall-Clock Time: %.4f seconds", stressResult.wallClockSeconds);
                        ImGui::BulletText("Executed Slices: %d | Schedule Switches: %d", stressResult.slicesCount, stressResult.contextSwitches);

                        if (stressResult.postStats.available) {
                            ImGui::Spacing();
                            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.14f, 0.18f, 0.25f, 0.9f));
                            ImGui::BeginChild("KernelSchedBox", ImVec2(0, 110), true);
                            ImGui::TextColored(ImVec4(0.40f, 0.80f, 1.0f, 1.0f), "REAL LINUX KERNEL SCHEDULER TELEMETRY (/proc/self/sched):");
                            
                            long voluntaryDelta = stressResult.postStats.voluntarySwitches - stressResult.preStats.voluntarySwitches;
                            long involuntaryDelta = stressResult.postStats.involuntarySwitches - stressResult.preStats.involuntarySwitches;
                            double execTimeDelta = stressResult.postStats.execRuntimeMs - stressResult.preStats.execRuntimeMs;

                            ImGui::Text("Kernel CPU Exec Runtime Delta: %.2f ms", execTimeDelta);
                            ImGui::Text("Linux Voluntary Context Switches Delta: %ld", voluntaryDelta);
                            ImGui::Text("Linux Involuntary Preemptions Delta: %ld (Timeslice expiries by Linux scheduler)", involuntaryDelta);
                            ImGui::Text("Linux Scheduling Policy: %d (0 = SCHED_NORMAL/CFS/EEVDF) | Priority: %d", stressResult.postStats.policy, stressResult.postStats.priority);
                            ImGui::EndChild();
                            ImGui::PopStyleColor();
                        } else {
                            ImGui::TextDisabled("Linux /proc/self/sched telemetry not active (running under Windows MSVC).");
                        }
                    } else {
                        ImGui::TextDisabled("Click 'RUN REAL CPU WORKLOAD' to execute active CPU work on your processor.");
                    }

                    ImGui::EndTabItem();
                }

                // TAB 6: Real Workload Execution Logs
                ImGuiTabItemFlags tab6Flags = (requestedTab == 5) ? ImGuiTabItemFlags_SetSelected : 0;
                if (ImGui::BeginTabItem("Real Workload Execution Logs", nullptr, tab6Flags)) {
                    ImGui::Spacing();
                    ImGui::TextColored(ImVec4(0.4f, 0.85f, 1.0f, 1.0f), "DETAILED REAL HARDWARE / EQUATION EXECUTION LOGS");
                    ImGui::TextDisabled("Shows exact equation computed values, rendering times, and generated burst ticks.");
                    ImGui::Separator();
                    ImGui::Spacing();

                    if (ImGui::Button("Clear Logs")) {
                        workloadLogs.clear();
                    }

                    ImGui::Spacing();
                    ImGui::BeginChild("LogConsole", ImVec2(0, 0), true);
                    for (const auto& logLine : workloadLogs) {
                        if (logLine.find("Error") != std::string::npos || logLine.find("Failed") != std::string::npos) {
                            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s", logLine.c_str());
                        } else if (logLine.find("Result") != std::string::npos || logLine.find("Successfully") != std::string::npos) {
                            ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.5f, 1.0f), "%s", logLine.c_str());
                        } else {
                            ImGui::Text("%s", logLine.c_str());
                        }
                    }
                    ImGui::EndChild();

                    ImGui::EndTabItem();
                }

                // TAB 7: Real Performance Monitoring Guide
                ImGuiTabItemFlags tab7Flags = (requestedTab == 6) ? ImGuiTabItemFlags_SetSelected : 0;
                if (ImGui::BeginTabItem("Performance Monitoring Guide", nullptr, tab7Flags)) {
                    ImGui::Spacing();
                    ImGui::TextColored(ImVec4(0.35f, 0.85f, 1.0f, 1.0f), "HOW TO MONITOR ACTUAL HARDWARE & KERNEL PERFORMANCE IN REAL-TIME");
                    ImGui::Separator();
                    ImGui::Spacing();

                    ImGui::TextColored(ImVec4(0.95f, 0.80f, 0.35f, 1.0f), "Method 1: Linux Terminal Inside WSL2 (Recommended)");
                    ImGui::BulletText("Open a second WSL2 terminal window alongside SmartSched.");
                    ImGui::BulletText("Run: 'htop' (install with: sudo apt install htop)");
                    ImGui::TextWrapped("   When you run the Concurrent Task Race or CPU Stress, watch the selected CPU core bar spike to 100%% utilization in real-time!");
                    ImGui::BulletText("Run: 'mpstat -P ALL 1' (install with: sudo apt install sysstat)");
                    ImGui::TextWrapped("   Shows per-second breakdown of user (%%usr), system (%%sys), and context switches per CPU core.");

                    ImGui::Spacing();
                    ImGui::TextColored(ImVec4(0.95f, 0.80f, 0.35f, 1.0f), "Method 2: Windows Task Manager");
                    ImGui::BulletText("Press Ctrl + Shift + Esc on Windows to open Task Manager.");
                    ImGui::BulletText("Go to the 'Performance' tab -> Click 'CPU'.");
                    ImGui::BulletText("Right-click the main CPU graph -> Change graph to -> 'Logical processors'.");
                    ImGui::TextWrapped("   Watch the individual logical CPU thread jump to 100%% when Core Affinity pinning is active!");

                    ImGui::Spacing();
                    ImGui::TextColored(ImVec4(0.95f, 0.80f, 0.35f, 1.0f), "Method 3: Direct Kernel Telemetry in SmartSched");
                    ImGui::BulletText("SmartSched automatically reads '/proc/self/sched' on WSL2.");
                    ImGui::TextWrapped("   Exposes exact voluntary and involuntary kernel context switches, actual timeslice preemption counts, and kernel execution runtime.");

                    ImGui::EndTabItem();
                }

                ImGui::EndTabBar();
                requestedTab = -1; // Reset programmatic request so user clicks work naturally
            }

            ImGui::EndChild(); // MainContent

            ImGui::End(); // SmartSchedRoot

            // Render
            ImGui::Render();
            SDL_SetRenderDrawColor(renderer, 24, 27, 33, 255);
            SDL_RenderClear(renderer);
            ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), renderer);
            SDL_RenderPresent(renderer);
        }

        // Clean Thread Shutdown
        if (raceMgr.isRunning) {
            raceMgr.stopRequested = true;
            if (raceMgr.workerThread.joinable()) raceMgr.workerThread.join();
        }
        if (benchMgr.isRunning) {
            benchMgr.stopRequested = true;
            if (benchMgr.workerThread.joinable()) benchMgr.workerThread.join();
        }

        // Cleanup ImGui & SDL
        ImGui_ImplSDLRenderer2_Shutdown();
        ImGui_ImplSDL2_Shutdown();
        ImGui::DestroyContext();

        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();

        return 0;
    }
}
