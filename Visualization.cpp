#include "Visualization.h"
#include <SDL.h>
#include <cmath>
#include <unordered_map>
#include <algorithm>

namespace {
    struct Color { Uint8 r, g, b; };
    static Color kPalette[] = {
        {0x4e,0xc9,0xb0}, {0xff,0x99,0x33}, {0x66,0x99,0xcc}, {0xed,0x56,0x5a},
        {0x9a,0x66,0xcc}, {0xff,0xcc,0x00}, {0x66,0xcc,0x66}, {0xcc,0x66,0x66},
        {0x33,0x99,0xcc}, {0xcc,0x99,0x33}, {0x99,0x66,0xcc}, {0x66,0xcc,0x99}
    };

    Color colorForPid(int pid) {
        size_t idx = static_cast<size_t>(std::abs(pid)) % (sizeof(kPalette) / sizeof(kPalette[0]));
        return kPalette[idx];
    }

    void drawFilledRect(SDL_Renderer* r, int x, int y, int w, int h, Color c) {
        SDL_Rect rect{ x, y, w, h };
        SDL_SetRenderDrawColor(r, c.r, c.g, c.b, 255);
        SDL_RenderFillRect(r, &rect);
    }

    void drawRect(SDL_Renderer* r, int x, int y, int w, int h, Color c) {
        SDL_Rect rect{ x, y, w, h };
        SDL_SetRenderDrawColor(r, c.r, c.g, c.b, 255);
        SDL_RenderDrawRect(r, &rect);
    }

    void drawBar(SDL_Renderer* r, int x, int y, int w, int h, const Color& c) {
        drawFilledRect(r, x, y, w, h, c);
        Color border{ Uint8(std::max(0, c.r - 30)), Uint8(std::max(0, c.g - 30)), Uint8(std::max(0, c.b - 30)) };
        drawRect(r, x, y, w, h, border);
    }

    int timelineEnd(const std::vector<TimeSlice>& t) {
        if (t.empty()) return 0;
        return t.back().endTime;
    }

    void renderTimeline(SDL_Renderer* renderer,
                        const ScheduleResult& res,
                        int W, int H,
                        float originX, float scaleX,
                        int rowTop,
                        int rowHeight) {
        SDL_SetRenderDrawColor(renderer, 20, 22, 25, 255);
        SDL_RenderClear(renderer);

        const auto& tl = res.timeline;
        int maxT = std::max(0, timelineEnd(tl));
        int pixelsPerTick = std::max(1, int(scaleX));
        int step = (pixelsPerTick >= 40) ? 1 : (pixelsPerTick >= 20 ? 2 : (pixelsPerTick >= 10 ? 5 : 10));

        // Grid
        for (int t = 0; t <= maxT; t += step) {
            int x = int(originX + t * scaleX);
            if (x < 0 || x > W) continue;
            SDL_SetRenderDrawColor(renderer, 60, 65, 72, 255);
            SDL_RenderDrawLine(renderer, x, 0, x, H);
        }

        // Bars
        int y = rowTop;
        for (const auto& ts : tl) {
            int x0 = int(originX + ts.startTime * scaleX);
            int x1 = int(originX + ts.endTime * scaleX);
            int w = x1 - x0;
            if (w <= 0) continue;
            Color c = colorForPid(ts.pid);
            drawBar(renderer, x0, y, w, rowHeight, c);
        }
    }
}

