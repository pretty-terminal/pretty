#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "config.h"
#include "macro_utils.h"

static char CONFIG_PATH_SUFFIX[] = "/pretty/pretty.conf";

char *get_default_config_file(void)
{
    static char config_lookup[PATH_MAX];

    const char *xdg_home = getenv("XDG_CONFIG_HOME");
    if (xdg_home != NULL) {
        size_t len = strlen(xdg_home);

        memcpy(config_lookup, xdg_home, len);
        memcpy(config_lookup + len,
            CONFIG_PATH_SUFFIX, length_of(CONFIG_PATH_SUFFIX));
        if (access(config_lookup, F_OK) == 0)
            return config_lookup;
    }

    const char *xdg_dirs = getenv("XDG_CONFIG_DIRS");

    if (xdg_dirs == NULL)
        xdg_dirs = "/etc/xdg";

    for (;;) {
        size_t len = strcspn(xdg_dirs, ":");
        if (len > (PATH_MAX - length_of(CONFIG_PATH_SUFFIX)))
            continue;

        memcpy(config_lookup, xdg_dirs, len);
        memcpy(config_lookup + len,
            CONFIG_PATH_SUFFIX, length_of(CONFIG_PATH_SUFFIX));
        if (access(config_lookup, F_OK) == 0)
            return config_lookup;

        xdg_dirs += len;
        if (*xdg_dirs == '\0')
            break;
        xdg_dirs++;
    }

    return NULL;
}

static
config_section *parse_config(char *cat_config, char **keys, int num_keys)
{
    int len_keys = num_keys;

    config_section *settings = malloc(len_keys * sizeof(config_section));

    if (settings == NULL) {
        fprintf(stderr, "settings: Failed to allocate memory");
        return NULL;
    }

    int i;

    for (i = 0; i < len_keys; i++) {
        char *key_pos = strstr(cat_config, keys[i]);
        settings[i].name = keys[i];
        settings[i].kvs.key = keys[i];

        if (!key_pos) {
            settings[i].kvs.value = NULL;
            continue;
        }

        int key_id = (int)(key_pos - cat_config);
        while (cat_config[key_id] && cat_config[key_id] != '=') key_id++;
        if (cat_config[key_id] != '=') {
            settings[i].kvs.value = NULL;
            continue;
        }
        key_id+=2;
        if (cat_config[key_id] == '"')
            key_id++;

        int start = key_id;
        while (cat_config[key_id] && (cat_config[key_id] != '\n'
            || cat_config[key_id] != '"'))
            key_id++;
        int len = key_id - start;

        settings[i].kvs.value = malloc(len + 1);
        if (settings[i].kvs.value) {
            memcpy(settings[i].kvs.value, &cat_config[start], len);
            settings[i].kvs.value[len] = '\0';
        }
    }

    settings->count = i;

    return settings;
}


config_struct *return_config(char *cat_config)
{
    config_struct *config = malloc(sizeof(config_struct));
    if (config == NULL) {
        fprintf(stderr, "config: Failed to allocate memory");
        return NULL;
    }

    char *font_keys[] = {"family", "size"};
    config->font = parse_config(cat_config, font_keys, length_of(font_keys));

    char *window[] = {"opacity", "padding"};
    config->window = parse_config(cat_config, window, length_of(window));

    char *pallete[] = {"background", "foreground"};
    config->pallete = parse_config(cat_config, pallete, length_of(pallete));

    return config;
}
