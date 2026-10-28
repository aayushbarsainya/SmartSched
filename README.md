# SmartSched - Advanced Operating System Scheduling Simulator & Real-Life Hardware Benchmark

SmartSched is an advanced, production-grade CPU scheduling algorithm simulator and physical hardware benchmark that bridges the gap between theoretical operating systems principles and real-world silicon execution.

It implements, evaluates, and visualizes **5 distinct scheduling algorithms** (3 traditional + 2 AI-based) across **6 real-world computational workloads**, and features **two live operating system demonstration modules**: a real-time single-core concurrent task race and a hardware context-switch overhead benchmark.

---

## 📑 Table of Contents
1. [Core Features & Capabilities](#-core-features--capabilities)
2. [Scheduling Algorithms Implemented](#-scheduling-algorithms-implemented)
   - [Traditional Schedulers (FCFS, SJF, Round Robin)](#1-traditional-algorithms)
   - [Q-Learning AI Scheduler (Reinforcement Learning)](#2-q-learning-ai-scheduler)
   - [Neural Network-Like Adaptive Scheduler (Perceptron Scoring)](#3-neural-network-like-adaptive-scheduler)
3. [Fairness Metric: Jain's Fairness Index on Slowdown](#-fairness-metric-jains-fairness-index-on-slowdown)
4. [Real-Life Operating System Implementations](#-real-life-operating-system-implementations)
   - [Live Concurrent Task Race (Single Physical Core Contention)](#module-1-live-concurrent-task-race-suggestion-1)
   - [The Cost of Context Switching & Quantum Tradeoff Curve](#module-2-the-cost-of-context-switching--quantum-tradeoff-curve-suggestion-3)
   - [Hardware Telemetry & Kernel Introspection](#module-3-hardware-telemetry--kernel-introspection)
5. [The 6 Workload Generation Modes](#-the-6-workload-generation-modes)
6. [Interactive Graphical User Interface (GUI)](#-interactive-graphical-user-interface-gui)
7. [Running via Docker (Instant Web-GUI on Any OS)](#-running-via-docker-instant-web-gui)
8. [Native Setup & Execution (WSL2 / Linux / Windows)](#-native-setup--execution-wsl2--linux)
9. [Project Architecture & File Tree](#-project-architecture--file-tree)

---

## 🎯 Core Features & Capabilities

- **5 Distinct Schedulers**: First-Come First-Served (FCFS), Shortest Job First (SJF), Round Robin (RR), Q-Learning AI, and Neural Network-like AI.
- **6 Workload Modes**: Manual process table, 10 diverse SDL2 graphics renders, file dataset I/O (`real_data.txt`), real mathematical equations (primes, Fibonacci, factorials), multi-frame BMP image processing, and hardware CPU stress replays.
- **Full Interactive Graphical Interface**: Dear ImGui embedded on SDL2 with 7 specialized analysis tabs.
- **Live Concurrent Task Race**: 4 concurrent tasks competing for a single physical CPU core, visually exposing the Convoy Effect, fair time-sharing, and shortest-job-first sprinting live at 60 FPS.
- **Context Switch Overhead Benchmark**: Physical hardware micro-benchmark varying the Round Robin quantum from 100ms down to 0.5ms to plot the classic CPU Thrashing vs. Responsiveness curve.
- **Jain's Fairness Index on Slowdown**: Mathematical fairness evaluation normalized against process duration.
- **Real Linux Kernel Telemetry**: Core affinity pinning via `sched_setaffinity()` and kernel preemption tracking via `/proc/self/sched`.
- **Zero-Install Docker Support**: Run inside a web browser (HTML5 noVNC) on Windows, macOS, or Linux with a single command.

---

## 📊 Scheduling Algorithms Implemented

### 1. Traditional Algorithms

#### A. First-Come First-Served (FCFS)
- **Type**: Non-preemptive, deterministic.
- **Mechanism**: Processes are executed strictly in the order of their arrival time ($t_{\text{arrival}}$). Once a process gains the CPU, it runs to completion without interruption.
- **Characteristics**:
  - Zero scheduling context-switch overhead ($CS = n - 1$).
  - Highly susceptible to the **Convoy Effect**: a long batch job holding the CPU locks out shorter interactive jobs, causing high average waiting times.
- **Best Suited For**: Background batch computing pipelines with uniform job durations.

#### B. Shortest Job First (SJF)
- **Type**: Non-preemptive, greedy.
- **Mechanism**: Whenever the CPU is idle, the scheduler inspects the ready queue and selects the job with the minimum burst duration ($B_i$).
- **Characteristics**:
  - **Theoretically optimal** for minimizing Average Waiting Time (AWT).
  - Can cause **starvation** of long jobs if shorter jobs continually arrive.
  - Requires prior knowledge of job duration (which SmartSched profiles in Phase 1).
- **Best Suited For**: Environments where job execution times are predictable in advance.

#### C. Round Robin (RR)
- **Type**: Preemptive, cyclic time-sharing.
- **Mechanism**: Every ready process receives a fixed time slice called the **Time Quantum** ($q$). If the process does not complete within $q$, an OS timer interrupt preempts it, saving its context and moving it to the back of the ready queue.
- **Characteristics**:
  - High interactive responsiveness and stellar fairness ($J \approx 0.96 - 0.98$).
  - Incurs context-switching overhead proportional to $\frac{\sum B_i}{q}$.
- **Best Suited For**: General-purpose desktop and interactive multi-user operating systems.

---

### 2. Q-Learning AI Scheduler

The Q-Learning scheduler applies **Model-Free Reinforcement Learning** to learn adaptive process selection policies that minimize queuing delay without hardcoded scheduling heuristics.

```
                  ┌──────────────────────────────────────────────┐
                  │                 Environment                  │
                  │   Ready Queue: [P1(bt=12), P2(bt=4), ...]    │
                  └──────────────┬───────────────────────────────┘
                                 │
                   State s (Discretized Min Burst)
                   Reward r (-Sum of Waiting Times)
                                 │
                                 ▼
                  ┌──────────────────────────────┐
                  │       Q-Learning Agent       │
                  │  Q(s, a) Table Updates via   │
                  │     Bellman Optimality       │
                  └──────────────┬───────────────┘
                                 │
                          Action a (Selected PID)
                                 │
                                 ▼
                  ┌──────────────────────────────┐
                  │       CPU Execution          │
                  │   1-tick slice dispatched    │
                  └──────────────────────────────┘
```

#### Core Reinforcement Learning Formulation:
1. **State ($s$)**: Discretized category of the minimum remaining burst among all ready processes:
   $$s = \text{clamp}\left(\min_{P_i \in \text{Ready}} B_i^{\text{rem}}, 1, \text{maxBurstCategory}\right) - 1$$
   This maps the continuous queue state into discrete buckets ($0 \dots 4$), keeping the state space compact for fast convergence.
2. **Action ($a$)**: Selecting the index of the candidate process from the sorted ready queue.
3. **Reward ($r$)**: Negative sum of waiting times experienced by all other unselected ready processes during this time slice:
   $$r = -\sum_{P_j \in \text{Ready}, j \neq a} 1$$
   Every tick another job waits in the queue, the agent receives an immediate penalty. Minimizing this penalty is mathematically equivalent to minimizing aggregate waiting time.
4. **Bellman Equation Update**:
   $$Q(s, a) \leftarrow Q(s, a) + \alpha \Big[ r + \gamma \max_{a'} Q(s', a') - Q(s, a) \Big]$$

#### Role & Impact of Every Hyperparameter:

| Parameter | Default | Theoretical Meaning | Impact When High | Impact When Low |
|---|---|---|---|---|
| `alpha` ($\alpha$) | `0.50` | **Learning Rate**: Controls the magnitude of Q-value updates per step. | Overreacts to single outlier schedules; policy oscillates and struggles to converge. | Learns extremely slowly; requires thousands of episodes to adapt to arrival patterns. |
| `gamma` ($\gamma$) | `0.90` | **Discount Factor**: Weighs the importance of future rewards vs. immediate rewards. | Agent plans far ahead; avoids scheduling a long task now if it starves 5 small tasks later. | Short-sighted / greedy; only cares about the current 1-tick penalty, ignoring future queue congestion. |
| `epsilon` ($\epsilon$) | `0.10` | **Exploration Rate**: Probability of picking a random process in $\epsilon$-greedy action selection. | Excessive exploration; schedule remains chaotic and noisy even after hundreds of episodes. | Gets trapped in suboptimal local minima early on (e.g., locking into strict arrival order). |
| `trainingEpisodes` | `200` | Number of complete simulated scheduling runs performed during offline training. | Allows the Q-table to stabilize and converge across diverse arrival patterns. | Insufficient training; Q-table remains sparse, leading to unoptimized fallback actions. |
| `maxBurstCategory` | `5` | Quantization granularity of the burst-time state space. | Finer state distinction, but requires significantly more episodes to populate the Q-table. | Coarse categorization; merges vastly different tasks into the same state bucket. |

---

### 3. Neural Network-Like Adaptive Scheduler

The NN-like scheduler implements an **adaptive single-layer perceptron scoring model** optimized through gradient-free evolutionary search.

#### Scoring Function:
At every scheduling decision point, the scheduler calculates a score for every ready process $P_i$:
$$\text{Score}(P_i) = w_1 \cdot \text{RemainingBurst}_i + w_2 \cdot \text{ArrivalTime}_i + w_3 \cdot \text{Priority}_i$$
The scheduler dispatches the process with the **maximum score**:
$$P^* = \arg\max_{P_i \in \text{Ready}} \text{Score}(P_i)$$

#### Evolutionary Optimization Loop:
- Over `trainIterations` (default 100), the scheduler slightly perturbs the weight vector:
  $$w_1 \leftarrow w_1 \cdot (1 \pm \delta_1), \quad w_2 \leftarrow w_2 \cdot (1 \pm \delta_2), \quad w_3 \leftarrow w_3 \cdot (1 \pm \delta_3)$$
- It simulates the entire workload under the candidate weights and calculates the Average Waiting Time (AWT).
- If the new weights improve AWT, the update is permanently locked in; otherwise, it is discarded.

#### Role & Impact of Every Weight & Parameter:

| Parameter | Baseline | Purpose | Behavioral Impact |
|---|---|---|---|
| **Weight $w_1$** (Burst) | `-1.0` | **Duration Penalty** | Because $w_1 < 0$, tasks with smaller remaining bursts receive higher scores. This drives the scheduler toward **Shortest-Job-First behavior**, drastically reducing average waiting time. |
| **Weight $w_2$** (Arrival) | `+0.1` | **Aging / Anti-Starvation** | Counteracts starvation. As an old process waits in the queue, its arrival time remains far in the past. This positive weight gradually boosts its score relative to newer arrivals, guaranteeing CPU time. |
| **Weight $w_3$** (Priority) | `-0.5` | **Priority Sensitivity** | Scales external priority levels relative to burst duration. |
| `trainIterations` | `100` | Optimization Epochs | Higher iterations allow the evolutionary search to fine-tune the $(w_1, w_2, w_3)$ ratios to the specific workload characteristics. |

---

## ⚖️ Fairness Metric: Jain's Fairness Index on Slowdown

### 1. Mathematical Formulation
To quantify fairness rigorously across processes of unequal length, SmartSched uses **Jain's Fairness Index ($J$)** calculated over normalized process **Slowdown**:

$$J(s_1, s_2, \dots, s_n) = \frac{\left( \sum_{i=1}^n s_i \right)^2}{n \cdot \sum_{i=1}^n s_i^2}$$

where the per-process Slowdown ($s_i$) is defined as:
$$s_i = \frac{\text{Turnaround Time}_i}{\max(1, \text{Burst Time}_i)}$$

### 2. Why Slowdown Instead of Raw Waiting Time?
In standard OS analysis, comparing raw waiting times is fundamentally biased:
- A **10-second wait** is catastrophic for a **1-second interactive task** (Slowdown = $\frac{11}{1} = 11.0\times$).
- That exact same **10-second wait** is unnoticeable for a **1-hour rendering task** (Slowdown = $\frac{3610}{3600} = 1.002\times$).

By evaluating Jain's Index over normalized slowdown, we measure **relative equity**: *did every process experience a proportional delay relative to its requested compute time?*

### 3. Properties & Interpretation:
- **Range**: Bounded between $\frac{1}{n}$ (worst possible starvation) and $1.0$ (perfect fairness).
- **$J = 1.0$**: Absolute fairness. Every process suffered the exact same relative degradation.
- **Empirical Results in SmartSched**:
  - **Round Robin**: Highest fairness ($J \approx 0.96 - 0.98$) due to uniform time-slicing.
  - **NN-like AI / Q-Learning**: Balanced fairness ($J \approx 0.88 - 0.92$) while achieving lower waiting times than RR.
  - **FCFS / SJF**: Lower fairness ($J \approx 0.80 - 0.85$) due to convoy head-of-line blocking or long-task starvation.

---

## 🔬 Real-Life Operating System Implementations

### Module 1: Live Concurrent Task Race (Suggestion 1)

Located in **Tab 3: "Concurrent Task Race (Real-Time OS)"**:
Instead of static numerical simulations, this module creates **4 real concurrent workloads** competing for a **single physical CPU core** (`CPU Core 1` pinned via `sched_setaffinity`):
1. **Task 1: Video 4K Encoder** (Heavy task: 1800 ms of work)
2. **Task 2: Database Query** (Medium task: 1000 ms of work)
3. **Task 3: Audio MP3 Transcoder** (Short task: 500 ms of work)
4. **Task 4: Mouse / GUI Event Handler** (Tiny interactive task: 200 ms of work)

```
[ PHYSICAL CPU CORE 1 ] ── Contended by 4 Concurrent Threads ──
  ├── Under FCFS        : Task 1 claims core -> Tasks 2, 3, 4 completely FROZEN at 0%!
  ├── Under Round Robin : Preempts every 35ms -> All 4 tasks advance smoothly together!
  └── Under SJF / SRTF  : Task 4 (200ms) & Task 3 (500ms) sprint to 100% in under 1 second!
```

#### What You Observe Live on Screen:
- **FCFS Convoy Effect**: Task 1 claims the core and animates from 0% to 100%. Tasks 2, 3, and 4 are **completely frozen at 0%** for nearly 3 seconds, visually proving why desktop operating systems cannot use FCFS.
- **Round Robin Fair Time-Sharing**: Slices the physical core every $X$ ms (configurable slider, 15ms–80ms). All 4 progress bars advance concurrently. Task 4 finishes first in $\approx 0.7$s without waiting for the 4K encoder!
- **SJF Efficiency vs. Starvation**: Task 4 and Task 3 sprint to the finish line instantly, slashing average turnaround time.
- **Physical Core Occupancy Banner**: A live telemetry bar lights up indicating which task currently owns the physical CPU silicon.

---

### Module 2: The Cost of Context Switching & Quantum Tradeoff Curve (Suggestion 3)

Located in **Tab 4: "Context Switch Overhead & Quantum Curve"**:
> **The Real-Life Dilemma**: In OS textbooks, context switches are often assumed to take $0$ ms. On physical hardware, switching tasks forces register saving, TLB flushes, and severe **L1/L2 cache invalidation**.

#### The Live Benchmark:
Click **"RUN QUANTUM OVERHEAD BENCHMARK"**:
The engine executes the exact same total computational workload across 4 tasks on your CPU, but iterates the Round Robin time quantum from **100 ms down to 0.5 ms**:

```
Quantum q = 100 ms  ──>    6 switches ──> Wall Time: 0.49s  (Overhead:  +2%)
Quantum q =  25 ms  ──>   24 switches ──> Wall Time: 0.52s  (Overhead:  +6%)
Quantum q =   5 ms  ──>  120 switches ──> Wall Time: 0.78s  (Overhead: +28%)
Quantum q =   1 ms  ──>  480 switches ──> Wall Time: 1.45s  (Overhead: +120%)
Quantum q = 0.5 ms  ──>  960 switches ──> Wall Time: 2.40s  (Overhead: +300% -> THRASHING!)
```

#### The Live Graphical Plot:
The UI plots the **Hyperbolic Responsiveness vs. Throughput Curve** directly:
- **Red Zone ($q < 5$ ms) — Thrashing Zone**: Context-switch tax exceeds $40\%$. The CPU spends more time switching than executing user code.
- **Green Zone ($10\text{ ms} \le q \le 35\text{ ms}$) — The "Sweet Spot"**: This is the Goldilocks zone used by real operating system kernels (e.g., Linux CFS `sysctl_sched_latency` defaults to 6ms–24ms).
- **Blue Zone ($q > 50$ ms) — Batch Zone**: High computational throughput, but sluggish interactive UI responsiveness.

---

### Module 3: Hardware Telemetry & Kernel Introspection

Located in **Tab 5: "Real CPU Stress & Kernel Benchmark"**:
Replays scheduling decisions as active CPU workloads on your laptop, utilizing real Linux kernel hooks:
- **Core Pinning**: Uses `sched_setaffinity()` to bind execution to a specific vCPU.
- **Kernel Introspection (`/proc/self/sched`)**:
  - **`nr_voluntary_switches`**: Tracks yields when waiting or sleeping.
  - **`nr_involuntary_switches`**: Tracks real Linux kernel timeslice expirations (preemptions by CFS/EEVDF).
  - **`se.sum_exec_runtime`**: Measures exact CPU silicon execution time.

---

## 📦 The 6 Workload Generation Modes

| Mode | Name | Description |
|---|---|---|
| **Mode 1** | **Manual / Presets** | Interactive editable table (PID, Arrival, Burst, Priority) + presets (*Standard 5-Task*, *Convoy Effect*, *Bursty Arrivals*). |
| **Mode 2** | **Real SDL2 Render Tasks** | Executes up to 10 actual SDL2 graphics rendering tasks (from 640x480 up to 4K 120 frames), measures real elapsed time, and derives burst ticks. |
| **Mode 3** | **Dataset File I/O** | Save current queues to disk or load previously benchmarked datasets (`real_data.txt`). |
| **Mode 4** | **Real Math Equations** | Computes 4 heavy algorithmic equations on your processor:<br>1. **$k$-th Prime** (Sieve & trial division)<br>2. **Fibonacci($k$)** (Sequence calculation)<br>3. **Factorial($k$)** (Multiplicative product)<br>4. **$k$-th Palindrome** (Decimal reversal search) |
| **Mode 5** | **Real BMP Image Tasks** | Loads BMP files and performs multi-frame software scaling/blitting to benchmark image manipulation workloads. |
| **Mode 6** | **Hardware CPU Replay** | Replays scheduling decisions as busy loops pinned to specific physical CPU cores. |

---

## 🖥️ Interactive Graphical User Interface (GUI)

The application features a modern Dark Theme interface divided into 7 functional tabs:

```
┌────────────────────────────────────────────────────────────────────────────────────────┐
│ SmartSched - Advanced CPU Scheduling Algorithm Simulator & Benchmark                  │
├──────────────────────┬─────────────────────────────────────────────────────────────────┤
│ WORKLOAD GENERATOR   │ [Gantt Chart] [Metrics] [Task Race] [Quantum Curve] [Stress] ... │
│ Mode: [Math Eqns ▼]  ├─────────────────────────────────────────────────────────────────┤
│ Intensity k: [15000] │ FCFS : [P1=======][P2===][P3=][P4==]   Fair: 0.84               │
│ [ COMPUTE & BENCH ]  │ SJF  : [P4==][P3=][P2===][P1=======]   Fair: 0.88               │
│                      │ RR   : [P1=][P2=][P3=][P4=][P1=]...    Fair: 0.97               │
│ Active Queue:        │ Q-Lrn: [P4==][P2===][P1=]...           Fair: 0.91               │
│ 4 Tasks (28 ticks)   │ NN-AI: [P3=][P4==][P2===][P1=======]   Fair: 0.92               │
│                      ├─────────────────────────────────────────────────────────────────┤
│ RR Quantum: [ 2 ]    │ RECOMMENDATION: NN-like AI achieved lowest AWT (4.40 ticks)    │
│ [ RE-SIMULATE ALL ]  │ with Jain Fairness of 0.92. Context switches: 6.                │
└──────────────────────┴─────────────────────────────────────────────────────────────────┘
```

- **Tab 1: Interactive Gantt Chart**: Stacked timelines, hover tooltips, PID color-coding, zoom slider, and fairness meters.
- **Tab 2: Metrics & Performance Matrix**: Benchmark table with winner highlights and AI recommendation engine.
- **Tab 3: Concurrent Task Race**: Real-time visual contention on a single core.
- **Tab 4: Context Switch Overhead & Quantum Curve**: Live hyperbolic tradeoff graph.
- **Tab 5: Real CPU Stress & Kernel Benchmark**: Linux `/proc/self/sched` telemetry.
- **Tab 6: Real Workload Execution Logs**: Detailed computational output and execution times.
- **Tab 7: Performance Monitoring Guide**: Step-by-step instructions for `htop`, `mpstat`, and Task Manager.

---

## 🐳 Running via Docker (Instant Web-GUI)

Anyone on **Windows, macOS, or Linux** can run SmartSched with **zero local dependencies** using Docker. The container includes a built-in lightweight X-server and HTML5 **noVNC** bridge so the GUI renders directly in any modern web browser!

### Option A: Quick Run with Docker CLI
1. Build the Docker image:
   ```bash
   docker build -t smartsched .
   ```
2. Run the container:
   ```bash
   docker run -d -p 8080:8080 --name smartsched-app smartsched
   ```
3. Open your browser at:
   **[http://localhost:8080](http://localhost:8080)**

### Option B: Using Docker Compose
```bash
docker compose up -d
```
Open **[http://localhost:8080](http://localhost:8080)** in Chrome, Edge, or Firefox.

### Option C: Native Desktop Display via WSLg (Windows 11)
If you prefer the GUI window to pop up natively on your Windows desktop:
```bash
docker run -it --rm \
  -e DISPLAY=$DISPLAY \
  -v /tmp/.X11-unix:/tmp/.X11-unix \
  --privileged \
  smartsched
```

---

## 💻 Native Setup & Execution (WSL2 / Linux)

### 1. Install Prerequisites in WSL2 (Ubuntu)
```bash
sudo apt update
sudo apt install -y build-essential cmake libsdl2-dev htop sysstat
```

### 2. Build the Project
```bash
# Navigate to the project directory
cd SmartSched

# Generate build files with CMake
cmake -B build-wsl -S .

# Compile
cmake --build build-wsl -j$(nproc)
```

### 3. Launch the Application
```bash
# Launches the Interactive GUI window via WSLg:
./build-wsl/SmartSched

# (Optional) Run the legacy terminal CLI mode:
./build-wsl/SmartSched --cli
```

---

## 📂 Project Architecture & File Tree

```
SmartSched/
├── CMakeLists.txt              # Cross-platform build script (Ubuntu / Windows / vcpkg)
├── Dockerfile                  # Multi-stage Docker build with noVNC Web GUI
├── docker-compose.yml          # One-click Docker deployment
├── docker-entrypoint.sh        # Dual-mode entrypoint (Web browser / Native X11)
├── WSL2_SETUP_AND_RUN.md       # Comprehensive WSL2 setup and telemetry guide
│
├── main.cpp                    # Application entrypoint (GUI default, --cli fallback)
├── AppUI.h / AppUI.cpp         # Complete Dear ImGui + SDL2 GUI implementation
├── Process.h                   # Process data structures and state
├── Scheduler.h                 # IScheduler polymorphic interface & ScheduleResult
│
├── TraditionalSchedulers.h/.cpp# FCFS, SJF, and Round Robin algorithms
├── AISchedulers.h/.cpp         # Q-Learning AI & NN-like Adaptive Scoring schedulers
├── Metrics.h / Metrics.cpp     # Jain's fairness index, AWT, ATT, slowdown calculators
├── Utils.h / Utils.cpp         # Recommendation engine, CLI matrices, formatters
├── Visualization.h/.cpp        # Native SDL2 fallback Gantt renderer
│
├── cpu_stress/                 # Hardware busy loops, core pinning & /proc/self/sched reader
│   ├── CpuStress.h
│   └── CpuStress.cpp
├── custom_tasks/               # Real mathematical equations (Prime, Fibo, Factorial, Palindrome)
│   ├── Tasks.h
│   └── Tasks.cpp
├── image_tasks/                # BMP multi-frame software scaling benchmark
│   ├── ImageTasks.h
│   ├── ImageTasks.cpp
│   └── *.bmp
├── cmake/                      # Custom CMake find modules
│   └── FindSDL2.cmake
└── imgui/                      # Bundled Dear ImGui immediate-mode GUI library
```

---

## 👥 Authors & Academic Context
Developed as an Advanced Operating Systems Course Project demonstrating the convergence of classical CPU scheduling theory, modern machine learning policies, and real physical hardware execution.
