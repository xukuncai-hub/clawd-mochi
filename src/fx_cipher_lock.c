#include "effects.h"
#include "fast_math.h"
#include <string.h>
#include <math.h>

#define NUM_LOCK_RINGS 4

static float ring_rots[NUM_LOCK_RINGS];
static float anim_time = 0.0f;
static float unlock_pulse = 0.0f;

static void cipher_lock_init(void) {
    anim_time = 0.0f;
    unlock_pulse = 0.0f;
    for (int i = 0; i < NUM_LOCK_RINGS; i++) ring_rots[i] = 0.0f;
}

static void cipher_lock_cleanup(void) {
}

static void cipher_lock_update(float dt) {
    anim_time += dt;
    ring_rots[0] += dt * 0.75f;
    ring_rots[1] -= dt * 1.15f;
    ring_rots[2] += dt * 1.55f;
    ring_rots[3] -= dt * 0.85f;

    if (fmodf(anim_time, 3.8f) < 0.22f) {
        unlock_pulse = 1.0f;
    } else if (unlock_pulse > 0.0f) {
        unlock_pulse -= dt * 3.0f;
        if (unlock_pulse < 0.0f) unlock_pulse = 0.0f;
    }
}

static void draw_cog_ring(uint16_t *fb, int cx, int cy, float r, int teeth, float rot, uint16_t col) {
    float step = (2.0f * FAST_PI) / (float)teeth;
    for (int i = 0; i < teeth; i++) {
        if (i % 4 == 0) continue; // 密码缺口

        float a1 = rot + (float)i * step;
        float a2 = a1 + step * 0.75f;

        int x1 = cx + (int)(r * cosf(a1));
        int y1 = cy + (int)(r * sinf(a1));
        int x2 = cx + (int)(r * cosf(a2));
        int y2 = cy + (int)(r * sinf(a2));

        display_draw_line(fb, x1, y1, x2, y2, col);

        // 放射插销齿
        int tx = cx + (int)((r + 5.0f) * cosf(a1));
        int ty = cy + (int)((r + 5.0f) * sinf(a1));
        display_draw_line(fb, x1, y1, tx, ty, col);
    }
}

static void cipher_lock_render(uint16_t *fb) {
    memset(fb, 0, DISP_W * DISP_H * sizeof(uint16_t));
    int cx = DISP_W / 2;
    int cy = DISP_H / 2;

    uint16_t ring_c = (unlock_pulse > 0.1f) ? COLOR_WHITE : rgb565(0, 220, 255);

    // 1. 4 重同心差速密码转盘
    draw_cog_ring(fb, cx, cy, 102.0f, 32, ring_rots[0], ring_c);
    draw_cog_ring(fb, cx, cy, 80.0f,  24, ring_rots[1], rgb565(255, 60, 160));
    draw_cog_ring(fb, cx, cy, 58.0f,  16, ring_rots[2], rgb565(255, 230, 40));
    draw_cog_ring(fb, cx, cy, 38.0f,  12, ring_rots[3], rgb565(0, 255, 140));

    // 2. 轴向对齐检验激光刻度标尺 (四向对齐十字光刃)
    display_draw_line(fb, cx, 10, cx, 28, rgb565(255, 40, 50));
    display_draw_line(fb, cx, DISP_H - 28, cx, DISP_H - 10, rgb565(255, 40, 50));
    display_draw_line(fb, 10, cy, 28, cy, rgb565(255, 40, 50));
    display_draw_line(fb, DISP_W - 28, cy, DISP_W - 10, cy, rgb565(255, 40, 50));

    // 3. 核心量子解密芯片与全屏冲击光环
    int core_w = 16 + (int)(unlock_pulse * 12.0f);
    display_draw_box(fb, cx - core_w, cy - core_w, core_w * 2, core_w * 2, ring_c);
    display_fill_circle(fb, cx, cy, 4, (unlock_pulse > 0.1f) ? COLOR_WHITE : rgb565(0, 255, 220));

    if (unlock_pulse > 0.05f) {
        int r_shock = (int)((1.0f - unlock_pulse) * 115.0f);
        display_draw_circle(fb, cx, cy, r_shock, rgb565(255, 255, 100));
        display_draw_circle(fb, cx, cy, r_shock + 2, COLOR_WHITE);
    }
}

Effect fx_cipher_lock = {
    .id = "cipher_lock",
    .title = "赛博核心解密锁环",
    .subtitle = "星盘级四重同心差速密码盘对齐解锁",
    .init = cipher_lock_init,
    .update = cipher_lock_update,
    .render = cipher_lock_render,
    .cleanup = cipher_lock_cleanup
};
