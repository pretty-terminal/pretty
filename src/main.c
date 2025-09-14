#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

#include <SDL3/SDL.h>

#include "macro_utils.h"

static void display_fps_metrics(SDL_Window *win)
{
    static unsigned short frames = 0;
    static Uint64 last_time = 0;

    frames++;
    Uint64 current_time = SDL_GetTicks();

    if (current_time - last_time >= 1000) {
        char title[length_of("Pretty | ..... fps")];

        snprintf(title, sizeof title, "Pretty | %5hu fps", frames);
        SDL_SetWindowTitle(win, title);
        last_time = current_time;
        frames = 0;
    }
}

int main(void)
{
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("Couldn't initialize SDL: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    SDL_Window *win;
    SDL_Renderer *renderer;

    if (!SDL_CreateWindowAndRenderer(
            "examples/renderer/clear", 720, 480, 0, &win, &renderer)) {
        SDL_Log("Couldn't create window/renderer: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    for (bool is_running = true; is_running;) {
        display_fps_metrics(win);

        SDL_RenderClear(renderer);
        SDL_RenderPresent(renderer);

        for (SDL_Event event; SDL_PollEvent(&event);) {
            if (event.type == SDL_EVENT_QUIT) {
                is_running = false;
                break;
            }
        }
    }

    SDL_DestroyWindow(win);
    return EXIT_SUCCESS;
}
