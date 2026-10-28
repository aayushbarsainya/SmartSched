cmake_minimum_required(VERSION 3.16)
project(SmartSched CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/imgui
    ${CMAKE_CURRENT_SOURCE_DIR}/imgui/backends
)

# ImGui sources
set(IMGUI_SRC
    imgui/imgui.cpp
    imgui/imgui_draw.cpp
    imgui/imgui_tables.cpp
    imgui/imgui_widgets.cpp
    imgui/backends/imgui_impl_sdl2.cpp
    imgui/backends/imgui_impl_sdlrenderer2.cpp
)

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
    AppUI.cpp
    custom_tasks/Tasks.cpp
    image_tasks/ImageTasks.cpp
    cpu_stress/CpuStress.cpp
    ${IMGUI_SRC}
)

# Find SDL2 (support both vcpkg on Windows and apt package on Linux/WSL2)
if(WIN32 AND EXISTS "C:/vcpkg/installed/x64-windows/share/sdl2")
    set(SDL2_DIR "C:/vcpkg/installed/x64-windows/share/sdl2")
endif()

find_package(SDL2 CONFIG QUIET)
if(NOT SDL2_FOUND)
    find_package(SDL2 QUIET)
endif()

if(NOT SDL2_FOUND)
    find_package(PkgConfig QUIET)
    if(PKG_CONFIG_FOUND)
        pkg_check_modules(SDL2 sdl2)
    endif()
    if(SDL2_FOUND)
        add_library(SDL2::SDL2 INTERFACE IMPORTED)
        target_include_directories(SDL2::SDL2 INTERFACE ${SDL2_INCLUDE_DIRS})
        target_link_libraries(SDL2::SDL2 INTERFACE ${SDL2_LIBRARIES})
    else()
        message(FATAL_ERROR "\n=======================================================\nSDL2 was not found!\nOn Ubuntu / WSL2, run:\n   sudo apt update && sudo apt install -y libsdl2-dev\nOn Windows, run:\n   vcpkg install sdl2:x64-windows\n=======================================================\n")
    endif()
endif()

add_executable(SmartSched ${SRC})

if(TARGET SDL2::SDL2main)
    target_link_libraries(SmartSched PRIVATE SDL2::SDL2main SDL2::SDL2)
else()
    target_link_libraries(SmartSched PRIVATE SDL2::SDL2)
endif()

if(UNIX AND NOT APPLE)
    target_link_libraries(SmartSched PRIVATE pthread dl)
endif()
