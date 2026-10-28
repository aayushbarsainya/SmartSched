#include "RealTaskManager.h"
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <iostream>
#include <cmath>
#include <SDL.h>

namespace RealTasks {

    double measureTaskTime(const std::string& command) {
        auto start = std::chrono::high_resolution_clock::now();
        int ret = std::system(command.c_str());
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> elapsed = end - start;
        if (ret != 0) {
            return -elapsed.count();
        }
        return elapsed.count();
    }

    double renderImageTask(int width, int height, int frames) {
        if (width <= 0 || height <= 0 || frames <= 0) return -1.0;

        bool needQuit = false;
        if (SDL_WasInit(SDL_INIT_VIDEO) == 0) {
            if (SDL_Init(SDL_INIT_VIDEO) != 0) {
                std::cerr << "SDL_Init Error: " << SDL_GetError() << "\n";
                return -1.0;
            }
            needQuit = true;
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
            if (needQuit) SDL_Quit();
            return -1.0;
        }

        SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
        if (!renderer) {
            std::cerr << "SDL_CreateRenderer Error: " << SDL_GetError() << "\n";
            SDL_DestroyWindow(window);
            if (needQuit) SDL_Quit();
            return -1.0;
        }

        auto start = std::chrono::high_resolution_clock::now();

        for (int f = 0; f < frames; ++f) {
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
            SDL_RenderClear(renderer);

            const int step = 2;
            for (int y = 0; y < height; y += step) {
                for (int x = 0; x < width; x += step) {
                    float fx = static_cast<float>(x) / width;
                    float fy = static_cast<float>(y) / height;
                    float ff = static_cast<float>(f) / frames;
                    
                    Uint8 r = static_cast<Uint8>(255 * (0.5 * sin(fx * 10 + ff * 3.14) + 0.5));
                    Uint8 g = static_cast<Uint8>(255 * (0.5 * cos(fy * 10 + ff * 6.28) + 0.5));
                    Uint8 b = static_cast<Uint8>(255 * (0.5 * sin((fx + fy) * 5 + ff * 9.42) + 0.5));
                    
                    SDL_Rect rect{ x, y, step, step };
                    SDL_SetRenderDrawColor(renderer, r, g, b, 255);
                    SDL_RenderFillRect(renderer, &rect);
                    
                    for(int k = 0; k < 5; k++) {
                        r = (r + g + b) / 3;
                        g = (r * g + b) / 3;
                        b = (r + g * b) / 3;
                    }
                }
            }

            SDL_RenderPresent(renderer);

            SDL_Event e;
            while (SDL_PollEvent(&e)) {
                if (e.type == SDL_QUIT) {
                    f = frames;
                    break;
                }
            }
        }

        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> elapsed = end - start;

        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        if (needQuit) SDL_Quit();

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
            int burstTicks = static_cast<int>(burstDouble + 0.5);
            if (burstTicks <= 0) burstTicks = 1;
            int pid = autoPid++;
            procs.emplace_back(pid, at, burstTicks, 0);
        }
        return procs;
    }
}
