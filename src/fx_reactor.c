#include "effects.h"
#include "fast_math.h"
#include <string.h>
#include <math.h>

#define TOKAMAK_COILS 16
#define PLASMA_HELIX_PTS 64

static float anim_time = 0.0f;

static void reactor_init(void) {
    anim_time = 0.0f;
}

static void reactor_cleanup(void) {
}

static void reactor_update(float dt) {
    anim_time += dt;
}

static void reactor_render(uint16_t *fb) {
    memset(fb, 0, DISP_W * DISP_H * sizeof(uint16_t));
    int cx = DISP_W / 2;
    int cy = DISP_H / 2;

    float r_major = 68.0f;
    float r_minor = 22.0f;

    // 1. 外部保护壳多重刻度环
    display_draw_circle(fb, cx, cy, 108, rgb565(0, 60, 90));
    display_draw_circle(fb, cx, cy, 104, rgb565(0, 140, 180));
    display_draw_circle(fb, cx, cy, 38, rgb565(0, 80, 120));

    // 2. 16 组超导磁体外壳线圈 (D-Shape Magnetic Coils)
    float coil_rot = anim_time * 0.45f;
    for (int i = 0; i < TOKAMAK_COILS; i++) {
        float a = coil_rot + (float)i * (2.0f * FAST_PI / (float)TOKAMAK_COILS);
        float cos_a = cosf(a), sin_a = sinf(a);

        int in_x = cx + (int)((r_major - r_minor) * cos_a);
        int in_y = cy + (int)((r_major - r_minor) * sin_a);
        int out_x = cx + (int)((r_major + r_minor) * cos_a);
        int out_y = cy + (int)((r_major + r_minor) * sin_a);

        display_draw_line(fb, in_x, in_y, out_x, out_y, (i % 2 == 0) ? rgb565(0, 220, 255) : rgb565(0, 90, 130));
        display_fill_circle(fb, out_x, out_y, 2, rgb565(0, 180, 240));
    }

    // 3. 反应腔内部高速螺旋扭曲的等离子体流光束 (Helical Plasma Flux)
    int last_px = 0, last_py = 0;
    for (int p = 0; p < PLASMA_HELIX_PTS; p++) {
        float t = (float)p * (2.0f * FAST_PI / (float)PLASMA_HELIX_PTS);
        float phi = t + anim_time * 1.8f;
        float theta = t * 6.0f + anim_time * 6.5f; // 快速自旋螺距

        float cur_r = r_major + r_minor * 0.65f * cosf(theta);
        int px = cx + (int)(cur_r * cosf(phi));
        int py = cy + (int)(cur_r * sinf(phi));

        if (p > 0) {
            uint8_t hue = (uint8_t)((int)(anim_time * 40.0f + p * 6) & 0xFF);
            uint16_t col = hsv2rgb565(hue, 240, 255);
            display_draw_line(fb, last_px, last_py, px, py, col);
        }
        last_px = px;
        last_py = py;
    }

    // 4. 中央高温聚变等离子体核心 (白热光球与磁约束射线)
    float pulse = sinf(anim_time * 7.0f);
    int core_r = (int)(18.0f + pulse * 3.5f);
    display_fill_circle(fb, cx, cy, core_r + 4, rgb565(0, 70, 130));
    display_fill_circle(fb, cx, cy, core_r, rgb565(0, 210, 255));
    display_fill_circle(fb, cx, cy, core_r / 2, COLOR_WHITE);

    // 4 根激光点火导轨
    display_draw_line(fb, cx - 35, cy, cx + 35, cy, rgb565(255, 60, 140));
    display_draw_line(fb, cx, cy - 35, cx, cy + 35, rgb565(255, 60, 140));
}

Effect fx_reactor = {
    .id = "reactor",
    .title = "赛博方舟反应堆",
    .subtitle = "托卡马克聚变超导磁体与螺旋等离子流",
    .init = reactor_init,
    .update = reactor_update,
    .render = reactor_render,
    .cleanup = reactor_cleanup
};
