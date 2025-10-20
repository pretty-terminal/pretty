#include <stdio.h>

#include "SDL3/SDL_render.h"
#include "macro_utils.h"
#include "renderer.h"

void display_fps_metrics(SDL_Window *win)
{
    static unsigned short frames = 0;
    static Uint64 last_time = 0;

    frames++;
    Uint64 current_time = SDL_GetTicks();

    if (current_time - last_time >= 1000) {
        char title[length_of("Pretty | ...... fps")];

        snprintf(title, sizeof title, "Pretty | %6hu fps", frames);
        SDL_SetWindowTitle(win, title);
        last_time = current_time;
        frames = 0;
    }
}

/* Note: This does the job of TTF_RenderText_Blended_Wrapped,
 * without computing Wraplines and thus is faster.
 *
 * Sadly, we still need to create a bunch of surface/texture.
 * TODO: investigate
 **/
bool render_frame(
    SDL_Renderer *renderer,
    struct dim win_size,
    const char *text,
    font_info *font,
    generic_config *conf
) {
    SDL_RenderClear(renderer);

    SDL_Color text_color = { HEX_TO_RGB(conf->color_palette[15]), .a=255 };

    int x = conf->win_padding;
    int y = conf->win_padding;

    if ((2 * conf->win_padding + font->advance) >= win_size.width) {
        /* We dont have room to render anything, so dont */
        SDL_RenderPresent(renderer);
        return true;
    }

    size_t line_max_width = (win_size.width - (2 * conf->win_padding)) / font->advance;
    uint8_t line_max_count= (win_size.height - (2 * conf->win_padding)) / font->line_skip;
    uint8_t line_count = 0;
    char const *p = text;

    for (; *p != '\0';) {
        line_renderer line = { 0 };

        if (line_count == line_max_count) {
            SDL_RenderClear(renderer);
            x = conf->win_padding;
            y = conf->win_padding;
            line_count = 0;
        }

        for (; *p != '\0' && *p != '\n';) {
            if (line.length == line_max_width)
                break;
            line.length++;
            p++;
        }

        if (line.length == 0)
            goto skip_line;

        line.surface = TTF_RenderText_Blended(
            font->ttf, text, line.length, text_color);
        if (line.surface == NULL)
            return false;

        line.texture = SDL_CreateTextureFromSurface(renderer, line.surface);
        if (line.texture == NULL)
            return false;

        /* TODO: font size is known */
        SDL_FRect dst = { x, y, line.texture->w, line.texture->h };
        SDL_RenderTexture(renderer, line.texture, NULL, &dst);
        SDL_DestroySurface(line.surface);
        SDL_DestroyTexture(line.texture);

skip_line:
        y += font->line_skip;
        if (*p == '\n')
            p++;
        text = p;

        line_count++;
    }

    SDL_RenderPresent(renderer);
    return true;
}
