#ifndef FONT_H
    #define FONT_H

    #include <SDL3_ttf/SDL_ttf.h>

typedef struct {
    TTF_Font *ttf;
    int advance;
    int line_skip;
} font_info;

char *find_font_path_from_fc_name(const char *font_name);
bool collect_font(char const *name, size_t size, font_info *font);

#endif // FONT_H
