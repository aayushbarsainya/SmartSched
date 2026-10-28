# SmartSched - WSL2 Setup, Execution & CPU Performance Monitoring Guide

This guide walks you through compiling, running, and monitoring **SmartSched** with its new **Interactive Graphical User Interface (GUI)** inside **WSL2 (Windows Subsystem for Linux)**.

---

## 1. System Requirements & Installations

### A. What Needs to Be Installed on Windows
1. **WSL2 & Ubuntu**:
   - Already installed and running on your system (`Ubuntu` on WSL2 with Linux kernel 6.x).
2. **WSLg (WSL GUI Support)**:
   - Built into Windows 10 (Build 19044+) and Windows 11.
   - WSLg forwards Linux graphical applications (SDL2 / OpenGL / Wayland) directly to your Windows desktop with zero setup.
3. **Windows Terminal** *(Recommended)*:
   - Available on Microsoft Store or preinstalled. Allows having multiple tabs (one for running the app, one for monitoring tools).
4. **Windows Task Manager**:
   - Built into Windows (press `Ctrl + Shift + Esc`).

---

### B. What Needs to Be Installed Inside WSL2 (Ubuntu)
Open your WSL2 Ubuntu terminal and run the following command to install the required libraries and monitoring tools:

```bash
sudo apt update
sudo apt install -y build-essential cmake libsdl2-dev htop sysstat
```

- **`build-essential`**: Installs `g++`, `gcc`, and `make` (C++17 compiler).
- **`cmake`**: Version >= 3.16 build generator.
- **`libsdl2-dev`**: SDL2 development libraries for windowing, rendering, and input.
- **`htop`**: Real-time interactive CPU core monitor.
- **`sysstat`**: Provides `mpstat` for per-core utilization and context switch tracking.

---

## 2. Navigating to the Project and Building

### Option A: Direct Execution from Windows Mount (Fastest)
Your Windows `C:` drive is automatically mounted inside WSL2 at `/mnt/c/`. You do not have to manually copy files:

```bash
# Navigate to the SmartSched folder
cd "/mnt/c/Users/Aayush Barsainya/Downloads/OS_CourseProject/SmartSched"

# Generate build files with CMake
cmake -B build-wsl -S .

# Compile the project
cmake --build build-wsl -j$(nproc)
```

### Option B: Copy into WSL2 Linux Filesystem (Fastest Disk I/O)
If you prefer running entirely within the Linux ext4 filesystem:

```bash
# Copy the project to your Linux home directory
cp -r "/mnt/c/Users/Aayush Barsainya/Downloads/OS_CourseProject/SmartSched" ~/SmartSched
cd ~/SmartSched

# Generate build files and compile
cmake -B build-wsl -S .
cmake --build build-wsl -j$(nproc)
```

---

## 3. Running SmartSched

### Launching the Graphical User Interface (Default)
In your WSL2 terminal, simply execute:

```bash
./build-wsl/SmartSched
```

A native desktop window titled **"SmartSched - Interactive CPU Scheduler Simulator & Visualizer"** will open on your Windows desktop via WSLg.

#### Supported Workload Generation Modes in the GUI:
1. **Mode 1: Manual / Sample Presets**:
   - Interactive process table (PID, Arrival, Burst, Priority) + presets (*Standard 5-Task Sample*, *Convoy Effect*, *Bursty Arrivals*).
2. **Mode 2: Real SDL2 Render Tasks (Graphics)**:
   - Runs 10 actual graphics workloads (ranging from 640x480 to 4K 120 frames), measures real hardware execution time, and converts elapsed seconds into scheduling burst ticks.
3. **Mode 3: Load / Save Dataset File (`real_data.txt`)**:
   - Save the current queue to disk or load previously recorded task datasets.
4. **Mode 4: Real Mathematical Equation Tasks (CPU Tasks)**:
   - Computes 4 actual heavy mathematical equations on your CPU and measures real wall-clock seconds to derive burst ticks:
     - **k-th Prime Number** (Sieve & trial division)
     - **Fibonacci(k)** (Iterative sequence computation)
     - **Factorial(k)** (Multiplicative product)
     - **k-th Palindrome Number** (Decimal reversal search)
   - Displays computed values, execution times, and assigns burst ticks automatically.
5. **Mode 5: Real BMP Image Processing**:
   - Performs software scaling blits over multi-frame BMP images (e.g. `sample-1024x1024.bmp`) to benchmark image processing workloads.
6. **Mode 6: Hardware CPU Stress & Replay (Core Pinning)**:
   - Replays scheduling decisions as active CPU compute loops on your physical hardware, with core affinity pinning and real-time Linux `/proc/self/sched` kernel telemetry.

