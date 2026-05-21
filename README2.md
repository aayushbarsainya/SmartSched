# SmartSched - Comprehensive Project Documentation

## Project Updates (Latest)

This release adds interactive visualization improvements, new workload modes, fairness metrics, and a CPU replay tool to observe scheduler impact directly in Windows Task Manager.

### Build
- Requirements: CMake, MSVC, SDL2 via vcpkg (`C:/vcpkg/installed/x64-windows/share/sdl2`).
- Build: `cmake --build build --config Release`
- Run: `build/Release/SmartSched.exe`

### Input Modes
- 1: Sample data (console input or default sample)
- 2: Real task measurement (synthetic render workloads)
- 3: Load processes from file (e.g., `real_data.txt`)
- 4: Custom CPU tasks (enter k)
  - kth prime, Fibonacci(k), factorial(k), kth palindrome
  - Optimized kth prime: large k uses sieve; repeat factor scales automatically
- 5: Image render (BMP)
  - Enter BMP path, frames per task, number of tasks
  - CPU-bound software scaling creates heavy workloads
- 6: Replay CPU load (Task Manager)
  - Replays any scheduler’s timeline as busy CPU loops
  - Options: ms per tick, single-core vs all-cores, per-switch overhead
  - Compare-all mode prints wall-time, context switches, and slice counts for each scheduler for quantitative comparisons

### Visualization (SDL2)
- Stacked Gantt charts: all schedulers shown in one window, one below another
- Pan/Zoom: A/D to pan, W/S or mouse wheel to zoom
- Legend: PID colors consistent across rows
- Fairness meter: small bar per row (left) shows Jain’s fairness index over per-process slowdown

### Fairness Metric
- Slowdown per process: `turnaround / max(1, burst)`
- Jain’s fairness index over slowdowns: `J = (sum s_i)^2 / (n * sum s_i^2)`; 0..1, higher is fairer
- Reported in:
  - Per-algorithm metrics
  - Comparison tables
  - Visualization meter (longer bar = more fair)

### Task Manager Replay (Mode 6)
- Single scheduler or Compare-all:
  - msPerTick: 50–250 (higher → longer replay)
  - Core mode: 1=single-core (makes differences obvious), 2=all-cores
  - Synthetic per-switch overhead ms: 0..3 (simulates context-switch cache/TLB impact)
- Compare-all output (example):
  - FCFS | slices=… cs=… replay=…s
  - SJF  | slices=… cs=… replay=…s
  - RR   | slices=… cs=… replay=…s
  - Q-Learning | …
  - NN-like    | …

### Suggested Scenarios
- Convoy effect (Mode 4 → k=30000), then Mode 6:
  - Compare FCFS vs RR (single-core, msPerTick=150–200, csOverhead=1–2)
- Mixed short/long (Mode 4 → k=45000), then Mode 6:
  - Compare SJF vs FCFS vs RR
- Image workloads (Mode 5 with large BMP, frames=120, tasks=8):
  - Compare all schedulers in Mode 6, single-core

### Notes
- Mode 4 large-k: kth prime uses sieve for k≥100000 to avoid stalls; repeats are scaled down for big k.
- Mode 5: Uses SDL’s BMP loader and software scaling only (no SDL_image dependency).
- The performance matrix and fairness lines are printed in console; the SDL window shows stacked timelines plus fairness meters.

