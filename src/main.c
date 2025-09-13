#include <stdlib.h>

#include <SDL3/SDL.h>

int main(void)
{
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("Couldn't initialize SDL: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    SDL_Window *win;
    SDL_Renderer *renderer;

    if (!SDL_CreateWindowAndRenderer("examples/renderer/clear", 1280, 720, 0, &win, &renderer)) {
        SDL_Log("Couldn't create window/renderer: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    bool is_running = true;

    while (is_running) {
        SDL_Event event;

        SDL_RenderClear(renderer);
        SDL_RenderPresent(renderer);
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                is_running = false;
                break;
            }
        }
    }

    SDL_DestroyWindow(win);
    return EXIT_SUCCESS;
}