#### Real-Life Operating System Implementations (New Tabs):
- **Live Concurrent Task Race (Suggestion 1)**:
  - 4 real concurrent workloads (`Video 4K Encoder`, `Database Query`, `Audio MP3 Transcoder`, `Mouse Event Handler`) compete for a single physical CPU core in real-time.
  - Live animated progress bars visually demonstrate the **Convoy Effect** under FCFS, **fair time-slicing** under Round Robin, and **starvation vs turnaround optimization** under SJF.
- **Context Switch Overhead & Quantum Tradeoff Curve (Suggestion 3)**:
  - Demonstrates the physical cost of context switching on hardware.
  - Runs a standardized compute benchmark varying the Round Robin quantum from 100ms down to 0.5ms.
  - Generates a live interactive plot mapping the **Thrashing Zone** (< 2ms), the **OS Sweet Spot** (10ms - 35ms), and the **Batch Processing Zone** (> 50ms).

### Launching in Terminal CLI Mode (Optional)
If you ever want to run the legacy command-line interface instead of the GUI:

```bash
./build-wsl/SmartSched --cli
```

---

## 4. How to Monitor Actual CPU Performance in Reality

To prove real kernel and hardware performance changes (e.g. for an OS project demonstration or report), use any of the following three methods:

---

### Method 1: Direct Kernel Telemetry Inside SmartSched GUI
1. In the SmartSched GUI, open the **"Real CPU Stress & Kernel Benchmark"** tab.
2. Select an algorithm timeline (e.g. Round Robin vs SJF).
3. Set CPU Core Target to **"Pin to Core 1"** and set `ms per Tick` to `50` or higher.
4. Click **"RUN REAL CPU WORKLOAD"**.
5. Look at the blue **"REAL LINUX KERNEL SCHEDULER TELEMETRY"** box. It directly reads `/proc/self/sched` from the Linux kernel and displays:
   - **Kernel CPU Exec Runtime Delta**: Real millisecond compute time scheduled by the Linux kernel.
   - **Linux Voluntary Context Switches**: Yields triggered when waiting or sleeping.
   - **Linux Involuntary Preemptions**: Real kernel preemption events where the Linux CFS/EEVDF scheduler stripped the CPU from the process because its timeslice expired!

---

### Method 2: Live WSL2 Terminal Monitors (`htop` & `mpstat`)
Open a **second tab or window** in Windows Terminal running WSL2 while SmartSched is active:

#### A. Interactive Core Monitor (`htop`):
```bash
htop
```
- In `htop`, you will see a separate percentage bar for every CPU core (e.g. Core 1, Core 2, Core 3...).
- In SmartSched, click **"RUN REAL CPU WORKLOAD"** with **"Pin to Core 1"**.
- In `htop`, you will watch **CPU 1 immediately spike to 100% utilization** while other cores remain idle, proving hardware core affinity (`sched_setaffinity`).

#### B. Detailed CPU Utilization & Context Switches (`mpstat`):
```bash
mpstat -P ALL 1
```
- Displays per-second statistics for every core.
- Look at `%usr` (user-space load) and `intr/s` (hardware interrupts).

#### C. Reading Kernel Process Scheduler State Directly:
```bash
# Find SmartSched process ID
pidof SmartSched

# Inspect real kernel scheduler telemetry for that PID
cat /proc/$(pidof SmartSched)/sched
```

---

### Method 3: Windows Task Manager (Logical Processors View)
You can also watch the effect from the Windows host OS:
1. Press `Ctrl + Shift + Esc` on Windows to open **Task Manager**.
2. Click the **Performance** tab on the left sidebar.
3. Click on **CPU**.
4. Right-click the big CPU graph -> **Change graph to** -> **Logical processors**.
5. You will now see small grid graphs for every CPU thread on your laptop (e.g., 8, 12, or 16 threads).
6. In SmartSched, run a workload with Core Affinity set.
7. Notice the corresponding logical processor graph on Windows spike to near 100% capacity!
8. Under the **Processes** tab, the Windows service **`VmmemWSL`** (Hyper-V VM engine) will reflect the increased CPU utilization.

---

## 5. Summary of Key Files

| File | Purpose |
|---|---|
| `AppUI.h` / `AppUI.cpp` | Complete Dear ImGui + SDL2 graphical user interface implementation. |
| `Visualization.h` / `Visualization.cpp` | Native SDL2 Gantt chart renderer. |
| `cpu_stress/CpuStress.cpp` | Real CPU stress engine with Linux `sched_setaffinity` & `/proc/self/sched` reader. |
| `TraditionalSchedulers.cpp` | Implementations of FCFS, SJF, and Round Robin. |
| `AISchedulers.cpp` | Q-Learning and Neural Network-like heuristic adaptive schedulers. |
| `CMakeLists.txt` | Cross-platform build script configured for WSL2 Ubuntu & Windows. |
| `imgui/` | Bundled Dear ImGui immediate-mode GUI library. |
