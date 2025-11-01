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

static const char *event_name[] = {
    FOREACH_EVENT(GENERATE_STRING)
};

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
void read_to_buff(
    tty_state *tty,
    char *buff,
    size_t buff_size,
    size_t *buff_pos,
    bool scroll
);
void scroll(SDL_Renderer *renderer, tty_state *tty, enum event scroll);

#endif // RENDERER_H
