#include <ctype.h>
#include <stdio.h>

#include "SDL3/SDL_render.h"
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

/* Note: This does the job of TTF_RenderText_Blended_Wrapped,
 * without computing Wraplines and thus is faster.
 *
 * Sadly, we still need to create a bunch of surface/texture.
 * TODO: investigate
 **/
bool render_frame(
    SDL_Renderer *renderer,
    struct dim win_size,
    tty_state *tty,
    char *text,
    size_t buff_size,
    size_t *buff_pos,
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
    char *p = text;

    for (; *p != '\0';) {
        line_renderer line = { 0 };

        if (line_count == line_max_count) {
            calculate_scroll(tty, SCROLL_DOWN);
            scroll(tty, text, buff_size, buff_pos);
            p = text;
            continue;
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

void calculate_scroll(tty_state *tty, enum event dir)
{
    pthread_mutex_lock(&tty->lock);
    uint8_t nlines = 0;

    switch (dir) {
        case SCROLL_UP:
            while (nlines < 2 && tty->scroll_tail != 0) {
                tty->scroll_tail = (tty->scroll_tail + TTY_RING_CAP - 1) % TTY_RING_CAP;
                if (tty->buff[tty->scroll_tail] == '\n')
                    nlines++;
            }
            break;

        case SCROLL_DOWN: {
            size_t nline_end = tty->head;
            for (; tty->buff[nline_end] != '\n' && nline_end > 0; nline_end--);

            while (nlines < 1) {
                if (tty->scroll_tail == nline_end + 1)
                    break;

                if (tty->buff[tty->scroll_tail] == '\n')
                    nlines++;
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

void scroll(tty_state *tty, char *buff, size_t buff_size, size_t *buff_pos)
{
    pthread_mutex_lock(&tty->lock);
    pretty_log(PRETTY_INFO, "Scrolling to position: %zu", tty->scroll_tail);
    size_t pos = tty->scroll_tail;
    size_t visible = (tty->head >= pos)
        ? (tty->head - pos)
        : (TTY_RING_CAP - (pos - tty->head));

    size_t limit = (visible < buff_size - 1) ? visible : buff_size - 1;
    for (size_t i = 0; i < limit; i++) {
        buff[i] = tty->buff[pos];
        pos = (pos + 1) % TTY_RING_CAP;
    }

    buff[limit] = '\0';
    *buff_pos = limit;

    pthread_mutex_unlock(&tty->lock);
    return;
}

void read_to_buff(
    tty_state *tty,
    char *buff,
    size_t buff_size,
    size_t *buff_pos
) {
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
            } else {
                if (*buff_pos < buff_size - 1) {
                    buff[(*buff_pos)++] = ch;
                    buff[*buff_pos] = '\0';
                    pretty_log(PRETTY_INFO, "Added char '%c' at position %zu",
                        (isprint(ch)) ? ch : '?', *buff_pos - 1);
                }
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
