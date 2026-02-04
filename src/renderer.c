#include <stdio.h>
#include <stdlib.h>

#include "SDL3/SDL_render.h"
#include "SDL3_ttf/SDL_ttf.h"
#include "macro_utils.h"
#include "renderer.h"
#include "slave.h"
#include "log.h"
#include "terminal.h"

void display_fps_metrics(SDL_Window *win, const SDL_DisplayMode *mode)
{
    static unsigned int frame_count = 0;
    static uint64_t last_time = 0;

    frame_count++;
    uint64_t current_time = SDL_GetTicks();
    uint64_t elapsed = current_time - last_time;

    if (elapsed >= 1000) {
        float actual_fps = (float)frame_count / (elapsed / 1000.0f);

        char title[64];
        snprintf(title, sizeof(title), "Pretty | %.1f/%.0f fps", 
                 actual_fps, mode->refresh_rate);
        SDL_SetWindowTitle(win, title);

        last_time = current_time;
        frame_count = 0;
    }
}

glyph_atlas *create_atlas(SDL_Renderer *renderer, TTF_Font *font, generic_config *conf)
{
    glyph_atlas *atlas = malloc(sizeof *atlas);

    if (atlas == NULL)
        return NULL;

    int minx, maxx, miny, maxy, advance;
    TTF_GetGlyphMetrics(font, 'M', &minx, &maxx, &miny, &maxy, &advance);
    atlas->w = advance;
    atlas->h = TTF_GetFontHeight(font);

    int atlas_w = atlas->w * 16;
    int atlas_h = atlas->h * 8;
    atlas->texture = SDL_CreateTexture(
        renderer, SDL_PIXELFORMAT_RGBA8888,
        SDL_TEXTUREACCESS_TARGET, atlas_w, atlas_h);

    SDL_SetTextureBlendMode(atlas->texture, SDL_BLENDMODE_BLEND);
    SDL_SetRenderTarget(renderer, atlas->texture);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
    SDL_RenderClear(renderer);


    SDL_Color text_color = { HEX_TO_RGB(conf->color_palette[15]), .a=255 };
    SDL_Color bg_color = { HEX_TO_RGB(conf->color_palette[COLOR_BACKGROUND]), .a=255 };

    for (int i = ' '; i <= '~'; i++) {
        SDL_Surface *s = TTF_RenderGlyph_LCD(font, i, text_color, bg_color);
        if (!s) continue;

        SDL_Texture *t = SDL_CreateTextureFromSurface(renderer, s);

        int col = i % 16;
        int row = i / 16;

        SDL_FRect dst = {
            (float)(col * atlas->w),
            (float)(row * atlas->h),
            (float)s->w,
            (float)s->h
        };

        SDL_RenderTexture(renderer, t, NULL, &dst);

        // Save the source rect for later
        atlas->glyphs[i] = dst;

        SDL_DestroySurface(s);
        SDL_DestroyTexture(t);
    }
    int total_glyphs = 0;
    for (int i = ' '; i <= '~'; i++) {
        if (atlas->glyphs[i].w > 0) total_glyphs++;
    }

    pretty_log(PRETTY_INFO, "Atlas baked: %d glyphs. Cell size: %dx%d", 
           total_glyphs, atlas->w, atlas->h);

    SDL_SetRenderTarget(renderer, NULL);
    return atlas;
}

void grid_resize(Grid *grid, int new_rows, int new_cols) {
    Cell *new_cells = calloc(new_rows * new_cols, sizeof(Cell));
    if (!new_cells) return;

    int copy_rows = (new_rows < grid->rows) ? new_rows : grid->rows;
    int copy_cols = (new_cols < grid->cols) ? new_cols : grid->cols;

    for (int r = 0; r < copy_rows; r++) {
        Cell *old_row_start = &grid->cells[r * grid->cols];
        Cell *new_row_start = &new_cells[r * new_cols];
        memcpy(new_row_start, old_row_start, copy_cols * sizeof(Cell));
    }

    free(grid->cells);
    grid->cells = new_cells;
    grid->rows = new_rows;
    grid->cols = new_cols;
}


void rebuild_grid_from_buffer(Grid *grid, term *pretty, char *ring_buffer, size_t ring_size) {
    memset(grid->cells, 0, grid->rows * grid->cols * sizeof(Cell));

    int row = 0;
    int col = 0;
    size_t pos = pretty->scroll_tail;

    while (pos != pretty->head && row < grid->rows) {
        char ch = ring_buffer[pos];

        if (ch == '\n') {
            row++;
            col = 0;
        } else if (ch == '\r') {
            col = 0;
        } else if (ch == '\b' || ch == 0x7f) {
            if (col > 0) {
                col--;
                grid->cells[row * grid->cols + col].ch = ' ';
            } else if (row > 0) {
                row--;
                col = grid->cols - 1;
                grid->cells[row * grid->cols + col].ch = ' ';
            }
        } else if (ch >= ' ' && ch <= '~') {
            if (col >= grid->cols) {
                row++;
                col = 0;
                if (row >= grid->rows) break;
            }

            grid->cells[row * grid->cols + col].ch = ch;
            col++;
        }
        pos = (pos + 1) % ring_size;
    }

    pretty->cursor_row = row;
    pretty->cursor_col = col;
}

bool render_frame(
    SDL_Renderer *renderer,
    glyph_atlas *atlas,
    Grid *grid,
    generic_config *conf)
{
    SDL_Color bg = {
        HEX_TO_RGB(conf->color_palette[COLOR_BACKGROUND]),
        .a = 255
    };

    SDL_SetRenderDrawColor(renderer, bg.r, bg.g, bg.b, bg.a);
    SDL_RenderClear(renderer);

    for (int row = 0; row < grid->rows; row++) {
        for (int col = 0; col < grid->cols; col++) {
            Cell *c = &grid->cells[row * grid->cols + col];

            if (c->ch == '\0' || c->ch == ' ') continue;

            SDL_FRect src = atlas->glyphs[(unsigned char)c->ch];
            SDL_FRect dst = {
                .x = col * atlas->w,
                .y = row * atlas->h,
                .w = atlas->w,
                .h = atlas->h
            };

            SDL_RenderTexture(renderer, atlas->texture, &src, &dst);
        }
    }

    SDL_RenderPresent(renderer);
    return true;
}
