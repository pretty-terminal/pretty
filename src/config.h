#ifndef CONFIG_H
    #define CONFIG_H

    #include "macro_utils.h"
    #include <SDL3/SDL_pixels.h>

typedef struct {
    char *dir;
    char *path;
} config_paths;

typedef struct {
    char *key;
    char *value;
} config_pair;

typedef struct {
    const char *name;
    config_pair kvs;
    unsigned int count;
} config_section;

typedef struct {
    config_section *font;
    config_section *pallete;
    config_section *window;
} config_struct;

enum {
  REVERSED_COLOR = 15,
  COLOR_BACKGROUND,
  COLOR_COUNT,
};

typedef struct {
    char *font_name;
    unsigned char font_size;
    unsigned char win_padding;
    char color_palette[COLOR_COUNT][length_of("rrggbbaa") + 1];
} generic_config;

generic_config *return_config(char *cat_config);
char *get_default_config_file(void);

#endif // CONFIG_H

