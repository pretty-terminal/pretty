#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "config.h"
#include "macro_utils.h"

static char CONFIG_PATH_SUFFIX[] = "/pretty/config.toml";

enum type {
    V_STRING,
    V_NUMBER,
    V_COLOR,
};

struct cval {
    char const *section_name;
    char const *key_name;
    enum type value;
    void *target;
};

static generic_config CONFIG = {
    .font_name = "Terminus",
    .font_size = 12,
    .win_padding = 12,
    .color_palette = {
        "000000FF",
        "AA0000FF",
        "00AA00FF",
        "AA5500FF",
        "0000AAFF",
        "AA00AAFF",
        "00AAAAFF",
        "AAAAAAFF",
        "555555FF",
        "FF5555FF",
        "55FF55FF",
        "FFFF55FF",
        "5555FFFF",
        "FF55FFFF",
        "55FFFFFF",
        "FFFFFFFF",
        [COLOR_BACKGROUND] = "000000FF"
    }
};

static struct cval CONFIG_VALIDATION[] = {
   { "font", "family", V_STRING, &CONFIG.font_name },
   { "font", "size", V_NUMBER, &CONFIG.font_size },
   { "window", "padding", V_NUMBER, &CONFIG.win_padding },
   { "palette", "background", V_COLOR, CONFIG.color_palette[COLOR_BACKGROUND] },
   { "palette", "color0", V_COLOR, CONFIG.color_palette[0] },
   { "palette", "color1", V_COLOR, CONFIG.color_palette[1] },
   { "palette", "color2", V_COLOR, CONFIG.color_palette[2] },
   { "palette", "color3", V_COLOR, CONFIG.color_palette[3] },
   { "palette", "color4", V_COLOR, CONFIG.color_palette[4] },
   { "palette", "color5", V_COLOR, CONFIG.color_palette[5] },
   { "palette", "color6", V_COLOR, CONFIG.color_palette[6] },
   { "palette", "color7", V_COLOR, CONFIG.color_palette[7] },
   { "palette", "color8", V_COLOR, CONFIG.color_palette[8] },
   { "palette", "color9", V_COLOR, CONFIG.color_palette[9] },
   { "palette", "color10", V_COLOR, CONFIG.color_palette[10] },
   { "palette", "color11", V_COLOR, CONFIG.color_palette[11] },
   { "palette", "color12", V_COLOR, CONFIG.color_palette[12] },
   { "palette", "color13", V_COLOR, CONFIG.color_palette[13] },
   { "palette", "color14", V_COLOR, CONFIG.color_palette[14] },
   { "palette", "color15", V_COLOR, CONFIG.color_palette[15] },
};

static
ssize_t path_add(char *out, ssize_t len, char *suffix)
{
    size_t suffix_len = strlen(suffix);

    if (len < 0)
        return -1;
    if ((len + suffix_len + 1) > PATH_MAX)
        return -1;
    memcpy(out + len, suffix, suffix_len + 1);
    return len + suffix_len;
}

char *get_default_config_file(void)
{
    static char lookup[PATH_MAX];
    size_t len;

    const char *xdg_home = getenv("XDG_CONFIG_HOME");
    if (xdg_home != NULL) {
        len = strlen(xdg_home);
        memcpy(lookup, xdg_home, len);
        path_add(lookup, len, CONFIG_PATH_SUFFIX);
        printf("Checking (XDG_CONFIG_HOME) [%s] for config\n", lookup);
        if (access(lookup, F_OK) == 0)
            return lookup;
    }

    const char *home = getenv("HOME");
    if (home != NULL) {
        len = strlen(home);
        memcpy(lookup, home, len);
        len = path_add(lookup, len, "/.config");
        path_add(lookup, len, CONFIG_PATH_SUFFIX);
        printf("Checking (HOME) [%s] for config\n", lookup);
        if (access(lookup, F_OK) == 0)
            return lookup;
    }

    return NULL;
}

char *parse_value(struct cval *p, char *s)
{
    unsigned long n;

    switch (p->value) {
        case V_STRING:
            if (*s != '\"')
                fprintf(stderr, "Missing start quote!\n");
            else s++;
            *(char **)p->target = s;
            s += strcspn(s, "\"\n");
            if (*s != '\"')
                fprintf(stderr, "Unterminated string!\n");
            *s = '\0';
            break;
        case V_NUMBER:
            n = strtoul(s, &s, 10);
            if (n > UCHAR_MAX) {
                fprintf(stderr, "Value for [%s].[%s] is not in range 0-255!\n",
                    p->section_name, p->key_name);
                break;
            }
            *(unsigned char *)p->target = n;
            break;
        case V_COLOR:
            if (*s != '\"')
                fprintf(stderr, "Missing start quote!\n");
            else s++;
            if (*s == '#')
                s++;
            n = 0;
            for (; isxdigit(s[n]); n++);
            if (n != 6 && n != 8) {
                fprintf(stderr,
                    "Color [%s].[%s] must be in rgb(a) hex format: %s\n",
                    p->section_name, p->key_name, s);
                break;
            }
            memcpy(p->target, s, n);
            s += n;
            if (*s != '\"')
                fprintf(stderr, "Missing end quote!\n");
            else s++;
            break;
    }
    return s;
}

static
char *parse_key_value(char const *section_name, char *s)
{
    char const *key = s;

    for (; isalnum(*s); s++);
    bool seen = *s == '=';
    *s++ = '\0';
    for (; isspace(*s); s++);
    if (*s != '=' && !seen) {
        fprintf(stderr, "Missing assignment operator\n");
        return s;
    }
    s++;
    for (; isspace(*s); s++);

    struct cval *p = NULL;
    for (size_t i = 0; i < length_of(CONFIG_VALIDATION); i++) {
        if (!strcmp(CONFIG_VALIDATION[i].section_name, section_name)
          && !strcmp(CONFIG_VALIDATION[i].key_name, key)) {
            p = &CONFIG_VALIDATION[i];
            break;
        }
    }
    if (p == NULL) {
        fprintf(stderr, "key [%s].[%s] is not a valid a setting\n",
            section_name, key);
        return s;
    }

    return parse_value(p, s);
}

generic_config *return_config(char *cat_config)
{
    // TODO: write a even better parser
    char const *section_name = "global";

    if (cat_config == NULL)
        cat_config = "";

    for (char *s = cat_config; *s != '\0'; s++) {
        if (*s == '[') {
            section_name = ++s;
            for (; isalpha(*s); s++);
            if (*s != ']')
                fprintf(stderr, "Unterminated config section!\n");
            *s++ = '\0';
            goto skip;
        }
        if (isalpha(*s)) {
            s = parse_key_value(section_name, s);
            goto skip;
        }
        if (*s == '#')
            goto skip;
        if (*s != '\n')
            fprintf(stderr, "<<%.10s>> Garbage at the end of the line", s);
skip:
        s += strcspn(s, "\n");
    }
    return &CONFIG;
}
