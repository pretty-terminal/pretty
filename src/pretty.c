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
                printf("?? getopt returned character code 0%o ??\n", c);
        }
    }

    if (config_file == NULL)
        config_file = get_default_config_file();
    else if (access(config_file, F_OK) != 0) {
        fprintf(stderr, "Provided config file [%s] does not exists\n", config_file);
        config_file = get_default_config_file();
    }

    printf("Loading config from [%s]\n", config_file);
    char *cat_config = file_read(config_file);

    generic_config *config = return_config(cat_config);

    if (config == NULL) {
        fprintf(stderr, "Failed to get config!\n");
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
        SDL_Log("Couldn't initialize SDL: %s", SDL_GetError());
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
        SDL_Log("Couldn't create window/renderer: %s", SDL_GetError());
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

    tty_write(&tty, TEST_COMMAND, length_of(TEST_COMMAND));

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
                    SDL_Log("Window resized: %dx%d",
                            win_size.width, win_size.height);
                    goto render_frame;
                    break;

                case SDL_EVENT_WINDOW_EXPOSED:
                    goto render_frame;
                    break;

                case SDL_EVENT_USER:
                    pthread_mutex_lock(&tty.lock);
                    if (buff_pos + tty.buff_len < sizeof(buff) - 1) {
                        printf("Adding %zu bytes at position %zu\n", tty.buff_len, buff_pos);
                        printf("First few chars: [%.20s]\n", tty.buff);
                        memcpy(buff + buff_pos, tty.buff, tty.buff_len);
                        buff_pos += tty.buff_len;
                        buff[buff_pos] = '\0';
                    }
                    tty.buff_changed = false;
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
        if (tty.buff_changed)
            render_frame(renderer, win_size, buff, &font, config);
        display_fps_metrics(win);
    }

quit:
    SDL_DestroyWindow(win);
    SDL_DestroyRenderer(renderer);
    SDL_Quit();
    free(cat_config);
    return EXIT_SUCCESS;
}
