#include "ImageTasks.h"
#include <SDL.h>
#include <chrono>
#include <algorithm>

namespace ImageTasks {
    double renderImageBMP(const std::string& bmpPath, int frames, int minScalePct, int maxScalePct) {
        if (frames <= 0) frames = 1;
        if (minScalePct <= 0) minScalePct = 50;
        if (maxScalePct < minScalePct) maxScalePct = minScalePct;

        bool needQuit = false;
        if (SDL_WasInit(SDL_INIT_VIDEO) == 0) {
            if (SDL_Init(SDL_INIT_VIDEO) != 0) {
                return 0.0;
            }
            needQuit = true;
        }

        SDL_Surface* src = SDL_LoadBMP(bmpPath.c_str());
        if (!src) {
            if (needQuit) SDL_Quit();
            return 0.0;
        }
        SDL_Surface* src32 = SDL_ConvertSurfaceFormat(src, SDL_PIXELFORMAT_ARGB8888, 0);
        SDL_FreeSurface(src);
        if (!src32) {
            if (needQuit) SDL_Quit();
            return 0.0;
        }

        int w = src32->w;
        int h = src32->h;

        auto t0 = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < frames; ++i) {
            // Scale factor oscillates between min and max
            int pct = minScalePct + (i % std::max(1, (maxScalePct - minScalePct + 1)));
            int dw = std::max(1, w * pct / 100);
            int dh = std::max(1, h * pct / 100);

            SDL_Surface* dst = SDL_CreateRGBSurfaceWithFormat(0, dw, dh, 32, SDL_PIXELFORMAT_ARGB8888);
            if (!dst) continue;

            SDL_Rect srcR{ 0,0,w,h };
            SDL_Rect dstR{ 0,0,dw,dh };
            SDL_BlitScaled(src32, &srcR, dst, &dstR);

            // Downscale back to original to create more work
            SDL_Surface* back = SDL_CreateRGBSurfaceWithFormat(0, w, h, 32, SDL_PIXELFORMAT_ARGB8888);
            if (back) {
                SDL_Rect backR{ 0,0,w,h };
                SDL_BlitScaled(dst, nullptr, back, &backR);
                SDL_FreeSurface(back);
            }
            SDL_FreeSurface(dst);
        }
        auto t1 = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> diff = t1 - t0;

        SDL_FreeSurface(src32);
        if (needQuit) SDL_Quit();
        return diff.count();
    }
}


