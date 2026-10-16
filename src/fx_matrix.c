#include "effects.h"
#include "fast_math.h"
#include "font.h"
#include <stdlib.h>
#include <string.h>

#define COLS 30
#define ROWS 30

typedef struct {
    float head_y;
    float speed;
    int length;
    bool active;
    int delay;
    char glyphs[ROWS];
} MatrixCol;

static MatrixCol cols[COLS];

static const char glyph_pool[] = 
    "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ!@#$%&*+-/<>~:;=";

static inline char random_glyph(void) {
    return glyph_pool[fast_rand() % (sizeof(glyph_pool) - 1)];
}

static void init_col(MatrixCol *col, bool initial) {
    col->active = true;
    col->speed = 0.45f + ((float)(fast_rand() % 100) / 100.0f) * 0.65f;
    col->length = 8 + (fast_rand() % 14);
    col->head_y = initial ? (float)(fast_rand() % (ROWS + 10)) - (float)col->length : 0.0f;
    col->delay = 0;
    for (int y = 0; y < ROWS; y++) {
        col->glyphs[y] = random_glyph();
    }
}

static void matrix_init(void) {
    for (int x = 0; x < COLS; x++) {
        if ((fast_rand() % 3) != 0) {
            init_col(&cols[x], true);
        } else {
            cols[x].active = false;
            cols[x].delay = fast_rand() % 30;
        }
    }
}

static void matrix_cleanup(void) {
}

static void matrix_update(float dt) {
    (void)dt;
    for (int x = 0; x < COLS; x++) {
        MatrixCol *col = &cols[x];
        if (!col->active) {
            col->delay--;
            if (col->delay <= 0) {
                init_col(col, false);
            }
            continue;
        }

        col->head_y += col->speed;

        // 随机字符突变
        if ((fast_rand() % 100) < 25) {
            int ry = fast_rand() % ROWS;
            col->glyphs[ry] = random_glyph();
        }

        if (col->head_y - (float)col->length > (float)ROWS) {
            col->active = false;
            col->delay = fast_rand() % 20;
        }
    }
}

static inline void draw_matrix_glyph(uint16_t *fb, int px, int py, char ch, uint16_t color) {
    if (ch < 32 || ch > 126) ch = ' ';
    const uint8_t *glyph = font8x8[ch - 32];
    for (int r = 0; r < 8; r++) {
        uint8_t line = glyph[r];
        if (!line) continue;
        uint16_t *dst = fb + (py + r) * DISP_W + px;
        if (line & 0x80) dst[0] = color;
        if (line & 0x40) dst[1] = color;
        if (line & 0x20) dst[2] = color;
        if (line & 0x10) dst[3] = color;
        if (line & 0x08) dst[4] = color;
        if (line & 0x04) dst[5] = color;
        if (line & 0x02) dst[6] = color;
        if (line & 0x01) dst[7] = color;
    }
}

static void matrix_render(uint16_t *fb) {
    memset(fb, 0, DISP_W * DISP_H * sizeof(uint16_t));

    for (int x = 0; x < COLS; x++) {
        MatrixCol *col = &cols[x];
        if (!col->active && col->delay > 0) continue;

        int head_int = (int)col->head_y;
        int tail_int = head_int - col->length;
        int px = x * 8;

        for (int y = tail_int; y <= head_int; y++) {
            if (y < 0 || y >= ROWS) continue;

            int py = y * 8;
            int dist = head_int - y;
            float factor = 1.0f - ((float)dist / (float)col->length);
            if (factor < 0.05f) factor = 0.05f;

            char ch = col->glyphs[y % ROWS];
            uint16_t color;

            if (dist == 0) {
                // 头部字符高亮炽白
                color = COLOR_WHITE;
            } else if (dist == 1) {
                // 紧随头部高亮浅绿
                color = rgb565(140, 255, 150);
            } else {
                // 尾迹深绿渐暗
                uint8_t g = (uint8_t)(220 * factor);
                uint8_t b = (uint8_t)(40 * factor);
                color = rgb565(0, g, b);
            }

            draw_matrix_glyph(fb, px, py, ch, color);
        }
    }
}

Effect fx_matrix = {
    .id = "matrix",
    .title = "» MATRIX DIGITAL RAIN «",
    .subtitle = "黑客帝国数字流光雨",
    .init = matrix_init,
    .update = matrix_update,
    .render = matrix_render,
    .cleanup = matrix_cleanup
};
