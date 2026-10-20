#include "effects.h"
#include "fast_math.h"
#include <string.h>
#include <math.h>

static float anim_time = 0.0f;
static float glitch_timer = 0.0f;
static bool is_glitching = false;
static int glitch_offset_x = 0;
static int glitch_band_y = 0;
static int glitch_band_h = 0;

static void glitch_init(void) {
    anim_time = 0.0f;
    glitch_timer = 0.8f;
    is_glitching = false;
}

static void glitch_cleanup(void) {
}

static void glitch_update(float dt) {
    anim_time += dt;
    glitch_timer -= dt;

    if (glitch_timer <= 0.0f) {
        if (!is_glitching) {
            is_glitching = true;
            glitch_timer = 0.08f + (float)(fast_rand() % 10) * 0.01f;
            glitch_offset_x = (fast_rand() % 31) - 15;
            glitch_band_y = fast_rand() % (DISP_H - 40);
            glitch_band_h = 15 + (fast_rand() % 35);
        } else {
            is_glitching = false;
            glitch_timer = 0.4f + (float)(fast_rand() % 120) * 0.01f;
        }
    }
}

static void glitch_render(uint16_t *fb) {
    memset(fb, 0, DISP_W * DISP_H * sizeof(uint16_t));
    int cx = DISP_W / 2;
    int cy = DISP_H / 2;

    int shift = is_glitching ? glitch_offset_x : 0;

    // 1. 背景细微网格
    for (int y = 0; y < DISP_H; y += 16) {
        for (int x = 0; x < DISP_W; x += 16) {
            display_draw_pixel(fb, x, y, rgb565(15, 25, 45));
        }
    }

    // 2. 居中赛博赛车式几何盾徽 / 赛博标志
    float rot = anim_time * 0.9f;
    for (int side = 3; side <= 6; side++) {
        float r = 24.0f + (float)(side - 3) * 22.0f;
        float poly_rot = rot * ((side % 2 == 0) ? 1.0f : -1.0f);

        for (int i = 0; i < side; i++) {
            float a1 = poly_rot + (float)i * (2.0f * FAST_PI / (float)side);
            float a2 = poly_rot + (float)(i + 1) * (2.0f * FAST_PI / (float)side);

            int x1 = cx + shift + (int)(r * cosf(a1));
            int y1 = cy + (int)(r * sinf(a1));
            int x2 = cx + shift + (int)(r * cosf(a2));
            int y2 = cy + (int)(r * sinf(a2));

            // RGB 分离色彩模拟
            if (is_glitching) {
                display_draw_line(fb, x1 - 3, y1, x2 - 3, y2, rgb565(255, 0, 80)); // 红色通道偏移
                display_draw_line(fb, x1 + 3, y1, x2 + 3, y2, rgb565(0, 240, 255)); // 青色通道偏移
            } else {
                display_draw_line(fb, x1, y1, x2, y2, rgb565(0, 220, 255));
            }
        }
    }

    // 3. 故障触发时画水平撕裂区块与杂色条
    if (is_glitching) {
        int by = glitch_band_y;
        int bh = glitch_band_h;
        for (int y = by; y < by + bh && y < DISP_H; y += 2) {
            int rx = fast_rand() % 50;
            int rw = 80 + (fast_rand() % 100);
            uint16_t g_col = (y % 4 == 0) ? rgb565(255, 20, 100) : rgb565(0, 255, 200);
            display_fill_rect(fb, rx, y, rw, 1, g_col);
        }

        // 随机白噪雪花点
        for (int i = 0; i < 60; i++) {
            int nx = fast_rand() % DISP_W;
            int ny = fast_rand() % DISP_H;
            display_draw_pixel(fb, nx, ny, COLOR_WHITE);
        }
    }

    // 4. 动态数据扫描线
    int sy = (int)(anim_time * 160.0f) % DISP_H;
    display_draw_line(fb, 0, sy, DISP_W - 1, sy, rgb565(80, 160, 255));
}

Effect fx_glitch = {
    .id = "glitch",
    .title = "赛博故障裂缝",
    .subtitle = "数字信号撕裂与RGB色差故障",
    .init = glitch_init,
    .update = glitch_update,
    .render = glitch_render,
    .cleanup = glitch_cleanup
};
