#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>

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
 */
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
    pthread_mutex_lock(&tty->lock);
    SDL_RenderClear(renderer);

    SDL_Color text_color = { HEX_TO_RGB(conf->color_palette[15]), .a=255 };

    unsigned int x = conf->pad_x;
    unsigned int y = conf->pad_y;

    size_t line_max_width = (win_size.width - (2 * x)) / font->advance;
    size_t line_max_count= (win_size.height - (2 * y)) / font->line_skip;

    size_t chars_in_window = 0;
    size_t max_chars = line_max_width * line_max_count;


    size_t pos = tty->scroll_tail;
    size_t line_count = 0;

    char *line_buffer = malloc(line_max_width + 1);
    if (line_buffer == NULL) {
        pthread_mutex_unlock(&tty->lock);
        return false;
    }

    while (pos != *buff_pos && line_count < line_max_count) {
        line_renderer line = { 0 };
        size_t line_start = pos;

        while (pos != *buff_pos && text[pos] != '\n' && line.length < line_max_width) {
            line.length++;
            pos = (pos + 1) % buff_size;
            chars_in_window++;

            if (chars_in_window >= max_chars)
                goto done_rendering;
        }

        if (line.length == 0)
            goto skip_line;

        // Render this line
        for (size_t i = 0; i < line.length; i++) {
            char c = text[(line_start + i) % buff_size];

            if (isprint(c))
                line_buffer[i] = c;
            else
                line_buffer[i] = ' ';
        }

        line_buffer[line.length] = '\0';

        line.surface = TTF_RenderText_Blended(
            font->ttf, line_buffer, line.length, text_color);
        if (line.surface == NULL) {
            free(line_buffer);
            pthread_mutex_unlock(&tty->lock);
            return false;
        }

        line.texture = SDL_CreateTextureFromSurface(renderer, line.surface);
        if (line.texture == NULL) {
            SDL_DestroySurface(line.surface);
            free(line_buffer);
            pthread_mutex_unlock(&tty->lock);
            return false;
        }

        /* TODO: font size is known */
        SDL_FRect dst = { x, y, line.texture->w, line.texture->h };
        SDL_RenderTexture(renderer, line.texture, NULL, &dst);
        SDL_DestroySurface(line.surface);
        SDL_DestroyTexture(line.texture);

skip_line:
        y += font->line_skip;

        if (pos != *buff_pos && text[pos] == '\n') {
            pos = (pos + 1) % buff_size;
            chars_in_window++;
        }

        line_count++;
    }

done_rendering:
    free(line_buffer);
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
                if (tty->buff[tty->scroll_tail] == '\n')
                    nlines++;
            }
            break;

        case SCROLL_DOWN: {
            size_t nline_end = tty->head;
            for (; tty->buff[nline_end] != '\n' && nline_end > 0; nline_end--);

            while (nlines < 1) {
                if (tty->scroll_tail == nline_end)
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
