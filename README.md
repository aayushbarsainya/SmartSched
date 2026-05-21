# SmartSched - Advanced CPU Scheduling Algorithm Simulator

SmartSched is a comprehensive CPU scheduling algorithm simulator that bridges the gap between theoretical operating systems concepts and real-world computational workloads. It implements and compares **5 distinct scheduling algorithms** (3 traditional + 2 AI-based) using actual CPU-bound tasks with real execution timing.

The project demonstrates how different scheduling strategies perform under various workload conditions and uses AI techniques to discover adaptive scheduling policies that outperform traditional algorithms.

## 🎯 Core Features

- **5 Scheduling Algorithms**: FCFS, SJF, Round Robin (traditional) + Q-Learning, NN-like (AI-based)
- **Real Workload Simulation**: SDL2 graphics rendering, image processing, custom CPU tasks
- **Comprehensive Metrics**: Waiting time, turnaround time, fairness (Jain's index), context switches, slowdown
- **Automatic Recommendations**: Analyzes workload and recommends optimal scheduler
- **Interactive Visualization**: Stacked Gantt charts with pan/zoom and fairness meters
- **6 Operation Modes**: Sample data, real tasks, file loading, custom tasks, image rendering, CPU replay
- **Educational Value**: Demonstrates scheduling theory in practice with data-driven insights

## 📊 Scheduling Algorithms Implemented

### Traditional Algorithms

#### 1. First Come First Serve (FCFS)
- **Type**: Non-preemptive, deterministic
- **Decision Rule**: Execute processes in arrival order
- **Characteristics**:
  - Simplest to implement
  - No context switching overhead
  - Suffers from "convoy effect" (long jobs block short jobs)
  - Fair in order but not in performance
- **Best For**: Batch systems with similar-length jobs
- **Performance**: Baseline comparator

#### 2. Shortest Job First (SJF)
- **Type**: Non-preemptive, optimal for average waiting time
- **Decision Rule**: Select ready process with minimum burst time
- **Characteristics**:
  - Minimizes average waiting time (theoretically optimal)
  - Can cause starvation of long jobs
  - Requires knowing job duration in advance
  - Fair: 0.88 (moderate fairness)
- **Best For**: Systems where job lengths are known
- **Performance**: Best average waiting time among traditional algorithms

#### 3. Round Robin (RR)
- **Type**: Preemptive, time-slice based
- **Decision Rule**: Each process gets time quantum (2 units default), then moves to back of queue
- **Characteristics**:
  - Fairest traditional algorithm (0.97 fairness index)
  - Higher context switching overhead
  - Good for interactive/multi-user systems
  - Responsive but may have longer turnaround times
- **Best For**: General-purpose systems, interactive workloads
- **Performance**: Best fairness, moderate timing metrics

### AI-Based Algorithms

#### 4. Q-Learning AI Scheduler
- **Type**: Reinforcement Learning (model-free)
- **Core Mechanism**:
  - Maintains Q-table: `Q(state, action) ← Q + α(reward + γ·max Q' - Q)`
  - State: Discretized burst time category (5 buckets)
  - Action: Process selection from ready queue
  - Reward: Negative of other processes' waiting time
  - **Learns over 200 episodes** with exploration-exploitation (ε=0.1)

- **Parameters**:
  - Learning Rate (α) = 0.5
  - Discount Factor (γ) = 0.9 (future rewards matter)
  - Exploration Rate (ε) = 0.1 (10% random, 90% greedy)

- **How It Works**:
  1. Training Phase: Run multiple episodes, updating Q-values
  2. Exploitation Phase: Use learned policy to schedule
  3. Adapts scheduling decisions based on process characteristics

- **Characteristics**:
  - Learns optimal value for scheduling decisions
  - Balances exploration (try new choices) vs exploitation (use learned knowledge)
  - Shows competitive performance (6.00 avg waiting time)
  - Demonstrates that learned policies can match hand-crafted algorithms

#### 5. Neural Network-like (NN-like) AI Scheduler
- **Type**: Adaptive weighted scoring (simplified neural network)
- **Core Mechanism**:
  ```
  score = w1×(remaining_burst) + w2×(arrival_time) + w3×(priority)
  decision: pick process with MAX score
  ```
  - Implements single-layer perceptron scoring function
  - Weights are adaptively optimized during training

- **Training Process**:
  - **100 training iterations** of schedule simulation
  - Gradient-free optimization: perturb weights by ±2-5%, validate
  - Keeps weight combinations that minimize average waiting time
  - **Learned weights** (after optimization):
    - w1 = -1.0 (negative → favors short tasks)
    - w2 = 0.1 (small positive → slight arrival time consideration)
    - w3 = -0.5 (negative → deprioritizes priority field)

- **How It Works**:
  1. Initialize weights
  2. Run full schedule with current weights, measure waiting time
  3. Perturb weights slightly
  4. Keep improvements, discard degradations
  5. Repeat until converged

- **Characteristics**:
  - **BEST OVERALL PERFORMANCE** (4.40 avg waiting time)
  - 15% better than SJF (5.20)
  - Explainable decision process (weights have interpretable meaning)
  - Learned to prioritize burst time (w1 weight)
  - Fair allocation: 0.88 fairness index
  
- **Why It Works Best**:
  - Adapts all three parameters simultaneously
  - Free parameters allow flexibility vs fixed rules
  - Training loop validates decisions empirically

## ✨ Advanced Features

### Performance Metrics
- **Waiting Time**: Time process waits before first execution
- **Turnaround Time**: Total time from arrival to completion
- **Slowdown**: Turnaround time / Burst time (relative degradation)
- **Fairness Index**: Jain's fairness metric (0..1, higher = fairer)
  - `J = (Σ slowdown)² / (n × Σ slowdown²)`
- **CPU Utilization**: Percentage of time CPU is busy
- **Throughput**: Processes completed per unit time
- **Context Switches**: Number of times scheduler switches processes

### Workload Analysis Engine
- Automatically detects workload characteristics:
  - Task duration distribution (short, medium, long tasks)
  - Arrival pattern (staggered vs clustered)
  - Process count and burst time range
- Recommends best scheduler based on analysis
- Data-driven decision making

### Real Workload Simulation
- **SDL2 Graphics Rendering**: Actual computational workload, not synthetic timing
- **Diverse Task Types**:
  - Image rendering (BMP processing with software scaling)
  - Custom CPU tasks (prime numbers, Fibonacci, factorial)
  - Real timing measurement for authentic scheduling scenarios

### Visualization
- **Interactive Gantt Charts**: Stacked timeline view of all schedulers
- **Pan/Zoom Controls**: A/D to pan, W/S or mouse wheel to zoom
- **Fairness Visualization**: Per-scheduler fairness meter
- **Color Coding**: Consistent PID colors across scheduler rows
- **Legend**: Clear algorithm identification

### Task Manager Replay Mode
- Replay scheduling decisions as actual busy CPU loops
- Watch processes execute in Windows Task Manager
- Compare CPU impact of different schedulers in real-time
- Configurable ms-per-tick and context switch overhead simulation

## Project Features

- Real-world workload simulation using SDL2 graphics rendering
- Comprehensive performance metrics (waiting time, turnaround time, fairness, context switches)
- Support for both sample data and real task measurements
- Ability to save and load task datasets
- Interactive Gantt chart visualization with pan/zoom
- Automatic scheduler recommendation engine
- Data-driven comparative analysis

## 🛠️ Building and Running

### Prerequisites
1. **CMake**: 3.16 or higher
2. **C++ Compiler**: MSVC with C++17 support
3. **SDL2**: Via vcpkg (easiest)
4. **vcpkg**: Microsoft's package manager for C++ libraries

### Quick Setup

#### 1. Install SDL2 via vcpkg
```powershell
# Clone vcpkg if you haven't already
git clone https://github.com/Microsoft/vcpkg.git
cd vcpkg
./bootstrap-vcpkg.bat

# Install SDL2 for x64-windows
./vcpkg install sdl2:x64-windows

# Integrate with Visual Studio (optional but recommended)
./vcpkg integrate install
```

#### 2. Build the Project
```powershell
cd SmartSched

# Configure with CMake (use your vcpkg path)
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake

# Build in Release mode
cmake --build build --config Release
```

#### 3. Run the Program
```powershell
# Launch the simulator
./build/Release/SmartSched.exe
```

### Build Verification
```powershell
# Executable should be created at:
# C:\Users\[YourUsername]\Downloads\SmartSched\build\Release\SmartSched.exe

# Verify executable exists:
dir build\Release\SmartSched.exe
```

## 📋 Usage Guide - 6 Operating Modes

### Mode 1: Sample Data (Quick Testing)
```
Selection: 1
Enter number of processes (or 0 to use sample): 0
```
- Uses 5 predefined processes
- Best for: Understanding scheduler behavior quickly
- Output: Gantt charts, metrics, comparison table

**Example processes**:
- P1: Arrival=0, Burst=7
- P2: Arrival=2, Burst=4
- P3: Arrival=4, Burst=1
- P4: Arrival=5, Burst=4
- P5: Arrival=6, Burst=6

### Mode 2: Real Task Measurement (Actual Workloads)
```
Selection: 2
```
- Automatically measures 10 diverse SDL2 rendering tasks
- Tasks range from 720p to 4K resolution
- Measures actual execution time for realistic scheduling
- Saves results to `real_data.txt`

**Example workloads**:
- 4K Heavy (1920x1080, 120 frames)
- 720p Medium (1280x720, 60 frames)
- Long Animation (800x600, 180 frames)
- Extended rendering (1024x768, 150 frames)

### Mode 3: Load from File
```
Selection: 3
Enter filename: real_data.txt
```
- Load previously saved task datasets
- Enables consistent comparison across multiple runs
- Default file: `real_data.txt` (created by Mode 2)
- Useful for: Benchmarking, regression testing

### Mode 4: Custom CPU Tasks
```
Selection: 4
Enter k: 30000
```
- CPU-intensive mathematical tasks
- Options:
  - **kth Prime**: Calculate kth prime number
    - Uses sieve for k≥100000 (optimized)
  - **Fibonacci(k)**: Calculate kth Fibonacci number
  - **Factorial(k)**: Calculate k!
  - **kth Palindrome**: Find kth palindromic number

- Best for: Demonstrating convoy effect, scheduler behavior under stress
- Large k values create heavy computational loads

### Mode 5: Image Rendering (BMP Processing)
```
Selection: 5
Enter BMP path: /path/to/image.bmp
Enter frames per task: 120
Enter number of tasks: 8
```
- SDL2 BMP image processing with software scaling
- Creates realistic I/O and compute workloads
- No SDL_image dependency (pure SDL2 BMP loader)
- Best for: Testing schedulers with diverse, complex workloads

### Mode 6: CPU Replay (Task Manager Visualization)
```
Selection: 6
Scheduler (1=FCFS, 2=SJF, 3=RR, 4=Q-Learning, 5=NN-like, 6=Compare-all): 6
Enter ms per tick (50-250): 100
Core mode (1=single-core, 2=all-cores): 1
Synthetic context-switch overhead (0-3 ms): 1
```
- Replay scheduling decisions as actual CPU load
- Watch processes in Windows Task Manager
- Compare real CPU impact of different schedulers
- Output: Wall-time, context switches, time slices per scheduler

**Parameters**:
- **ms per tick**: Higher = longer replay, better visualization
- **Core mode**: Single-core shows differences more clearly
- **Context-switch overhead**: Simulates cache/TLB impact

## 📊 Sample Results & Performance Comparison

### Test Run: 5-Process Sample Workload

#### Algorithm Performance Matrix

| Scheduler | Avg Waiting | Avg Turnaround | Context Switches | Fairness |
|-----------|-------------|-----------------|------------------|----------|
| **NN-like AI** 🏆 | **4.40** | **8.80** | 6 | 0.88 |
| SJF | 5.20 | 9.60 | 4 | 0.88 |
| FCFS | 5.80 | 10.20 | 4 | 0.66 |
| Q-Learning | 6.00 | 10.40 | 5 | 0.61 |
| Round Robin | 7.20 | 11.60 | 11 | **0.97** 🏆 |

#### Key Insights

1. **NN-like AI (Best Overall)**
   - Achieved best waiting time (4.40)
   - Achieved best turnaround time (8.80)
   - 15% better than SJF (5.20)
   - Strong fairness (0.88)

2. **Round Robin (Fairest)**
   - Best fairness index (0.97)
   - Trade-off: Higher overhead (11 context switches)
   - Higher timing metrics

3. **SJF (Balanced)**
   - Second-best waiting time (5.20)
   - Good fairness (0.88)
   - Stable: only 4 context switches

4. **FCFS (Baseline)**
   - Predictable behavior
   - Poor fairness (0.66) due to convoy effect
   - Useful for comparison baseline

5. **Q-Learning (Adaptive)**
   - Competitive (6.00 waiting time)
   - Shows lower fairness (0.61) - optimization trade-off
   - Demonstrates learning capability

### Automatic Recommendation System Output

```
=== SCHEDULER RECOMMENDATION ===
Task Characteristics:
  Type: Short Tasks (all ≤ 3 units)
  Average burst time: 4.40
  Burst time range: 1 - 7
  Staggered arrivals: No

Recommendations:
  ✓ For short tasks, SJF or NN-like AI typically perform well
  ✓ Round Robin may cause overhead for very short tasks

  ➜ RECOMMENDED: NN-like AI
  
  Verification: NN-like AI is optimal for both metrics
```

## 🎓 Computer Science Concepts Demonstrated

### Operating Systems
- CPU Scheduling and context switching
- Process management and state transitions
- Queue management and resource allocation
- Preemptive vs non-preemptive scheduling

### Algorithms & Data Structures
- Scheduling algorithms (FCFS, SJF, Round Robin)
- Priority queues for ready process management
- Algorithm analysis and comparison

### Machine Learning / AI
- **Reinforcement Learning**: Q-Learning with state-action values
- **Adaptive Learning**: Weight optimization through perturbation
- **Exploration-Exploitation Trade-off**: Epsilon-greedy strategy
- **Value Iteration**: Bellman equation for Q-updates

### Software Engineering
- Object-Oriented Design (abstract IScheduler interface)
- Design Patterns (Strategy, Factory)
- Encapsulation (ProcessManager)
- Polymorphism and inheritance

### Performance Analysis
- Metrics computation and comparison
- Fairness measurement (Jain's index)
- Statistical analysis of scheduling behavior
- Trade-off analysis (efficiency vs fairness)

### Graphics & Visualization
- SDL2 graphics programming
- Interactive UI (pan/zoom controls)
- Real-time visualization of scheduling timelines
- Data visualization techniques

## 🏗️ Project Architecture

### Core Components

```
┌─────────────────────────────────┐
│      main.cpp                    │
│   (6 Input Modes)                │
└──────────┬──────────────────────┘
           │
     ┌─────┴──────────────┐
     │                    │
┌────▼────────┐  ┌────────▼──────┐
│ProcessManager   Visualization  │
│(Encapsulation) (SDL2 Rendering)│
└─────────────┘  └───────────────┘
     │
     ▼
┌──────────────────────────────┐
│  IScheduler (Interface)      │
└───────────┬──────────────────┘
            │
    ┌───────┴────────────┐
    │                    │
┌───▼──────────────┐  ┌──▼────────────┐
│TraditionalSchedulers   AISchedulers  │
│- FCFS             │  │- Q-Learning   │
│- SJF              │  │- NN-like      │
│- RR               │  └────────────────┘
└────────────────────┘
    │
    ▼
┌────────────────────┐
│  Metrics Engine    │
│  (Analysis)        │
└────────────────────┘
```

### File Structure
- **main.cpp**: Entry point, mode selection, user interface
- **Scheduler.h**: IScheduler interface, abstract base class
- **TraditionalSchedulers.{h,cpp}**: FCFS, SJF, Round Robin implementations
- **AISchedulers.{h,cpp}**: Q-Learning and NN-like AI implementations ✅ (Real ML algorithms)
- **Process.h**: Process data structure
- **ProcessManager.{h,cpp}**: Encapsulation of process collection
- **Metrics.{h,cpp}**: Performance metric computation
- **Visualization.{h,cpp}**: SDL2 graphics and Gantt chart rendering
- **Utils.{h,cpp}**: Utility functions, metric calculations
- **RealTaskManager.{h,cpp}**: File I/O for task datasets
- **ImageTasks.{h,cpp}**: BMP image rendering workloads
- **custom_tasks/Tasks.{h,cpp}**: CPU-intensive mathematical tasks
- **cpu_stress/CpuStress.{h,cpp}**: CPU load replay functionality
- **CMakeLists.txt**: Build configuration

## 🔬 Implementation Highlights

### Real AI Algorithms (Not Placeholders)
- **Q-Learning**: Full Bellman equation implementation
  - Maintains Q-table with state-action values
  - Runs 200 training episodes
  - Epsilon-greedy exploration-exploitation (ε=0.1)
  - Outperforms pure random selection

- **NN-like**: Adaptive weight optimization
  - Linear scoring function (simplified neural network)
  - 100 training iterations with weight perturbation
  - Empirical validation and improvement tracking
  - **Achieved best performance** (4.40 avg waiting time)

### Polymorphic Design
```cpp
class IScheduler {
    virtual ScheduleResult run(const std::vector<Process>&) = 0;
    virtual std::string name() const = 0;
};

// Each scheduler inherits and implements
class FCFS_Scheduler : public IScheduler { /* ... */ };
class QLearningScheduler : public IScheduler { /* ... */ };
```

### Encapsulation Example
```cpp
class ProcessManager {
private:
    std::vector<Process> processes;  // Hidden
public:
    void addProcess(const Process& p);  // Controlled access
    const std::vector<Process>& list() const;
};
```

## 📈 Experimental Validation

### Test Methodology
1. **Sample Data**: 5 processes with varying burst times
2. **Metrics**: Waiting time, turnaround time, fairness, context switches
3. **Comparison**: All 5 algorithms on identical workload
4. **Analysis**: Automatic recommendation based on characteristics

### Key Findings
- **NN-like AI outperformed SJF by 15%** on waiting time
- **Round Robin provides best fairness** (0.97) at cost of overhead
- **Q-Learning demonstrates learning capability** with competitive results
- **Fairness-Performance trade-off exists**: Fair RR vs efficient NN-like

## 🚀 Future Enhancements

- **Priority Scheduling**: Implement preemptive priority scheduling
- **Multi-core Simulation**: Extend to multiple CPU cores
- **Real System Integration**: Hook into actual process scheduler
- **Deep Learning**: Replace NN-like with actual neural networks (TensorFlow/PyTorch)
- **Genetic Algorithms**: Evolve optimal scheduler parameters
- **Real-time Analysis**: Live performance monitoring and adaptation
- **Workload Prediction**: Forecast future process characteristics
- **Energy Optimization**: Factor in power consumption into scheduling

## 🤝 Contributing

Contributions are welcome! Potential areas:

1. **New Scheduling Algorithms**:
   - Preemptive priority scheduling
   - Deadline-based scheduling (EDF)
   - Multi-queue hierarchical schedulers

2. **Enhanced AI Techniques**:
   - Deep reinforcement learning (PPO, DQN)
   - Genetic algorithm optimization
   - Evolutionary strategies

3. **Improvements**:
   - Better visualization features
   - More comprehensive benchmarks
   - Additional metrics (energy, throughput)
   - Real system integration

4. **Documentation**:
   - Algorithm analysis papers
   - Performance case studies
   - Educational tutorials

### To Contribute:
1. Fork the repository
2. Create a feature branch
3. Implement improvements
4. Test thoroughly
5. Submit a pull request

## 📚 References & Resources

### Scheduling Theory
- Operating Systems: Three Easy Pieces (OSTEP) - Chapter 7-9
- Tanenbaum's Modern Operating Systems
- Stallings' Operating Systems Internals and Design Principles

### Reinforcement Learning
- Sutton & Barto: Reinforcement Learning (Q-Learning Chapter)
- Introduction to Q-Learning
- Exploration-Exploitation Trade-off

### Implementation
- C++17 Standard Library
- SDL2 Graphics Library Documentation
- CMake Build System Guide

## 📄 License

This project is open source and available under the MIT License.

## 👨‍💻 Author & Acknowledgments

**SmartSched** demonstrates how traditional OS concepts can be enhanced with modern machine learning techniques. The project serves as both an educational tool and a practical scheduler comparison platform.

---

**Last Updated**: May 2026  
**Version**: 1.0  
**Status**: Production-ready with active development for enhancements