namespace Viz {
    void runInteractiveVisualization(const std::vector<IScheduler*>& /*schedulers*/,
                                     const std::vector<ScheduleResult>& results,
                                     int width, int height) {
        if (results.empty()) return;

        if (SDL_Init(SDL_INIT_VIDEO) != 0) {
            return;
        }

        SDL_Window* window = SDL_CreateWindow(
            "SmartSched - Interactive Gantt",
            SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
            width, height, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
        if (!window) {
            SDL_Quit();
            return;
        }

        SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
        if (!renderer) {
            SDL_DestroyWindow(window);
            SDL_Quit();
            return;
        }

        int W = width, H = height;
        float originX = 60.0f;
        float scaleX = 40.0f;
        const int topMargin = 120;
        const int rowHeight = 40;
        const int rowGap = 24;

        bool running = true;
        while (running) {
            SDL_Event e;
            while (SDL_PollEvent(&e)) {
                if (e.type == SDL_QUIT) running = false;
                if (e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
                    W = e.window.data1;
                    H = e.window.data2;
                }
                if (e.type == SDL_KEYDOWN) {
                    switch (e.key.keysym.sym) {
                        case SDLK_ESCAPE: running = false; break;
                        case SDLK_a: originX += 40; break;
                        case SDLK_d: originX -= 40; break;
                        case SDLK_w: scaleX = std::min(400.0f, scaleX * 1.2f); break;
                        case SDLK_s: scaleX = std::max(2.0f,   scaleX / 1.2f); break;
                    }
                }
                if (e.type == SDL_MOUSEWHEEL) {
                    int ix, iy; SDL_GetMouseState(&ix, &iy);
                    float mx = float(ix);
                    float oldScale = scaleX;
                    if (e.wheel.y > 0) scaleX = std::min(400.0f, scaleX * 1.1f);
                    if (e.wheel.y < 0) scaleX = std::max(2.0f, scaleX / 1.1f);
                    float tAtMouse = (mx - originX) / oldScale;
                    originX = mx - tAtMouse * scaleX;
                }
            }

            // Clear background
            SDL_SetRenderDrawColor(renderer, 20, 22, 25, 255);
            SDL_RenderClear(renderer);

            // Global grid using the longest timeline
            int globalMaxT = 0;
            for (const auto& r : results) {
                globalMaxT = std::max(globalMaxT, r.timeline.empty() ? 0 : r.timeline.back().endTime);
            }
            int pixelsPerTick = std::max(1, int(scaleX));
            int step = (pixelsPerTick >= 40) ? 1 : (pixelsPerTick >= 20 ? 2 : (pixelsPerTick >= 10 ? 5 : 10));
            for (int t = 0; t <= globalMaxT; t += step) {
                int x = int(originX + t * scaleX);
                if (x < 0 || x > W) continue;
                SDL_SetRenderDrawColor(renderer, 60, 65, 72, 255);
                SDL_RenderDrawLine(renderer, x, 0, x, H);
            }

            // Draw each scheduler timeline stacked
            int rowY = topMargin;
            for (size_t i = 0; i < results.size(); ++i) {
                // Alternating row band
                if (i % 2 == 0) {
                    SDL_SetRenderDrawColor(renderer, 28, 31, 36, 255);
                } else {
                    SDL_SetRenderDrawColor(renderer, 24, 27, 32, 255);
                }
                SDL_Rect band{ 0, rowY - 4, W, rowHeight + 8 };
                SDL_RenderFillRect(renderer, &band);

                // Render this timeline into its row (renderTimeline clears; avoid by drawing directly)
                const auto& tl = results[i].timeline;
                for (const auto& ts : tl) {
                    int x0 = int(originX + ts.startTime * scaleX);
                    int x1 = int(originX + ts.endTime * scaleX);
                    int w = x1 - x0;
                    if (w <= 0) continue;
                    Color c = colorForPid(ts.pid);
                    drawBar(renderer, x0, rowY, w, rowHeight, c);
                }

                // Fairness meter (left gutter): width proportional to Jain fairness
                int meterMaxW = 140;
                int meterH = 8;
                int meterX = 12;
                int meterY = rowY - 12;
                double jf = results[i].metrics.jainFairnessSlowdown; // 0..1
                jf = std::max(0.0, std::min(1.0, jf));
                int meterW = int(meterMaxW * jf);
                // background
                drawFilledRect(renderer, meterX, meterY, meterMaxW, meterH, Color{40,44,50});
                // value (green to yellow)
                Uint8 r = Uint8(255 - (int)(155 * jf));
                Uint8 g = Uint8(180);
                Uint8 b = Uint8(60);
                drawFilledRect(renderer, meterX, meterY, meterW, meterH, Color{ r, g, b });
                drawRect(renderer, meterX, meterY, meterMaxW, meterH, Color{20,20,20});

                // Row separator
                SDL_SetRenderDrawColor(renderer, 80, 85, 92, 255);
                SDL_RenderDrawLine(renderer, 0, rowY + rowHeight + 6, W, rowY + rowHeight + 6);

                rowY += rowHeight + rowGap;
            }

            // Banner
            SDL_SetRenderDrawColor(renderer, 34, 38, 45, 255);
            SDL_Rect banner{ 0, 0, W, 90 };
            SDL_RenderFillRect(renderer, &banner);

            // Legend (use first result to show PID colors)
            int legendY = 50;
            int legendX = 20;
            int boxW = 18, boxH = 14, gap = 8;
            std::unordered_map<int, bool> seen;
            int shown = 0;
            if (!results.empty()) {
                for (const auto& ts : results[0].timeline) {
                    if (seen[ts.pid]) continue;
                    seen[ts.pid] = true;
                    auto c = colorForPid(ts.pid);
                    drawFilledRect(renderer, legendX, legendY, boxW, boxH, c);
                    drawRect(renderer, legendX, legendY, boxW, boxH, Color{ 20,20,20 });
                    legendX += boxW + gap;
                    shown++;
                    if (shown >= 24) break;
                }
            }

            // Top separator
            SDL_SetRenderDrawColor(renderer, 80, 85, 92, 255);
            SDL_RenderDrawLine(renderer, 0, 90, W, 90);

            SDL_RenderPresent(renderer);
        }

        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
    }
}


