# SmartSched - CPU Scheduling Algorithm Simulator

SmartSched is a CPU scheduling algorithm simulator that implements and compares various traditional and AI-based scheduling techniques using real-world task simulations. The project uses SDL2 to create actual computational workloads through image rendering tasks.

## Scheduling Algorithms Implemented

### 1. First Come First Serve (FCFS)
- Processes are executed in the order they arrive
- Non-preemptive scheduling algorithm
- Simple to implement but may not provide optimal turnaround times
- Best for batch systems or when process times are similar

### 2. Shortest Job First (SJF)
- Selects the process with the shortest burst time
- Non-preemptive implementation
- Optimal average waiting time for a given set of processes
- May lead to starvation of longer processes

### 3. Round Robin (RR)
- Time-slice based scheduling with preemption
- Fair distribution of CPU time among processes
- Good for interactive systems
- Performance depends on quantum size
- Implements context switching after each time quantum

### 4. Q-Learning AI Scheduler
- Reinforcement learning based scheduler
- Learns optimal scheduling decisions through experience
- Adapts to different workload patterns
- Balances between exploration and exploitation
- Uses state-action-reward mechanism to improve scheduling decisions

### 5. Neural Network-like (NN-like) AI Scheduler
- Implements a simplified neural network approach
- Makes scheduling decisions based on multiple process attributes
- Considers process burst time, waiting time, and priority
- Adapts to workload patterns during execution
- Shows better performance with diverse workloads

## Project Features

- Real-world workload simulation using SDL2 graphics rendering
- Comprehensive performance metrics:
  - Average Waiting Time
  - Average Turnaround Time
  - CPU Utilization
  - Throughput
- Support for both sample data and real task measurements
- Ability to save and load task datasets
- Visual representation of scheduling timelines

## Building and Running

### Prerequisites
1. CMake (3.16 or higher)
2. C++ Compiler with C++17 support
3. SDL2 library
4. vcpkg (recommended for SDL2 installation)

### Installing SDL2 using vcpkg
```powershell
# Clone vcpkg if you haven't already
git clone https://github.com/Microsoft/vcpkg.git
cd vcpkg
./bootstrap-vcpkg.bat

# Install SDL2 for your system
./vcpkg install sdl2:x64-windows

# Optional: Integrate with Visual Studio (if using VS)
./vcpkg integrate install
```

### Building the Project
```powershell
# Clone the repository
git clone https://github.com/yourusername/SmartSched.git
cd SmartSched

# Configure with CMake
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=[path_to_vcpkg]/scripts/buildsystems/vcpkg.cmake

# Build
cmake --build build --config Release
```

### Running the Program
```powershell
# Run the executable
./build/Release/SmartSched.exe
```

## Usage

The program offers three modes of operation:

1. Sample Data Mode (Option 1):
   - Uses predefined sample processes for quick testing
   - Good for understanding basic scheduler behavior

2. Real Task Measurement (Option 2):
   - Simulates real workloads using SDL2 rendering
   - Automatically processes 10 diverse rendering tasks
   - Tasks range from light (720p) to heavy (4K) workloads

3. Load from File (Option 3):
   - Load previously saved task measurements
   - Useful for comparing scheduler performance on consistent datasets

## Performance Results

In our tests with diverse workloads, the schedulers showed the following characteristics:

- NN-like AI: Best overall performance with lowest average waiting time
- SJF: Good performance for shorter tasks
- FCFS: Consistent but not optimal performance
- Q-Learning: Adaptive performance improving over time
- Round Robin: Fair CPU distribution but higher overhead

## Contributing

Feel free to contribute to this project by:
- Implementing new scheduling algorithms
- Improving existing AI-based schedulers
- Adding more realistic workload simulations
- Enhancing performance metrics and visualization

## License

This project is licensed under the MIT License - see the LICENSE file for details.