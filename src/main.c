#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_error.h>
#include <SDL3/SDL_pixels.h>
#include <SDL3/SDL_render.h>
#include <SDL3/SDL_video.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <fontconfig/fontconfig.h>

#include "macro_utils.h"

#define SCREEN_WIDTH 1280
#define SCREEN_HEIGHT 720

#define HEX_TO_INT(h_ascii) ((h_ascii & 0xf) + (9 * ((h_ascii >> 6) & 1)))

#define _BUILD_CHANNEL_VAL(cd, cu)

#define HEX_TO_RGB(hex)                             \
    (HEX_TO_INT(hex[0]) << 4) | HEX_TO_INT(hex[1]), \
    (HEX_TO_INT(hex[2]) << 4) | HEX_TO_INT(hex[3]), \
    (HEX_TO_INT(hex[4]) << 4) | HEX_TO_INT(hex[5])

// TODO: move to a config system:
static const uint8_t TRANSPARENCY_LEVEL = 242; // ~95%

static
void display_fps_metrics(SDL_Window *win)
{
    static unsigned short frames = 0;
    static Uint64 last_time = 0;

    frames++;
    Uint64 current_time = SDL_GetTicks();

    if (current_time - last_time >= 1000) {
        char title[length_of("Pretty | ..... fps")];

        snprintf(title, sizeof title, "Pretty | %5hu fps", frames);
        SDL_SetWindowTitle(win, title);
        last_time = current_time;
        frames = 0;
    }
}

static
char *find_font_path_from_fc_name(const char* font_name)
{
    char *out = NULL;

    if (!FcInit())
        return NULL;

    FcPattern *pattern = FcNameParse((const FcChar8 *)font_name);
    FcConfigSubstitute(NULL, pattern, FcMatchPattern);
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

int main(void)
{
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("Couldn't initialize SDL: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    SDL_Window *win;
    SDL_Renderer *renderer;

    if (!SDL_CreateWindowAndRenderer(
        "examples/renderer/clear",
        SCREEN_WIDTH, SCREEN_HEIGHT,
            SDL_WINDOW_RESIZABLE
            | SDL_WINDOW_HIGH_PIXEL_DENSITY
            | SDL_WINDOW_TRANSPARENT,
        &win, &renderer)
    ) {
        SDL_Log("Couldn't create window/renderer: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    if (!TTF_Init()) {
        fprintf(stderr,
            "SDL_ttf could not initialize! TTF_Error: %s\n", SDL_GetError());
        SDL_Quit();
        return EXIT_FAILURE;
    }

    char const *font_path = find_font_path_from_fc_name("JetbrainsMono Nerd Font");
    TTF_Font *font = TTF_OpenFont(font_path, 12);

    if (font == NULL) {
        fprintf(stderr, "Failed to load font: %s\n", SDL_GetError());
        return EXIT_FAILURE;
    }

    TTF_SetFontHinting(font, TTF_HINTING_MONO);

    char hello_world[] = "Hello World!";
    SDL_Color text_color = {HEX_TO_RGB("C1C9EC"), .a = 255};

    SDL_Surface *text_surf = TTF_RenderText_Blended(
        font, hello_world, length_of(hello_world) - 1, text_color);
    if (text_surf == NULL) {
        fprintf(stderr, "Failed to create text surface: %s\n", SDL_GetError());
        return EXIT_FAILURE;
    }

    SDL_Texture *text_texture =
        SDL_CreateTextureFromSurface(renderer, text_surf);
    if (text_texture == NULL) {
        fprintf(stderr, "Failed to create text texture: %s\n", SDL_GetError());
        return EXIT_FAILURE;
    }

    for (bool is_running = true; is_running;) {
        display_fps_metrics(win);

        SDL_RenderClear(renderer);

        SDL_FRect text_rect = {50, 50, text_surf->w, text_surf->h};
        SDL_RenderTexture(renderer, text_texture, NULL, &text_rect);

        SDL_RenderPresent(renderer);
        for (SDL_Event event; SDL_PollEvent(&event);) {
            SDL_SetRenderDrawColor(
                renderer, HEX_TO_RGB("1A1C31"), TRANSPARENCY_LEVEL);

            switch (event.type) {
            case SDL_EVENT_QUIT:
                is_running = false;
                break;

            case SDL_EVENT_WINDOW_RESIZED:
                SDL_Log("Window resized: %dx%d", event.window.data1,
                    event.window.data2);
                SDL_SetRenderViewport(renderer, NULL);
                break;

            default:
                break;
            }
        }
    }

    SDL_DestroyWindow(win);
    SDL_DestroyRenderer(renderer);
    SDL_Quit();

    return EXIT_SUCCESS;
}
