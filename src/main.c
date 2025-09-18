#include <getopt.h>
#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_error.h>
#include <SDL3/SDL_pixels.h>
#include <SDL3/SDL_render.h>
#include <SDL3/SDL_video.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <fontconfig/fontconfig.h>

#include "config.h"
#include "macro_utils.h"
#include "pretty.h"

#define HEX_TO_INT(h_ascii) ((h_ascii & 0xf) + (9 * ((h_ascii >> 6) & 1)))

#define _BUILD_CHANNEL_VAL(cd, cu)

#define HEX_TO_RGB(hex)                             \
    (HEX_TO_INT(hex[0]) << 4) | HEX_TO_INT(hex[1]), \
    (HEX_TO_INT(hex[2]) << 4) | HEX_TO_INT(hex[3]), \
    (HEX_TO_INT(hex[4]) << 4) | HEX_TO_INT(hex[5])

#define HEX_TO_RGBA(hex) \
    HEX_TO_RGB(hex),     \
    (HEX_TO_INT(hex[6]) << 4) | HEX_TO_INT(hex[7])

#define SCREEN_WIDTH 1280
#define SCREEN_HEIGHT 720

struct dim {
    int height;
    int width;
};

typedef struct {
    char *dir;
    char *path;
} config_paths;

static
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

static
char *find_font_path_from_fc_name(const char *font_name)
{
    char *out = NULL;

    if (!FcInit())
        return NULL;

    FcPattern *pattern = FcNameParse((const FcChar8 *)font_name);
    FcConfigSubstitute(NULL, pattern, FcMatchPattern);
    FcPatternAddInteger(pattern, FC_WEIGHT, FC_WEIGHT_DEMIBOLD);
    FcDefaultSubstitute(pattern);

    FcResult result;
    FcPattern *matched = FcFontMatch(NULL, pattern, &result);
    FcChar8 *file = NULL;

    if (matched == NULL) {
        fprintf(stderr, "Font not found: %s\n", font_name);
        goto failure;
    }

    if (FcPatternGetString(matched, FC_FILE, 0, &file) != FcResultMatch) {
        fprintf(stderr, "Failed to get font file path for: %s\n", font_name);
        goto failure;
    }

    out = strdup((char const *)file);
failure:
    FcPatternDestroy(pattern);
    FcPatternDestroy(matched);
    return out;
}

typedef struct {
    SDL_Surface *surface;
    SDL_Texture *texture;
    size_t length;
} line_renderer;

/* Note: This does the job of TTF_RenderText_Blended_Wrapped,
 * without computing Wraplines and thus is faster.
 *
 * Sadly, we still need to create a bunch of surface/texture.
 * TODO: investigate
 **/
static bool render_frame(
    SDL_Renderer *renderer,
    struct dim win_size,
    const char *text,
    TTF_Font *font,
    generic_config *conf
) {
    SDL_RenderClear(renderer);

    SDL_Color text_color = { HEX_TO_RGB(conf->color_palette[15]), .a=255 };

    int advance;
    /* TODO: call it once for all */
    TTF_GetGlyphMetrics(font, 'M', NULL, NULL, NULL, NULL, &advance);

    int x = conf->win_padding;
    int y = conf->win_padding;

    if ((2 * conf->win_padding + advance) >= win_size.width) {
        /* We dont have room to render anything, so dont */
        SDL_RenderPresent(renderer);
        return true;
    }

    size_t line_max_width = (win_size.width - (2 * conf->win_padding)) / advance;
    char const *p = text;

    for (; *p != '\0';) {
        line_renderer line = { 0 };

        for (; *p != '\0' && *p != '\n';) {
            if (line.length == line_max_width)
                break;
            line.length++;
            p++;
        }

        line.surface = TTF_RenderText_Blended(
            font, text, line.length, text_color);
        if (line.surface == NULL)
            return false;

        line.texture = SDL_CreateTextureFromSurface(renderer, line.surface);
        if (line.texture == NULL)
            return false;

        SDL_DestroySurface(line.surface);

        /* TODO: font size is known */
        SDL_FRect dst = { x, y, line.length * advance, TTF_GetFontHeight(font) };
        SDL_RenderTexture(renderer, line.texture, NULL, &dst);
        SDL_DestroyTexture(line.texture);

        y += TTF_GetFontLineSkip(font);
        if (*p == '\n')
            p++;
        text = p;
    }

    SDL_RenderPresent(renderer);
    return true;
}

static
bool handle_event(SDL_Event *event, struct dim *win_size, bool *is_running)
{
    switch (event->type) {
        case SDL_EVENT_QUIT:
            *is_running = false;
            break;

        case SDL_EVENT_WINDOW_RESIZED:
            win_size->width = event->window.data1;
            win_size->height = event->window.data2;
            SDL_Log("Window resized: %dx%d",
                win_size->width, win_size->height);

            /* fallthrough */
        case SDL_EVENT_WINDOW_EXPOSED:
            return true;

        default:
            break;
    }
    return false;
}

static struct option LONG_OPTIONS[] = {
    {"config", required_argument, 0, 'c'},
    {0,        0,                 0,  0 }
};

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

    if (cat_config == NULL) {
        // TODO: fallback to default config!
        fprintf(stderr, "Failed to read config!\n");
        return EXIT_FAILURE;
    }

    generic_config *config = return_config(cat_config);

    if (config == NULL) {
        fprintf(stderr, "Failed to get config!\n");
        return EXIT_FAILURE;
    }

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

    if (!TTF_Init()) {
        fprintf(stderr,
            "SDL_ttf could not initialize! TTF_Error: %s\n", SDL_GetError());
        SDL_Quit();
        return EXIT_FAILURE;
    }

    char *font_path = find_font_path_from_fc_name(config->font_name);
    TTF_Font *font = TTF_OpenFont(font_path, config->font_size);

    if (font == NULL) {
        fprintf(stderr, "Failed to load font: %s\n", SDL_GetError());
        return EXIT_FAILURE;
    }

    free(font_path);
    TTF_SetFontHinting(font, TTF_HINTING_MONO);

    char const *buff = file_read("flake.nix");
    if (buff == NULL) {
        fprintf(stderr, "Failed to read the flake.nix file!\n");
        return EXIT_FAILURE;
    }

    SDL_GetWindowSize(win, &win_size.width, &win_size.height);

    if (!render_frame(renderer, win_size, buff, font, config))
        return false;

    for (bool is_running = true; is_running;) {
        SDL_Event event;
#ifdef WAIT_EVENTS
        SDL_WaitEvent(&event);
        if (handle_event(&event, &win_size, &is_running)
            && !render_frame(renderer, win_size, buff, font)
        )
            return false;
#else
        for (; SDL_PollEvent(&event); ) {
            if (handle_event(&event, &win_size, &is_running)) {
                if (!render_frame(renderer, win_size, buff, font, config))
                    return false;
                break;
            }
        }
        display_fps_metrics(win);
#endif
    }

    SDL_DestroyWindow(win);
    SDL_DestroyRenderer(renderer);
    SDL_Quit();
    free(cat_config);
    return EXIT_SUCCESS;
}
