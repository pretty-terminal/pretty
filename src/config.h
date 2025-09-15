#ifndef CONFIG_H
#define CONFIG_H

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

    config_struct *return_config(char *cat_config);

#endif // CONFIG_HO

