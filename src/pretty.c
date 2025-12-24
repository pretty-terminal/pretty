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

#include "SDL3/SDL_events.h"
#include "SDL3/SDL_keycode.h"
#include "SDL3_ttf/SDL_ttf.h"
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

    if (s != 0) pretty_log(PRETTY_ERROR, "pthread_join failed");
    else pretty_log(PRETTY_DEBUG, "thread [%lu] exited cleanly", tty->thread);
}

int main(int argc, char **argv)
{
    char *config_file = NULL;
    int option_index, c;

    while (true) {
        c = getopt_long(argc, argv, ":c:", LONG_OPTIONS, &option_index);

        if (c < 0) break;

        switch (c) {
            case 'c':
                config_file = optarg;
                break;
            case '?':
                break;
            default:
                pretty_log(PRETTY_INFO, "?? getopt returned character code 0%o ??", c);
                break;
        }
    }

    if (config_file == NULL) config_file = get_default_config_file();

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

    char buff[TTY_RING_CAP] = { 0 };
    size_t buff_pos = 0;

    tty_state tty = {
        .pty_master_fd = tty_new((char *[]){ "/bin/sh", NULL }),
        .buff_changed = false,
        .lock = PTHREAD_MUTEX_INITIALIZER,
        .overwrite_oldest = true,
        .child_exited = false
    };

    if (pthread_create(&tty.thread, NULL, tty_poll_loop, &tty) != 0) return EXIT_FAILURE;

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
            &win, &renderer))
    {
        pretty_log(PRETTY_ERROR, "Couldn't create window/renderer: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    SDL_SetRenderDrawColor(renderer,
        HEX_TO_RGBA(config->color_palette[COLOR_BACKGROUND]));
#ifdef WAIT_EVENTS
    SDL_SetWindowTitle(win, "Pretty");
#endif

    font_info font;
    if (!collect_font(config->font_name, config->font_size, &font)) {
        pretty_log(PRETTY_ERROR, "Failed to retrieve specified font");
        goto quit;
    }

    glyph_atlas *atlas = create_atlas(renderer, font.ttf, config);
    if (!atlas) {
        pretty_log(PRETTY_ERROR, "Failed to bake glyph atlas");
        goto quit;
    }

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
                case SDL_EVENT_KEY_DOWN: {
                    SDL_Keymod mod = SDL_GetModState();

                    if (mod & SDL_KMOD_LCTRL) switch (event.key.key) {
                        case SDLK_C:
                            tty_write(&tty, "\x03", 1);
                            break;
                        case SDLK_D:
                            tty_write(&tty, "\x04", 1);
                            break;
                        case SDLK_Z:
                            tty_write(&tty, "\x1A", 1);
                            break;
                        default:
                            pretty_log(PRETTY_DEBUG, "unhandled key combination: LCtrl+%s",
                                    SDL_GetKeyName(event.key.key));
                            break;
                    }

                    else if (event.key.key <= UCHAR_MAX && isprint(event.key.key))
                        tty_write(&tty, (char *)&event.key.key, sizeof(char));

                    else if (event.key.key == SDLK_RETURN)
                        tty_write(&tty, "\r", length_of("\r"));

                    else if (event.key.key == SDLK_BACKSPACE)
                        tty_write(&tty, "\x7f", 1);

                    else pretty_log(PRETTY_DEBUG, "unhandled key: %s", SDL_GetKeyName(event.key.key));
                    break;
                }

                case SDL_EVENT_MOUSE_WHEEL:
                    if (event.wheel.y > 0) calculate_scroll(&tty, SCROLL_UP);
                    else if (event.wheel.y < 0) calculate_scroll(&tty, SCROLL_DOWN);

                    read_to_buff(&tty, buff, sizeof(buff), &buff_pos);
                    goto render_frame;
                    break;
                case SDL_EVENT_USER:
                    read_to_buff(&tty, buff, sizeof(buff), &buff_pos);
                    goto render_frame;

render_frame:
                    if (!render_frame(renderer, atlas, win_size, &tty, buff,
                                sizeof(buff), &buff_pos, &font, config))
                    {
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

        if (tty.child_exited) {
            is_running = false;
            tty.should_exit = true;
        }
    }

    SDL_DestroyTexture(atlas->texture);
    free(atlas);

quit:
    thread_handle_quit(&tty);
    TTF_CloseFont(font.ttf);
    TTF_Quit();
    SDL_DestroyWindow(win);
    SDL_DestroyRenderer(renderer);
    SDL_Quit();
    free(cat_config);
    pretty_log(PRETTY_INFO, "Succesfully closed Pretty instance");

    return EXIT_SUCCESS;
}
