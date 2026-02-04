#include <string.h>

#include "terminal.h"
#include "renderer.h"
#include "log.h"

static
inline size_t ring_count(const term *pretty)
{
    return (pretty->head >= pretty->tail)
        ? (pretty->head - pretty->tail)
        : (TTY_RING_CAP - (pretty->tail - pretty->head));
}

static
size_t ring_write(term *pretty, const char *src, size_t nbytes)
{
    size_t space = TTY_RING_CAP - 1 - ring_count(pretty);

    if (nbytes > space) {
        // drop excess
        if (!pretty->overwrite_oldest) nbytes = space;

        // advance tail to free necessary bytes
        else {
            size_t fneed = nbytes - space;
            pretty->tail = (pretty->tail + fneed) % TTY_RING_CAP;
        };
    }

    // split copy across end if needed
    size_t first = nbytes;
    size_t end_space = TTY_RING_CAP - pretty->head;

    if (first > end_space) first = end_space;

    memcpy(pretty->buff + pretty->head, src, first);
    memcpy(pretty->buff, src + first, nbytes - first);

    pretty->last_head = pretty->head;
    pretty->head = (pretty->head + nbytes) % TTY_RING_CAP;

    return nbytes;
}

size_t ring_read_span(const term *pretty, const char **ptr)
{
    size_t cont = ring_count(pretty);
    if (!cont) { *ptr = NULL; return 0; }

    size_t end_contig = (pretty->head >= pretty->tail)
        ? (pretty->head - pretty->tail)
        : (TTY_RING_CAP - pretty->tail);

    *ptr = pretty->buff + pretty->tail;
    return end_contig;
}

void ring_consume(term *pretty, size_t k)
{
    size_t cont = ring_count(pretty);
    if (k > cont) k = cont;

    pretty->tail = (pretty->tail + k) % TTY_RING_CAP;
}

void ring_update(term *pretty, const char *src, size_t nbytes)
{

    size_t wrote = ring_write(pretty, src, nbytes);

    if (wrote < (size_t)nbytes)
        pretty_log(PRETTY_DEBUG, "Ring %s: replaced %zu oldest bytes",
                   pretty->overwrite_oldest ? "Overwrite" : "Dropped",
                   nbytes - wrote);

    if (!pretty->buff_changed) pretty->buff_changed = true;
}

void calculate_scroll_internal(term *pretty, enum event dir)
{
    switch (dir) {
        case SCROLL_UP: {
            if (pretty->scroll_tail == 0) break;

            size_t pos = (pretty->scroll_tail + TTY_RING_CAP - 1) % TTY_RING_CAP;

            while (pos != 0 && pretty->buff[pos] != '\n')
                pos = (pos + TTY_RING_CAP - 1) % TTY_RING_CAP;

            if (pretty->buff[pos] == '\n' && pos != 0) {
                pos = (pos + TTY_RING_CAP - 1) % TTY_RING_CAP;

                while (pos != 0 && pretty->buff[pos] != '\n')
                    pos = (pos + TTY_RING_CAP - 1) % TTY_RING_CAP;

                if (pretty->buff[pos] == '\n') pos = (pos + 1) % TTY_RING_CAP;
            }

            pretty->scroll_tail = pos;
            break;
        }
        case SCROLL_DOWN: {
            if (pretty->scroll_tail == pretty->head) break;

            size_t pos = pretty->scroll_tail;
            while (pos != pretty->head && pretty->buff[pos] != '\n') {
                pos++;
                if (pos >= TTY_RING_CAP) pos = 0;
            }

            if (pos != pretty->head && pretty->buff[pos] == '\n') {
                pos++;
                if (pos >= TTY_RING_CAP) pos = 0;
            }

            if (pos != pretty->head) pretty->scroll_tail = pos;
            break;
        }
        default:
            pretty_log(PRETTY_ERROR, "unhandled scroll event %d", dir);
            return;
    }
}

void calculate_scroll(term *pretty, enum event dir)
{
    pthread_mutex_lock(&pretty->buffer_lock);
    calculate_scroll_internal(pretty, dir);
    pthread_mutex_unlock(&pretty->buffer_lock);
}

static
void read_to_buff(
    term *pretty,
    char *buff,
    size_t buff_size,
    size_t *buff_pos)
{
    pthread_mutex_lock(&pretty->buffer_lock);

    const char *p;
    size_t new_bytes = ring_read_span(pretty, &p);

    if (new_bytes) {
        for (size_t i = 0; i < new_bytes; i++) {
            char ch = p[i];
            if (*buff_pos < buff_size - 1) {
                buff[(*buff_pos)++] = ch;
                buff[*buff_pos] = '\0';
            }

            if (*buff_pos >= buff_size - 1) *buff_pos = 0;
        }
        ring_consume(pretty, new_bytes);
    }
    pthread_mutex_unlock(&pretty->buffer_lock);
}

void process_buffer(term *pretty, Grid *grid, char *buff, size_t *buff_pos, size_t buff_size)
{
    size_t i = *buff_pos;
    read_to_buff(pretty, buff, buff_size, buff_pos);

    bool is_at_end = (pretty->scroll_tail == pretty->last_head);

    for (; i < *buff_pos; i++) {
        unsigned char ch = buff[i];

        if (ch == '\b' || ch == 0x7f) {
            if (pretty->cursor_col > 0) {
                pretty->cursor_col--;
            } else if (pretty->cursor_row > 0) {
                pretty->cursor_row--;
                pretty->cursor_col = grid->cols - 1;
            }

            Cell *cell = &grid->cells[pretty->cursor_row * grid->cols + pretty->cursor_col];
            cell->ch = ' ';
            continue;
        }

        if (ch == '\n') {
            pretty->cursor_row++;
            pretty->cursor_col = 0;
            continue;
        } else if (ch == '\r') {
            pretty->cursor_col = 0;
            continue;
        }

        if (pretty->cursor_col >= grid->cols) {
            pretty->cursor_col = 0;
            pretty->cursor_row++;
        }
        if (pretty->cursor_row >= grid->rows) {
           calculate_scroll(pretty, SCROLL_DOWN);
           pretty->cursor_row = grid->rows - 1;
        }

        Cell *cell = &grid->cells[pretty->cursor_row * grid->cols + pretty->cursor_col];
        cell->ch = (char)ch;

        pretty->cursor_col++;
    }

    *buff_pos = 0;

    if (!is_at_end) rebuild_grid_from_buffer(grid, pretty, pretty->buff, TTY_RING_CAP);
}
// TODO: What should go in here?
// struct pty_session *term_init()
// {
//
//     return term;
// }
