#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fontconfig/fontconfig.h>

#include "font.h"
#include "log.h"

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
        pretty_log(PRETTY_ERROR, "Font not found: %s", font_name);
        goto failure;
    }

    if (FcPatternGetString(matched, FC_FILE, 0, &file) != FcResultMatch) {
        pretty_log(PRETTY_ERROR, "Failed to get font file path for: %s", font_name);
        goto failure;
    }

    out = strdup((char const *)file);
failure:
    FcPatternDestroy(pattern);
    FcPatternDestroy(matched);
    return out;
}

bool collect_font(char const *name, size_t size, font_info *font)
{
    if (!TTF_Init()) {
        pretty_log(PRETTY_ERROR,
            "SDL_ttf could not initialize! TTF_Error: %s", SDL_GetError());
        return false;
    }

    pretty_log(PRETTY_INFO, "font name: [%s]", name);
    char *font_path = find_font_path_from_fc_name(name);
    pretty_log(PRETTY_INFO, "font path: [%s]", font_path);
    font->ttf = TTF_OpenFont(font_path, size);

    if (font == NULL) {
        pretty_log(PRETTY_ERROR, "Failed to load font: %s", SDL_GetError());
        return EXIT_FAILURE;
    }

    free(font_path);
    TTF_SetFontHinting(font->ttf, TTF_HINTING_MONO);
    font->line_skip = TTF_GetFontLineSkip(font->ttf);

    TTF_GetGlyphMetrics(font->ttf, '~', NULL, NULL, NULL, NULL, &font->advance);

    bool mono = true;
    for (char c = ' '; c < '~'; c++) {
        int advance;

        TTF_GetGlyphMetrics(font->ttf, c, NULL, NULL, NULL, NULL, &advance);
        mono &= advance == font->advance;
        if (!mono) {
            pretty_log(PRETTY_ERROR,
                "\033[31mWarning! Your font is not monospace."
                "This will cause rendering issues!\033[0m");
            break;
        }
    }
    return true;
}
