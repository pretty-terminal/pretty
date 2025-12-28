#ifndef RENDERER_H
    #define RENDERER_H

    #include <SDL3/SDL.h>

    #include "font.h"
    #include "config.h"
    #include "slave.h"


#define FOREACH_EVENT(EVENT) \
        EVENT(SCROLL_UP)   \
        EVENT(SCROLL_DOWN)  \

#define GENERATE_ENUM(ENUM) ENUM,
#define GENERATE_STRING(STRING) #STRING,

enum event {
    FOREACH_EVENT(GENERATE_ENUM)
};

typedef struct {
    SDL_Texture *texture;
    SDL_FRect glyphs[128];
    int w, h;
} glyph_atlas;

struct dim {
    int width;
    int height;
};

void display_fps_metrics(SDL_Window *win);
glyph_atlas* create_atlas(SDL_Renderer *renderer, TTF_Font *font, generic_config *conf);
bool render_frame(
    SDL_Renderer *renderer,
    glyph_atlas *atlas,
    struct dim win_size,
    tty_state *tty,
    char *text,
    size_t buff_size,
    size_t *buff_pos,
    font_info *font,
    generic_config *conf
);
void read_to_buff(
    tty_state *tty,
    char *buff,
    size_t buff_size,
    size_t *buff_pos
);

void calculate_scroll(tty_state *tty, enum event dir);

#endif // RENDERER_H
