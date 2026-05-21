#include "RealTaskManager.h"
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <iostream>
#include <SDL.h>

namespace RealTasks {

    double measureTaskTime(const std::string& command) {
        auto start = std::chrono::high_resolution_clock::now();
        // Run the command; on Windows, system uses cmd.exe, on *nix uses /bin/sh
        int ret = std::system(command.c_str());
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> elapsed = end - start;
        if (ret != 0) {
            // still return timing, but negative signals failure
            return -elapsed.count();
        }
        return elapsed.count();
    }

    double renderImageTask(int width, int height, int frames) {
        if (width <= 0 || height <= 0 || frames <= 0) return -1.0;

        if (SDL_Init(SDL_INIT_VIDEO) != 0) {
            std::cerr << "SDL_Init Error: " << SDL_GetError() << "\n";
            return -1.0;
        }

        SDL_Window* window = SDL_CreateWindow(
            "SmartSched Render Task",
            SDL_WINDOWPOS_CENTERED,
            SDL_WINDOWPOS_CENTERED,
            width,
            height,
            SDL_WINDOW_SHOWN
        );
        if (!window) {
            std::cerr << "SDL_CreateWindow Error: " << SDL_GetError() << "\n";
            SDL_Quit();
            return -1.0;
        }

        SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
        if (!renderer) {
            std::cerr << "SDL_CreateRenderer Error: " << SDL_GetError() << "\n";
            SDL_DestroyWindow(window);
            SDL_Quit();
            return -1.0;
        }

        // Start timing just before the rendering work
        auto start = std::chrono::high_resolution_clock::now();

        // Render a simple animated gradient for a number of frames
        for (int f = 0; f < frames; ++f) {
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
            SDL_RenderClear(renderer);

            // More intensive pixel processing
            const int step = 2; // smaller step for more work
            for (int y = 0; y < height; y += step) {
                for (int x = 0; x < width; x += step) {
                    // More complex color calculations for increased CPU work
                    float fx = static_cast<float>(x) / width;
                    float fy = static_cast<float>(y) / height;
                    float ff = static_cast<float>(f) / frames;
                    
                    // Trigonometric functions to increase CPU load
                    Uint8 r = static_cast<Uint8>(255 * (0.5 * sin(fx * 10 + ff * 3.14) + 0.5));
                    Uint8 g = static_cast<Uint8>(255 * (0.5 * cos(fy * 10 + ff * 6.28) + 0.5));
                    Uint8 b = static_cast<Uint8>(255 * (0.5 * sin((fx + fy) * 5 + ff * 9.42) + 0.5));
                    
                    SDL_Rect rect{ x, y, step, step };
                    SDL_SetRenderDrawColor(renderer, r, g, b, 255);
                    SDL_RenderFillRect(renderer, &rect);
                    
                    // Additional math operations to increase CPU load
                    for(int k = 0; k < 5; k++) {
                        r = (r + g + b) / 3;
                        g = (r * g + b) / 3;
                        b = (r + g * b) / 3;
                    }
                }
            }

            SDL_RenderPresent(renderer);

            // Process pending events quickly so Windows doesn't mark the window unresponsive
            SDL_Event e;
            while (SDL_PollEvent(&e)) {
                if (e.type == SDL_QUIT) {
                    f = frames; // early exit
                    break;
                }
            }
        }

        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> elapsed = end - start;

        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();

        return elapsed.count();
    }

    bool saveDataset(const std::string& filename, const std::vector<Process>& processes) {
        std::ofstream out(filename);
        if (!out) return false;
        for (const auto& p : processes) {
            out << "P" << p.pid << ' ' << p.arrivalTime << ' ' << p.burstTime << "\n";
        }
        return true;
    }

    std::vector<Process> loadProcessesFromFile(const std::string& filename) {
        std::vector<Process> procs;
        std::ifstream in(filename);
        if (!in) return procs;
        std::string line;
        int autoPid = 1;
        while (std::getline(in, line)) {
            if (line.empty()) continue;
            std::istringstream iss(line);
            std::string name;
            double burstDouble = 0.0;
            int at = 0;
            if (!(iss >> name >> at >> burstDouble)) continue;
            // If burstDouble is fractional, round up to nearest integer tick for simulator
            int burstTicks = static_cast<int>(burstDouble + 0.5);
            if (burstTicks <= 0) burstTicks = 1; // minimum 1 tick
            int pid = autoPid++;
            procs.emplace_back(pid, at, burstTicks, 0);
        }
        return procs;
    }
}


