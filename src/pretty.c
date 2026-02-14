#include <SDL3/SDL_rect.h>
#include <SDL3/SDL_timer.h>
#include <getopt.h>
#include <limits.h>
#include <pthread.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/eventfd.h>
#include <sys/ioctl.h>

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
#include "terminal.h"

#define SCREEN_WIDTH 1280
#define SCREEN_HEIGHT 720

static struct option LONG_OPTIONS[] = {
    {"config", required_argument, 0, 'c'},
    {0,        0,                 0,  0 }
};

static void thread_handle_quit(pty_session *pty);

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

    term pretty = {
        .pty = {
            .io_lock = PTHREAD_MUTEX_INITIALIZER,
            .child_exited = false,
            .wakeup_fd = eventfd(0, EFD_NONBLOCK)
        },
        .buffer_lock = PTHREAD_MUTEX_INITIALIZER,
        .buff_changed = false,
        .overwrite_oldest = true,
        .cursor_col = 0,
        .cursor_row = 0
    };

    if (!pty_pair(&pretty.pty)) die("failed to pair pty");

    if (!pty_init(&pretty.pty)) die("failed to initialize pty");

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

    SDL_DisplayID display = SDL_GetPrimaryDisplay();
    const SDL_DisplayMode *mode = SDL_GetCurrentDisplayMode(display);

    thread_args args = {
        .pretty = &pretty,
        .notify_interval = 1000 / mode->refresh_rate,
    };

    if (pthread_create(&pretty.pty.thread, NULL, pty_poll_loop, &args) != 0) return EXIT_FAILURE;

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
    if (atlas == NULL) {
        pretty_log(PRETTY_ERROR, "Failed to bake glyph atlas");
        goto quit;
    }

    int available_width = SCREEN_WIDTH - (2 * config->pad_x);
    int available_height = SCREEN_HEIGHT - (2 * config->pad_y);

    Grid grid;
    grid.rows = available_height / atlas->h;
    grid.cols = available_width / atlas->w;
    grid.cells = calloc(grid.rows * grid.cols, sizeof(Cell));

    SDL_StartTextInput(win);
    SDL_SetRenderVSync(renderer, 1);

    for (bool is_running = true; is_running;) {
        SDL_Event event;
        for (; SDL_PollEvent(&event); ) {
            switch (event.type) {
                case SDL_EVENT_QUIT:
                    is_running = false;
                    break;
                case SDL_EVENT_WINDOW_RESIZED:
                    win_size.width = event.window.data1;
                    win_size.height = event.window.data2;

                    available_width = win_size.width - (2 * config->pad_x);
                    available_height = win_size.height - (2 * config->pad_y);

                    int new_cols = available_width / atlas->w;
                    int new_rows = available_height / atlas->h;

                    grid_resize(&grid, new_rows, new_cols);

                    if (pretty.cursor_col >= grid.cols) pretty.cursor_col = grid.cols - 1;
                    if (pretty.cursor_row >= grid.rows) pretty.cursor_row = grid.rows - 1;

                    break;
                case SDL_EVENT_WINDOW_EXPOSED:
                    break;
                case SDL_EVENT_TEXT_INPUT: {
                    const char *text = event.text.text;
                    unsigned char ch = (unsigned char)text[0];

                    if (ch > 0 && ch < 0x80) pty_write(&pretty.pty, (char *)&ch, sizeof(char));
                    break;
                }
                case SDL_EVENT_KEY_DOWN: {
                    SDL_Keymod mod = SDL_GetModState();

                    if (mod & SDL_KMOD_LCTRL) switch (event.key.key) {
                        case SDLK_C:
                            pty_write(&pretty.pty, "\x03", 1);
                            break;
                        case SDLK_D:
                            pty_write(&pretty.pty, "\x04", 1);
                            break;
                        case SDLK_Z:
                            pty_write(&pretty.pty, "\x1A", 1);
                            break;
                        default:
                            pretty_log(PRETTY_DEBUG, "unhandled key combination: LCtrl+%s",
                                    SDL_GetKeyName(event.key.key));
                            break;
                    }

                    else if (event.key.key == SDLK_RETURN)
                        pty_write(&pretty.pty, "\r", length_of("\r"));

                    else if (event.key.key == SDLK_BACKSPACE)
                        pty_write(&pretty.pty, "\x7f", 1);
                    break;
                }
                case SDL_EVENT_MOUSE_WHEEL:
                    if (event.wheel.y > 0) calculate_scroll(&pretty, SCROLL_UP);
                    else if (event.wheel.y < 0) calculate_scroll(&pretty, SCROLL_DOWN);

                    rebuild_grid_from_buffer(&grid, &pretty, pretty.buff, TTY_RING_CAP);
                    break;
                case SDL_EVENT_USER:
                    process_buffer(&pretty, &grid, buff, &buff_pos, sizeof(buff));
                    break;
                default:
                    break;
            }
        }

        if (!render_frame(renderer, atlas, &grid, config))
            is_running = false;

        display_fps_metrics(win, mode);

        if (pretty.pty.child_exited) {
            is_running = false;
            pretty.pty.should_exit = true;
        }
    }

    SDL_DestroyTexture(atlas->texture);
    free(atlas);

quit:
    thread_handle_quit(&pretty.pty);
    TTF_CloseFont(font.ttf);
    TTF_Quit();
    SDL_DestroyWindow(win);
    SDL_DestroyRenderer(renderer);
    SDL_Quit();
    free(cat_config);
    pretty_log(PRETTY_INFO, "Succesfully closed Pretty instance");

    return EXIT_SUCCESS;
}

static
void thread_handle_quit(pty_session *pty)
{
    pty->should_exit = true;
    pretty_log(PRETTY_INFO, "waiting for thread [%lu] to exit", pty->thread);

    uint64_t val = 1;
    if (write(pty->wakeup_fd, &val, sizeof(val)) != sizeof(val)) {
        pretty_log(PRETTY_ERROR, "writing to wakeup_fd failed, falling back to pthread_cancel");
        pthread_cancel(pty->thread);
    }

    void *res;
    int s = pthread_join(pty->thread, &res);

    if (s != 0) pretty_log(PRETTY_ERROR, "pthread_join failed");
    else pretty_log(PRETTY_INFO, "thread [%lu] exited cleanly", pty->thread);
}
