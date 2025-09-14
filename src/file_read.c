#include <fcntl.h>
#include <stddef.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

static
int file_read_n(char const *filepath, char *buff, size_t size)
{
    int fd = open(filepath, O_RDONLY);
    int rd;

    if (fd < 0)
        return fd;
    rd = read(fd, buff, size);
    for (; close(fd) < 0;);
    return rd;
}

char *file_read(char const *filepath)
{
    struct stat fi;
    char *content;

    if (stat(filepath, &fi) < 0)
        return NULL;
    content = malloc((fi.st_size + 1) * sizeof(char));
    if (content == NULL)
        return NULL;
    content[fi.st_size] = '\0';
    if (file_read_n(filepath, content, fi.st_size) == fi.st_size)
        return content;
    free(content);
    return NULL;
}
