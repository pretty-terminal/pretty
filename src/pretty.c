#include <ctype.h>
#include <getopt.h>
#include <limits.h>
#include <pthread.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <SDL3/SDL_error.h>
#include <SDL3/SDL_pixels.h>
#include <SDL3/SDL_render.h>
#include <SDL3/SDL_video.h>

#include "config.h"
#include "macro_utils.h"
#include "pretty.h"
#include "slave.h"
#include "font.h"
#include "renderer.h"
#include "log.h"

#define SCREEN_WIDTH 1280
#define SCREEN_HEIGHT 720

#define TEST_COMMAND "python tests/plop.py\r"

static struct option LONG_OPTIONS[] = {
    {"config", required_argument, 0, 'c'},
    {0,        0,                 0,  0 }
};

void notify_ui_flush(void)
{
    static SDL_Event ev = { .type = SDL_EVENT_USER };

    SDL_PushEvent(&ev);
}

void thread_handle_quit(tty_state *tty)
{
    tty->should_exit = true;
    pretty_log(PRETTY_DEBUG, "waiting for thread [%lu] to exit", tty->thread);

    void *res;
    int s = pthread_join(tty->thread, &res);
    if (s != 0)
        pretty_log(PRETTY_ERROR, "pthread_join failed");
    else
        pretty_log(PRETTY_DEBUG, "thread [%lu] exited cleanly", tty->thread);
}

int main(int argc, char **argv)
{
    char *config_file = NULL;
    int option_index;
    int c;

    while (true) {
        c = getopt_long(argc, argv, ":c:", LONG_OPTIONS, &option_index);
        if (c < 0)
            break;
        switch (c) {
            case 'c':
                config_file = optarg;
                break;
            case '?':
                break;
            default:
                pretty_log(PRETTY_INFO, "?? getopt returned character code 0%o ??", c);
        }
    }

    if (config_file == NULL)
        config_file = get_default_config_file();
    else if (access(config_file, F_OK) != 0) {
        pretty_log(PRETTY_ERROR, "Provided config file [%s] does not exists", config_file);
        config_file = get_default_config_file();
    }

    pretty_log(PRETTY_INFO, "Loading config from [%s]", config_file);
    char *cat_config = file_read(config_file);

    generic_config *config = return_config(cat_config);

    if (config == NULL) {
        pretty_log(PRETTY_ERROR, "Failed to get config!");
        return EXIT_FAILURE;
    }

    char buff[BUFSIZ] = { 0 };
    size_t buff_pos = 0;

    tty_state tty = {
        .pty_master_fd = tty_new((char *[]){ "/bin/sh", NULL }),
        .buff_len = 0,
        .buff_changed = false,
        .lock = PTHREAD_MUTEX_INITIALIZER
    };

    if (pthread_create(&tty.thread, NULL, tty_poll_loop, &tty) != 0)
        return EXIT_FAILURE;

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        pretty_log(PRETTY_ERROR, "Couldn't initialize SDL: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    SDL_Window *win;
    SDL_Renderer *renderer;
    struct dim win_size = { SCREEN_WIDTH, SCREEN_HEIGHT };

    if (!SDL_CreateWindowAndRenderer(
        "examples/renderer/clear",
        win_size.width, win_size.height,
            SDL_WINDOW_RESIZABLE
            | SDL_WINDOW_HIGH_PIXEL_DENSITY
            | SDL_WINDOW_TRANSPARENT,
        &win, &renderer)
    ) {
        pretty_log(PRETTY_ERROR, "Couldn't create window/renderer: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    SDL_SetRenderDrawColor(renderer,
        HEX_TO_RGBA(config->color_palette[COLOR_BACKGROUND]));
#ifdef WAIT_EVENTS
    SDL_SetWindowTitle(win, "Pretty");
#endif

    font_info font;
    if (!collect_font(config->font_name, config->font_size, &font))
        goto quit;

    // tty_write(&tty, TEST_COMMAND, length_of(TEST_COMMAND));

    for (bool is_running = true; is_running;) {
        SDL_Event event;
#ifdef WAIT_EVENTS
        SDL_WaitEvent(&event);
#else
        for (; SDL_PollEvent(&event); ) {
#endif
            switch (event.type) {
                case SDL_EVENT_QUIT:
                    is_running = false;
                    break;

                case SDL_EVENT_WINDOW_RESIZED:
                    win_size.width = event.window.data1;
                    win_size.height = event.window.data2;
                    pretty_log(PRETTY_INFO, "Window resized: %dx%d",
                            win_size.width, win_size.height);
                    goto render_frame;
                    break;

                case SDL_EVENT_WINDOW_EXPOSED:
                    goto render_frame;
                    break;

                case SDL_EVENT_KEY_DOWN:
                    if (event.key.key <= UCHAR_MAX && isprint(event.key.key))
                        tty_write(&tty, (char *)&event.key.key, sizeof(char));
                    else if (event.key.key == SDLK_RETURN)
                        tty_write(&tty, "\r", length_of("\r"));
                    else if (event.key.key == SDLK_BACKSPACE)
                        tty_erase_last(&tty);
                    else
                        pretty_log(PRETTY_DEBUG, "unhandled key: %s", SDL_GetKeyName(event.key.key));
                    break;

                case SDL_EVENT_USER:
                    pthread_mutex_lock(&tty.lock);
                    if (tty.buff_consumed < tty.buff_len) {
                        size_t new_bytes = tty.buff_len - tty.buff_consumed;
                        pretty_log(PRETTY_INFO, "Processing %zu new bytes (consumed: %zu, total: %zu)",
                           new_bytes, tty.buff_consumed, tty.buff_len);

                        for (size_t i = 0; i < new_bytes; i++) {
                            char ch = tty.buff[tty.buff_consumed + i];

                            if (ch == '\b' || ch == 0x7f) {
                                if (buff_pos > 0) {
                                    buff_pos--;
                                    pretty_log(PRETTY_INFO, "Backspace: removed char at position %zu", buff_pos);
                                    buff[buff_pos] = '\0';
                                }
                            } else {
                                if (buff_pos < sizeof(buff) - 1) {
                                    buff[buff_pos++] = ch;
                                    buff[buff_pos] = '\0';
                                    pretty_log(PRETTY_INFO, "Added char '%c' at position %zu",
                                        (isprint(ch)) ? ch : '?', buff_pos - 1);
                                }
                            }
                        }

                        tty.buff_consumed = tty.buff_len;
                    }
                    pthread_mutex_unlock(&tty.lock);
                    goto render_frame;

render_frame:
                    if (!render_frame(renderer, win_size, buff, &font, config)) {
                        is_running = false;
                        continue;
                    }
                    break;

                default:
                    break;
            }
#ifndef WAIT_EVENTS
        }
#endif
        display_fps_metrics(win);
    }

quit:
    thread_handle_quit(&tty);
    SDL_DestroyWindow(win);
    SDL_DestroyRenderer(renderer);
    SDL_Quit();
    free(cat_config);

    return EXIT_SUCCESS;
}
