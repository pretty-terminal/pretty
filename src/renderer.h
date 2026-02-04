#ifndef RENDERER_H
    #define RENDERER_H

    #include <SDL3/SDL.h>
    #include <SDL3/SDL_video.h>

    #include "font.h"
    #include "config.h"
    #include "types.h"

typedef struct {
    SDL_Texture *texture;
    SDL_FRect glyphs[128];
    int w, h;
} glyph_atlas;

struct Cell {
    unsigned char ch;
    uint8_t fg;
    uint8_t bg;
};

struct Grid {
    int rows;
    int cols;
    Cell *cells;
};

struct dim {
    int width;
    int height;
};

void display_fps_metrics(SDL_Window *win, const SDL_DisplayMode *mode);
glyph_atlas* create_atlas(SDL_Renderer *renderer, TTF_Font *font, generic_config *conf);
void grid_resize(Grid *grid, int new_rows, int new_cols);
void rebuild_grid_from_buffer(Grid *grid, term *pretty, char *ring_buffer, size_t ring_size);
bool render_frame(
    SDL_Renderer *renderer,
    glyph_atlas *atlas,
    Grid *grid,
    generic_config *conf
);

#endif // RENDERER_H
