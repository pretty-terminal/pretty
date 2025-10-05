#ifndef RENDERER_H
    #define RENDERER_H

    #include <SDL3/SDL.h>

    #include "font.h"
    #include "config.h"

typedef struct {
    SDL_Surface *surface;
    SDL_Texture *texture;
    size_t length;
} line_renderer;

struct dim {
    int width;
    int height;
};

void display_fps_metrics(SDL_Window *win);
bool render_frame(
    SDL_Renderer *renderer,
    struct dim win_size,
    const char *text,
    font_info *font,
    generic_config *conf
);

#endif // RENDERER_H
