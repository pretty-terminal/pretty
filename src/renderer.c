#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>

#include "SDL3/SDL_render.h"
#include "SDL3_ttf/SDL_ttf.h"
#include "macro_utils.h"
#include "renderer.h"
#include "log.h"
#include "pthread.h"
#include "slave.h"

static const char *event_name[] = {
    FOREACH_EVENT(GENERATE_STRING)
};

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

glyph_atlas* create_atlas(SDL_Renderer *renderer, TTF_Font *font, generic_config *conf)
{
    glyph_atlas *atlas = malloc(sizeof(glyph_atlas));

    int minx, maxx, miny, maxy, advance;
    TTF_GetGlyphMetrics(font, 'M', &minx, &maxx, &miny, &maxy, &advance);
    atlas->w = advance;
    atlas->h = TTF_GetFontHeight(font);

    int atlas_w = atlas->w * 16;
    int atlas_h = atlas->h * 8;
    atlas->texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888,
                                       SDL_TEXTUREACCESS_TARGET, atlas_w, atlas_h);

    SDL_SetTextureBlendMode(atlas->texture, SDL_BLENDMODE_BLEND);
    SDL_SetRenderTarget(renderer, atlas->texture);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
    SDL_RenderClear(renderer);


    SDL_Color text_color = { HEX_TO_RGB(conf->color_palette[15]), .a=255 };
    SDL_Color bg_color = { HEX_TO_RGB(conf->color_palette[COLOR_BACKGROUND]), .a=255 };

    for (int i = 32; i < 127; i++) {
        SDL_Surface *s = TTF_RenderGlyph_LCD(font, i, text_color, bg_color);
        if (!s) continue;

        SDL_Texture *t = SDL_CreateTextureFromSurface(renderer, s);

        int col = i % 16;
        int row = i / 16;

        SDL_FRect dst = { 
            (float)(col * atlas->w),
            (float)(row * atlas->h),
            (float)s->w,
            (float)s->h
        };

        SDL_RenderTexture(renderer, t, NULL, &dst);

        // Save the source rect for later
        atlas->glyphs[i] = dst;

        SDL_DestroySurface(s);
        SDL_DestroyTexture(t);
    }

    SDL_SetRenderTarget(renderer, NULL);
    return atlas;
}

bool render_frame(
    SDL_Renderer *renderer,
    glyph_atlas *atlas,
    struct dim win_size,
    tty_state *tty,
    char *text,
    size_t buff_size,
    size_t *buff_pos,
    font_info *font,
    generic_config *conf)
{
    pthread_mutex_lock(&tty->lock);

    SDL_Color bg = { HEX_TO_RGB(conf->color_palette[COLOR_BACKGROUND]), .a=255 };
    SDL_SetRenderDrawColor(renderer, bg.r, bg.g, bg.b, bg.a);

    SDL_RenderClear(renderer);

    unsigned int x = conf->pad_x;
    unsigned int y = conf->pad_y;

    size_t line_max_width = (win_size.width - (2 * x)) / font->advance;
    size_t line_max_count= (win_size.height - (2 * y)) / font->line_skip;

    size_t pos = tty->scroll_tail;
    size_t line_count = 0;

    while (pos != *buff_pos && line_count < line_max_count) {
        size_t current_line_len = 0;

        while (pos != *buff_pos
            && text[pos] != '\n'
            && current_line_len < line_max_width)
        {
            char c = text[pos];

            if (isprint(c)) {
                SDL_FRect src_rect = atlas->glyphs[(unsigned char)c];

                SDL_FRect dst_rect = { 
                    (float)(x + (current_line_len * font->advance)),
                    (float)y, 
                    (float)font->advance, 
                    (float)font->line_skip 
                };

                SDL_RenderTexture(renderer, atlas->texture, &src_rect, &dst_rect);
            }

            current_line_len++;
            pos = (pos + 1) % buff_size;
        }

        if (pos != *buff_pos && text[pos] == '\n') pos = (pos + 1) % buff_size;

        line_count++;

        if (line_count < line_max_count) y += font->line_skip;

    }

    SDL_RenderPresent(renderer);
    pthread_mutex_unlock(&tty->lock);

    return true;
}

void calculate_scroll(tty_state *tty, enum event dir)
{
    pthread_mutex_lock(&tty->lock);
    size_t nlines = 0;

    switch (dir) {
        case SCROLL_UP:
            while (nlines < 2 && tty->scroll_tail != 0) {
                tty->scroll_tail = (tty->scroll_tail + TTY_RING_CAP - 1) % TTY_RING_CAP;
                if (tty->buff[tty->scroll_tail] == '\n') nlines++;
            }
            break;
        case SCROLL_DOWN: {
            size_t nline_end = tty->head;
            for (; tty->buff[nline_end] != '\n' && nline_end > 0; nline_end--);

            while (nlines < 1) {
                if (tty->scroll_tail == nline_end) break;
                if (tty->buff[tty->scroll_tail] == '\n') nlines++;

                tty->scroll_tail = (tty->scroll_tail + 1) % TTY_RING_CAP;
            }
            break;
        }
        default:
            pthread_mutex_unlock(&tty->lock);
            pretty_log(PRETTY_ERROR, "unhandled scroll event %d", dir);
            return;
    }

    pretty_log(PRETTY_DEBUG, "scroll: event=%s, tail=%zu head=%zu", 
            event_name[dir], tty->scroll_tail, tty->head);
    pthread_mutex_unlock(&tty->lock);
}

void read_to_buff(
    tty_state *tty,
    char *buff,
    size_t buff_size,
    size_t *buff_pos)
{
    pthread_mutex_lock(&tty->lock);

    const char *p;
    size_t new_bytes = ring_read_span(tty, &p);

    if (new_bytes) {
        pretty_log(PRETTY_INFO, "Processing %zu new bytes (consumed: %zu, total: %zu)",
           new_bytes, tty->tail, tty->head);

        for (size_t i = 0; i < new_bytes; i++) {
            char ch = p[i];

            if (ch == '\b' || ch == 0x7f) {
                if (*buff_pos > 0) {
                    (*buff_pos)--;
                    pretty_log(PRETTY_INFO, "Backspace: removed char at position %zu", *buff_pos);
                    buff[*buff_pos] = '\0';
                }
            } else if (*buff_pos < buff_size - 1) {
                buff[(*buff_pos)++] = ch;
                buff[*buff_pos] = '\0';
                pretty_log(PRETTY_INFO, "Added char '%c' at position %zu",
                    (isprint(ch)) ? ch : '?', *buff_pos - 1);
            }

            if (*buff_pos >= buff_size - 1) {
                pretty_log(PRETTY_WARN, "buff_pos overflow, resseting");
                *buff_pos = 0;
            }
        }
        ring_consume(tty, new_bytes);
    }
    pthread_mutex_unlock(&tty->lock);
}