## Table of Contents
1. [Project Objective](#project-objective)
2. [Project Overview](#project-overview)
3. [Operating Modes](#operating-modes)
4. [Scheduling Algorithms - Deep Dive](#scheduling-algorithms---deep-dive)
5. [Performance Metrics and Analysis](#performance-metrics-and-analysis)
6. [Technical Implementation Details](#technical-implementation-details)
7. [Future Scope and Next Steps](#future-scope-and-next-steps)
8. [Project Architecture](#project-architecture)
9. [Building and Running](#building-and-running)
10. [Contributing](#contributing)

---

## Project Objective

**SmartSched** is an advanced CPU scheduling algorithm simulator and performance analysis tool that bridges the gap between theoretical scheduling algorithms and real-world computational workloads. The primary objectives of this project are:

1. **Comparative Analysis**: Provide a comprehensive platform to compare traditional CPU scheduling algorithms (FCFS, SJF, Round Robin) with modern AI-based approaches (Q-Learning, Neural Network-like schedulers).

2. **Real-World Validation**: Move beyond theoretical simulations by using actual computational tasks (SDL2-based image rendering) to measure and validate scheduling algorithm performance.

3. **Intelligent Recommendations**: Analyze task characteristics and automatically recommend the most suitable scheduling algorithm for specific workload types.

4. **Performance Visualization**: Present detailed performance matrices showing per-process metrics (waiting time, turnaround time) across all scheduling algorithms, enabling data-driven decision making.

5. **Educational Value**: Demonstrate how different scheduling strategies perform under various workload conditions, helping understand the trade-offs between fairness, efficiency, and responsiveness.

---

## Project Overview

SmartSched implements five distinct CPU scheduling algorithms and evaluates their performance using three different modes of operation. The project uses SDL2 graphics rendering as a real computational workload, measuring actual execution times to create realistic process scheduling scenarios.

### Key Features

- **Five Scheduling Algorithms**: FCFS, SJF, Round Robin, Q-Learning AI, and NN-like AI
- **Three Operating Modes**: Sample data, real task measurement, and file-based loading
- **Real Computational Workloads**: SDL2-based image rendering tasks
- **Comprehensive Metrics**: Per-process and aggregate performance analysis
- **Intelligent Recommendations**: Task-aware scheduler recommendations
- **Performance Matrices**: Detailed comparison tables and summary matrices

---

## Operating Modes

### Mode 1: Sample Data Mode

**What it does:**
- Uses predefined sample processes for quick testing and demonstration
- Allows manual input of custom process specifications
- Provides a controlled environment for algorithm comparison

**Why we do it:**
1. **Quick Testing**: Enables rapid testing without waiting for real task execution
2. **Reproducibility**: Uses consistent, known workloads for fair algorithm comparison
3. **Educational Purpose**: Demonstrates algorithm behavior with simple, understandable examples
4. **Development and Debugging**: Provides a stable baseline for testing new features

**How it works:**
- Default sample: 5 processes with varying arrival times (0-6), burst times (1-7), and priorities (1-3)
- Option to input custom number of processes with their arrival time, burst time, and priority
- All schedulers run on the same dataset for fair comparison

**Use Cases:**
- Learning how different schedulers handle the same workload
- Understanding algorithm behavior with known process characteristics
- Quick performance comparisons without computational overhead

---

### Mode 2: Real Task Measurement Mode

**What it does:**
- Executes actual SDL2-based image rendering tasks
- Measures real execution time for each rendering task
- Creates a dataset of processes based on actual computational workloads
- Automatically processes 10 diverse rendering tasks with varying complexities

**Why we do it:**
1. **Real-World Validation**: Tests schedulers with actual computational workloads, not theoretical estimates
2. **Accurate Performance Metrics**: Uses real execution times, accounting for system-specific factors (CPU speed, memory, GPU acceleration)
3. **Workload Diversity**: Tests with varied task sizes (from 640x480 to 1920x1080, 45-240 frames)
4. **Practical Application**: Simulates real scenarios where processes have different computational requirements

**How it works:**
1. **Task Configuration**: Pre-defined set of 10 diverse rendering tasks:
   - Heavy workloads: 1920x1080 resolution with 120 frames (4K Heavy)
   - Medium workloads: 1280x720 with 60 frames (720p Medium)
   - Long animations: 800x600 with 180-240 frames
   - Various resolutions: 640x480, 1024x768, 1440x900, 1600x900, 1920x1080

2. **Rendering Process**:
   - Initializes SDL2 window and renderer
   - For each frame:
     - Clears the renderer
     - Renders an animated gradient pattern using trigonometric functions
     - Performs CPU-intensive pixel calculations
     - Presents the frame
   - Measures elapsed time using high-resolution clock

3. **Process Creation**:
   - Each rendering task becomes a process
   - Execution time (in seconds) is converted to burst time (ticks)
   - Processes arrive with staggered arrival times (1 tick apart)
   - All processes are saved to `real_data.txt` for future use

**Technical Details:**
- Uses `SDL_CreateWindow()` and `SDL_CreateRenderer()` for graphics initialization
- Implements CPU-intensive rendering with trigonometric calculations (sin, cos) for realistic workload
- Processes events to prevent window from being marked unresponsive
- Scales execution time by factor of 10 to make differences more noticeable in scheduling

**Use Cases:**
- Testing scheduler performance with real computational tasks
- Understanding how schedulers handle diverse workload sizes
- Validating algorithm effectiveness in practical scenarios
- Creating datasets for repeated testing

---

### Mode 3: Load from File Mode

**What it does:**
- Loads previously saved process datasets from text files
- Allows reuse of measured workloads without re-execution
- Enables consistent testing across multiple runs

**Why we do it:**
1. **Consistency**: Run the same workload multiple times without re-measuring
2. **Time Efficiency**: Skip time-consuming rendering tasks for repeated tests
3. **Reproducibility**: Share datasets across different test runs
4. **Comparison**: Compare scheduler performance on identical workloads over time

**How it works:**
- Reads from `real_data.txt` (or user-specified filename)
- File format: `P<pid> <arrivalTime> <burstTime>` per line
- Parses process data and creates Process objects
- Falls back to sample data if file is empty or invalid

**File Format:**
```
P1 0 15
P2 1 23
P3 2 8
...
```

**Use Cases:**
- Re-running scheduler comparisons on the same dataset
- Sharing workloads between different test sessions
- Analyzing how algorithm improvements affect known workloads
- Creating benchmark datasets for performance regression testing

---

### Mode 4: Custom Task Mode (User-defined CPU tasks, repeated runs)

**What it does:**
- Lets you define custom CPU-bound tasks (math kernels, small algorithms) or pick from a short builtin list.
- Examples of builtin tasks: kth-prime, Fibonacci(k), factorial(k), prime-sieve chunk, matrix multiply (small N), or custom lambda-style workloads.
- You can supply a list of task definitions and select which schedulers to benchmark.

**Why we do it:**
1. **User-driven experiments:** allows researchers and students to test specific CPU patterns.
2. **Make metrics measurable:** short tasks are noisy; running each task multiple times reduces variance and yields stable burst estimates.
3. **Controlled comparisons:** enables A/B testing of schedulers on exactly the same repeated workload.

**How it works:**
- You provide k tasks (via console prompts or a small config file).
- For each task the system runs the computational kernel repeatedly (configurable repetitions) and measures the median/mean execution time.
- The measured execution time (seconds) is converted into simulator ticks; repeated runs reduce system noise.
- Schedulers run on the resulting process list (with measured burst times). Optionally the same experiment is repeated multiple times to produce confidence intervals.

**Notes & tips:**
- Use a larger number of repetitions for short tasks (e.g., 100–1000) to reduce timer noise.
- Builtin tasks include both constant-time and data-dependent kernels so you can exercise different cache/branch behavior.

---

### Mode 5: BMP Image Render Mode (User-provided image)

**What it does:**
- Loads a BMP image provided by the user and renders it repeatedly to generate a realistic heavy CPU workload.

**Why we do it:**
1. **Real content-driven load:** user images may produce different processing patterns than synthetic gradients.
2. **Reproducible heavy workloads:** rendering the same BMP multiple times gives repeatable burst times.
3. **Flexible intensity:** change frames or scale factor to increase/decrease workload.

**How it works:**
- Prompt for a BMP file path and frames-per-task (and optional scale/transform parameters).
- The renderer loads the BMP and performs CPU-side processing (scaling, pixel transformations, filters) per frame.
- Each image-rendering job becomes a process whose burst = measured rendering time (converted to ticks).

**Use cases:**
- Benchmark how schedulers behave with image-processing workloads supplied by users (e.g., photographs, screenshots, textures).

---

### Mode 6: CPU Load Replay Mode (Observe real CPU impact)

**What it does:**
- Replays a previously generated schedule (from Mode 2/3/4/5 or a sample) by running busy loops according to each process' burst and the scheduler timeline.
- Can replay a single process, a subset, or all processes concurrently to mimic real system load.

**Why we do it:**
1. **Observe real impact:** see wall-clock CPU usage in OS tools (Task Manager, top) and measure how scheduling fragmentation affects real CPU graphs.
2. **Per-process vs all-at-once:** replaying individually helps isolate a single process' effect; replaying all processes shows combined load.
3. **Tunable replay parameters:** ms-per-tick, single-core vs all-cores, optional synthetic per-switch delay to emulate cache/TLB effects.

**How it works:**
- Load a ScheduleResult (from file or the last run).
- For each TimeSlice in the timeline, the replay engine either:
   - Spins a busy loop on the selected core(s) for msPerTick * (end-start) milliseconds, or
   - Spawns threads to run the same CPU work the original measurement used.
- The engine can introduce a configurable per-context-switch delay (ms) to model extra overhead such as TLB/cache misses or OS scheduling latency.
- While replaying, watch Task Manager (CPU graph, per-core usage) to visually confirm the load and compare schedulers' real-world footprints.

**Use cases:**
- Visual validation that a scheduler with many small slices (e.g., Round Robin) produces a fragmented CPU usage pattern and more context-switch churn.
- Demonstrate the difference between a compact schedule (FCFS/SJF) and a highly preemptive one (RR, AI tick-based) on actual CPU graphs.

---

## Scheduling Algorithms - Deep Dive

### 1. First Come First Serve (FCFS)

**How it works:**
1. **Sorting**: Processes are sorted by arrival time (with PID as tiebreaker)
2. **Sequential Execution**: Each process runs to completion before the next starts
3. **Non-Preemptive**: Once a process starts, it cannot be interrupted
4. **Idle Time Handling**: CPU waits if no process is ready

**Algorithm Steps:**
```
1. Sort processes by arrival time
2. For each process in order:
   a. Wait until process arrival time (if needed)
   b. Execute process for its full burst time
   c. Record completion time
3. Calculate metrics (waiting time, turnaround time)
```

**Characteristics:**
- **Simplicity**: Easiest to implement and understand
- **Fairness**: Processes executed in arrival order
- **Starvation**: No process starves (all eventually execute)
- **Efficiency**: Low overhead, no context switching

**Best For:**
- Batch processing systems
- Processes with similar execution times
- Simple systems where fairness is more important than optimization

**Limitations:**
- Poor average waiting time if short processes arrive after long ones
- No consideration of process priority or burst time
- Can lead to convoy effect (short processes wait behind long ones)

---

### 2. Shortest Job First (SJF) - Non-Preemptive

**How it works:**
1. **Ready Queue**: Maintains a priority queue of ready processes
2. **Selection**: Always selects the process with shortest burst time
3. **Execution**: Runs selected process to completion
4. **Dynamic Updates**: Adds new processes to ready queue as they arrive

**Algorithm Steps:**
```
1. Sort processes by arrival time
2. Initialize time = 0, ready queue = empty
3. While processes remain:
   a. Add all processes that have arrived to ready queue
   b. If ready queue empty, jump to next arrival time
   c. Select process with minimum burst time from ready queue
   d. Execute process to completion
   e. Update time and remove completed process
4. Calculate metrics
```

**Data Structures:**
- Priority queue (min-heap) ordered by burst time
- PID used as tiebreaker for equal burst times

**Characteristics:**
- **Optimality**: Provides minimum average waiting time for given set of processes
- **Efficiency**: Good for short tasks
- **Starvation**: Long processes may wait indefinitely if short processes keep arriving
- **Predictability**: Requires knowledge of burst times (not always available)

**Best For:**
- Batch systems where burst times are known
- Workloads with many short tasks
- Systems prioritizing average waiting time

**Limitations:**
- Requires prior knowledge of execution time
- Can starve long-running processes
- Not suitable for interactive systems

---

### 3. Round Robin (RR)

**How it works:**
1. **Time Quantum**: Each process gets a fixed time slice (quantum = 2 in our implementation)
2. **Circular Queue**: Processes are executed in round-robin fashion
3. **Preemption**: Process is interrupted after quantum expires
4. **Re-queuing**: Incomplete processes return to the end of the queue

**Algorithm Steps:**
```
1. Sort processes by arrival time
2. Initialize time = 0, ready queue = empty
3. While processes remain:
   a. Add all processes that have arrived to ready queue
   b. If queue empty, jump to next arrival time
   c. Dequeue first process
   d. Execute for min(quantum, remaining_time)
   e. If process not complete, re-queue it
   f. Update time
4. Calculate metrics
```

**Key Parameters:**
- **Time Quantum**: 2 time units (configurable)
- **Preemption**: After each quantum, CPU switches to next process
- **Context Switching**: Overhead from frequent switching

**Characteristics:**
- **Fairness**: All processes get equal CPU time
- **Responsiveness**: Good for interactive systems
- **Overhead**: Context switching overhead increases with smaller quantum
- **No Starvation**: Every process gets CPU time regularly

**Best For:**
- Interactive systems requiring responsiveness
- Time-sharing systems
- Workloads where fairness is critical
- Systems with processes of similar priority

**Limitations:**
- Higher average waiting time compared to SJF
- Context switching overhead
- Performance depends on quantum size (too small = high overhead, too large = poor responsiveness)

---

### 4. Q-Learning AI Scheduler

**How it works:**
1. **Reinforcement Learning**: Uses Q-Learning algorithm to learn optimal scheduling decisions
2. **State-Action-Reward Model**:
   - **State**: Burst category of minimum burst in ready queue
   - **Action**: Selecting which process to execute from ready queue
   - **Reward**: Negative of waiting time increment for other processes
3. **Training Phase**: Runs multiple episodes (200 by default) to learn optimal Q-values
4. **Exploitation Phase**: Uses learned Q-values to make scheduling decisions

**Algorithm Details:**

**State Representation:**
- Burst times are categorized into discrete buckets (1-5 categories)
- State = category of minimum burst time among ready processes
- Simplifies state space for learning

**Action Space:**
- Actions = indices into ready queue (sorted by PID)
- Each action represents selecting a specific process

**Q-Table:**
- Stores Q(state, action) values
- Updated using Bellman equation: `Q(s,a) = Q(s,a) + α[r + γ*max(Q(s',a')) - Q(s,a)]`
- Where:
  - α (alpha) = learning rate (0.5)
  - γ (gamma) = discount factor (0.9)
  - r = reward

**Reward Function:**
- Reward = -sum of waiting time increments for other processes
- Encourages minimizing waiting time for other processes
- Negative reward means penalty for making others wait

**Training Process:**
```
1. Initialize Q-table (empty, values default to 0)
2. For each training episode (200 episodes):
   a. Simulate scheduling with epsilon-greedy policy
   b. For each decision:
      - Observe current state
      - Choose action (explore with probability ε, exploit otherwise)
      - Execute action, observe reward and next state
      - Update Q-table using Bellman equation
3. After training, use greedy policy (no exploration) for final schedule
```

**Epsilon-Greedy Policy:**
- With probability ε (0.1): randomly select action (exploration)
- With probability (1-ε): select action with highest Q-value (exploitation)
- Balances exploration of new strategies with exploitation of learned knowledge

**Characteristics:**
- **Adaptive**: Learns from experience and improves over time
- **Context-Aware**: Considers system state when making decisions
- **Balanced**: Balances exploration and exploitation
- **Learning**: Performance improves with more training episodes

**Best For:**
- Dynamic workloads with varying patterns
- Systems where optimal strategy is not obvious
- Environments where learning from experience is valuable
- Complex scenarios with multiple competing objectives

**Limitations:**
- Requires training time before optimal performance
- May not converge to global optimum
- State space discretization may lose information
- Computational overhead from Q-table updates

---

### 5. Neural Network-like (NN-like) AI Scheduler

**How it works:**
1. **Weighted Scoring**: Uses a weighted combination of process attributes
2. **Score Function**: `score = w1*burstTime + w2*arrivalTime + w3*priority`
3. **Training Loop**: Iteratively adjusts weights to minimize average waiting time
4. **Selection**: Always selects process with highest score from ready queue

**Algorithm Details:**

**Initial Weights:**
- w1 = -1.0 (favors shorter bursts, negative means lower is better)
- w2 = 0.1 (slight preference for earlier arrivals)
- w3 = -0.5 (considers priority)

**Scoring Mechanism:**
- Higher score = higher priority for execution
- Negative w1 means shorter bursts get higher scores
- Combines multiple factors into single decision metric

**Training Process:**
```
1. Initialize weights (w1, w2, w3)
2. For each training iteration (100 iterations):
   a. Schedule processes with current weights
   b. Calculate average waiting time
   c. If better than best so far, save these weights
   d. Slightly adjust weights (multiply by small factors)
3. Use best weights found during training
4. Generate final schedule with optimal weights
```

**Weight Adjustment:**
- Weights are tweaked by small percentages (±1% to ±5%)
- Different adjustment patterns for each weight (based on iteration modulo)
- Keeps best-performing weight combination

**Decision Making:**
- At each time step, calculates score for all ready processes
- Selects process with maximum score
- Executes for 1 time unit (allows dynamic re-evaluation)

**Characteristics:**
- **Multi-Factor**: Considers burst time, arrival time, and priority
- **Adaptive**: Adjusts weights based on workload characteristics
- **Fast**: Quick decision making after training
- **Flexible**: Can be tuned for different objectives

**Best For:**
- Workloads with multiple important factors
- Systems requiring balanced consideration of various attributes
- Scenarios where simple heuristics are insufficient
- Mixed workloads with diverse process characteristics

**Limitations:**
- Training may not find global optimum
- Weight adjustment is heuristic-based
- May overfit to training workload
- Requires tuning of initial weights and adjustment rates

---

## Performance Metrics and Analysis

### Metrics Calculated

1. **Average Waiting Time**: Average time processes spend waiting in ready queue
2. **Average Turnaround Time**: Average time from arrival to completion
3. **Per-Process Metrics**: Individual waiting and turnaround times for each process

### Performance Matrix

The system generates three types of performance matrices:

1. **Per-Process Comparison Matrix**:
   - Rows: Processes (P1, P2, P3, ...)
   - Columns: Scheduling algorithms
   - Cells: Waiting Time (WT) and Turnaround Time (TT) for each process-algorithm combination
   - Purpose: See how each scheduler handles individual processes

2. **Overall Performance Matrix**:
   - Rows: Scheduling algorithms
   - Columns: Average Waiting Time, Average Turnaround Time, Best For
   - Purpose: Quick comparison of aggregate performance

3. **Recommendation System**:
   - Analyzes task characteristics (short/long/mixed/uniform)
   - Identifies best-performing schedulers
   - Provides recommendations based on workload type
   - Explains why certain schedulers work better for specific scenarios

### Analysis Features

- **Task Classification**: Automatically classifies workload as:
  - Short Tasks (all ≤ 3 units)
  - Long Tasks (all ≥ 10 units)
  - Mixed Workload (wide range of sizes)
  - Uniform Workload (similar sizes)

- **Performance Insights**: 
  - Identifies schedulers optimal for waiting time
  - Identifies schedulers optimal for turnaround time
  - Highlights schedulers optimal for both metrics

- **Recommendations**:
  - Suggests best scheduler based on task characteristics
  - Explains trade-offs between different approaches
  - Provides context-specific guidance

---

## Context Switches and Overhead (what we added and why it matters)

We added explicit tracking and reporting of context-switch behavior so the simulator is more transparent about preemption-related costs.

What we record now:
- `contextSwitches` — a simple count of how many times the scheduler switched from one process to another in the timeline.
- `contextSwitchOverhead` — a field reserved for modeling an explicit synthetic overhead (in ticks) if you want to simulate extra costs per switch. By default this is 0.0 because the real-cost of switching is already reflected in measured waiting/turnaround times.

Why we added this:
1. **Visibility**: seeing the `contextSwitches` number explains why highly-preemptive schedulers (Round Robin, tick-by-tick AI) often show worse waiting/turnaround times despite being fair.
2. **Configurable modeling**: we provide the hooks to optionally add a per-switch penalty (for example to model TLB/cache invalidation, scheduler latency, or expensive context-save operations) when you need to experiment with systems where context-switch cost is non-negligible.
3. **No double-counting by default**: because our timing is based on measured execution times (Mode 2/4/5) and the timeline fragmentation already causes increased waiting/turnaround, we do not add a synthetic overhead by default — doing so would double-count the cost in many cases.

Impact on metrics and interpretation:
- A scheduler with many small slices (high `contextSwitches`) will generally show higher average waiting and turnaround times. The simulator now prints the switch count beside the standard metrics so you can correlate fragmentation with metric degradation.
- If you want to model additional switch cost explicitly (for academic experiments), enable a per-switch overhead in the replay/measurement configuration. This will add a fixed per-switch time to the total makespan and will change CPU utilization/throughput accordingly.

Practical guidance:
- For modern desktop CPUs, the pure context-switch time is small, but cache/TLB effects can magnify the effective cost. Use the synthetic overhead parameter in Mode 6 when you want to emulate such platform-specific penalties.
- When comparing schedulers on measured workloads, prefer relying on the measured burst/turnaround values and use `contextSwitches` as an explanatory metric rather than as an automatic additive penalty.

---

## Technical Implementation Details

### Project Structure

```
SmartSched/
├── main.cpp                 # Entry point, mode selection
├── TraditionalSchedulers.h/cpp  # FCFS, SJF, Round Robin
├── AISchedulers.h/cpp       # Q-Learning, NN-like schedulers
├── Process.h                # Process data structure
├── ProcessManager.h/cpp     # Process collection management
├── RealTaskManager.h/cpp    # SDL2 rendering and file I/O
├── Scheduler.h               # Base scheduler interface
├── Metrics.h/cpp             # Metrics calculation
├── Utils.h/cpp               # Utility functions, printing, recommendations
├── CMakeLists.txt            # Build configuration
└── cmake/FindSDL2.cmake      # SDL2 finder module
```

### Key Design Patterns

1. **Polymorphism**: All schedulers implement `IScheduler` interface
2. **Encapsulation**: ProcessManager encapsulates process collection
3. **Function Overloading**: Multiple `printComparisonTable` variants
4. **Lambda Functions**: Used extensively for sorting and comparisons
5. **RAII**: SDL2 resources properly managed with cleanup

### SDL2 Rendering Implementation

**Rendering Task Details:**
- Creates SDL2 window and hardware-accelerated renderer
- Renders animated gradient patterns using trigonometric functions
- Performs CPU-intensive calculations:
  - Trigonometric operations (sin, cos) for color calculations
  - Multiple iterations of color mixing
  - Pixel-by-pixel rendering with configurable step size
- Measures execution time with nanosecond precision
- Handles SDL events to prevent window freezing

**Workload Characteristics:**
- Resolution affects pixel count (width × height)
- Frame count affects total computation time
- Mathematical complexity (trigonometric functions) ensures CPU-bound workload
- Step size (2 pixels) balances rendering quality and execution time

---

## Future Scope and Next Steps (status & notes)

Below we annotate the planned items with the current implementation status and brief pointers to the code that already provides parts of the feature.

### Immediate Enhancements

1. **User Input for Tasks**: PARTIALLY IMPLEMENTED
   - Status: Mode 4 (Custom tasks) supports several builtin CPU kernels and measures them interactively; see `main.cpp` (Mode 4) and `cpu_tasks/` implementations.
   - Remaining: richer interactive rendering-task editor (defining render parameters per task) and config-file driven batch task definitions.



2. **Dynamic Task Discovery**: NOT IMPLEMENTED
   - Status: planned. No automatic OS-level profiling or conversion of running apps into process workloads is currently available.
   - Next step: integrate a lightweight profiler/monitor (platform-specific) and convert traces to `real_data.txt` entries.

### Advanced Features



3. **Machine Learning Enhancements**: PARTIALLY IMPLEMENTED
   - Status: Q-Learning and a NN-like weighted scheduler are implemented (`AISchedulers.cpp`).
   - Remaining: DQN, policy-gradient methods and transfer learning are research items to add later.

4. **Advanced Scheduling Algorithms**: PARTIALLY IMPLEMENTED
   - Status: core algorithms (FCFS, SJF, RR) plus two AI schedulers exist. See `TraditionalSchedulers.cpp` and `AISchedulers.cpp`.
   - Remaining: SRTF, multilevel queues, lottery, fair-share, etc. are on the roadmap.

5. **Performance Analysis Tools**: PARTIALLY IMPLEMENTED
   - Status: Basic statistics, slowdown, Jain fairness, CPU utilization, and throughput are implemented in `Utils.cpp` / `Metrics.cpp`.
   - Remaining: confidence intervals, advanced statistical reporting, CSV/JSON export, and automated regression testing.

6. **Workload Generation**: PARTIALLY IMPLEMENTED
   - Status: Sample generators, SDL render workloads (Mode 2), BMP-based workloads (Mode 5), and CPU kernels (Mode 4) are available.
   - Remaining: scripted synthetic workload generators and import of real-world traces.

### Integration and Extensibility

7. **Plugin System**: NOT IMPLEMENTED
   - Status: No plugin API currently. Future work: add a dynamic scheduler loader and a small plugin interface.

8. **Distributed Scheduling**: NOT IMPLEMENTED
   - Status: Project currently targets single-host simulation and replay. Distributed/multi-node scheduling requires additional architecture.

9. **Real-Time Scheduling**: NOT IMPLEMENTED
   - Status: Real-time algorithms (EDF, RM) are not currently implemented.

10. **Web Interface**: NOT IMPLEMENTED
   - Status: No REST API or web dashboard; could be added later using a small HTTP server + frontend.

### Research and Development

11. **Adaptive Scheduling**: PARTIALLY IMPLEMENTED
   - Status: The AI schedulers provide an initial adaptive capability (Q-Learning and NN-like). See `AISchedulers.cpp`.
   - Remaining: continuous-state RL, online learning, and automated adaptation during production runs.

12. **Energy-Aware Scheduling**: NOT IMPLEMENTED
   - Status: No energy modeling or power-aware policies present.

13. **Security-Aware Scheduling**: NOT IMPLEMENTED
   - Status: No isolation or security-aware allocation features yet.

### Documentation and Usability

14. **Enhanced Documentation**: PARTIALLY IMPLEMENTED
   - Status: `README2.md` and `README.md` provide extensive documentation and usage instructions. API docs and tutorials remain to be added.

15. **Configuration System**: PARTIALLY IMPLEMENTED
   - Status: Command-line prompts and simple run-time options exist; a full config-file driven system is still planned.

16. **Benchmarking Suite**: PARTIALLY IMPLEMENTED
   - Status: The project saves measured workloads to `real_data.txt` and supports replay (Mode 6). A formal benchmarking harness, CSV/JSON export, and automated regression suite are future work.

---
---

## Project Architecture

### Class Hierarchy

```
IScheduler (Interface)
  ├── SchedulerBase (Abstract Base)
      ├── FCFS_Scheduler
      ├── SJF_Scheduler
      ├── RR_Scheduler
      ├── QLearningScheduler
      └── NNLiketScheduler
```

### Data Flow

1. **Input**: Process specifications or real task measurements
2. **Processing**: Each scheduler generates a timeline
3. **Metrics Calculation**: Compute waiting time, turnaround time
4. **Analysis**: Generate comparison matrices and recommendations
5. **Output**: Console output with detailed metrics and recommendations

### Memory Management

- Smart pointers for SDL2 resources (implicit cleanup)
- RAII principles for resource management
- Efficient data structures (priority queues, hash maps)
- Minimal memory allocations during scheduling

---

## Building and Running

### Prerequisites

- CMake 3.16 or higher
- C++17 compatible compiler (MSVC, GCC, Clang)
- SDL2 library
- vcpkg (recommended for dependency management)

### Building with vcpkg

```powershell
# Configure with vcpkg toolchain
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake

# Build Release version
cmake --build build --config Release

# Run executable
.\build\Release\SmartSched.exe
```

### Running the Program

1. **Select Mode**: Choose from 3 operating modes
2. **Provide Input**: Sample data, real tasks, or file path
3. **View Results**: 
   - Individual scheduler metrics
   - Per-process comparison matrix
   - Scheduler recommendations
   - Overall performance matrix

---

## Contributing

We welcome contributions! Areas for contribution:

- New scheduling algorithms
- Performance optimizations
- Additional workload types
- Visualization improvements
- Documentation enhancements
- Bug fixes and testing

### Contribution Guidelines

1. Follow existing code style
2. Add tests for new features
3. Update documentation
4. Submit pull requests with clear descriptions

---

## Conclusion

SmartSched provides a comprehensive platform for understanding, comparing, and optimizing CPU scheduling algorithms. By combining traditional algorithms with AI-based approaches and real-world workload validation, it offers valuable insights into scheduling performance and helps identify the best strategies for different computational scenarios.

The project's extensible architecture and clear separation of concerns make it an excellent foundation for research, education, and practical scheduling optimization.