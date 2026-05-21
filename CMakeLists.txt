cmake_minimum_required(VERSION 3.16)
project(SmartSched CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Sources
set(SRC
    main.cpp
    Utils.cpp
    Metrics.cpp
    ProcessManager.cpp
    TraditionalSchedulers.cpp
    AISchedulers.cpp
    RealTaskManager.cpp
    Visualization.cpp
    custom_tasks/Tasks.cpp
    image_tasks/ImageTasks.cpp
    cpu_stress/CpuStress.cpp
)

# Find SDL2
set(SDL2_DIR "C:/vcpkg/installed/x64-windows/share/sdl2")
find_package(SDL2 CONFIG REQUIRED)

add_executable(SmartSched ${SRC})

target_link_libraries(SmartSched PRIVATE SDL2::SDL2main SDL2::SDL2)
