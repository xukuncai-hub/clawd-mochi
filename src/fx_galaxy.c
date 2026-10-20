#include "effects.h"
#include "fast_math.h"
#include <string.h>
#include <math.h>

#define GALAXY_ARMS 4
#define PTS_PER_ARM 32

static float anim_time = 0.0f;
static float gal_rot = 0.0f;

static void galaxy_init(void) {
    anim_time = 0.0f;
    gal_rot = 0.0f;
}

static void galaxy_cleanup(void) {
}

static void galaxy_update(float dt) {
    anim_time += dt;
    gal_rot += dt * 0.95f;
}

static void galaxy_render(uint16_t *fb) {
    memset(fb, 0, DISP_W * DISP_H * sizeof(uint16_t));
    int cx = DISP_W / 2;
    int cy = DISP_H / 2;

    float tilt = 0.52f; // 倾角透视
    float cos_tilt = cosf(tilt);

    // 1. 绘制 4 条绚烂的引力密度波对数螺旋臂 (Logarithmic Spiral Arms)
    for (int arm = 0; arm < GALAXY_ARMS; arm++) {
        float arm_phase = gal_rot + (float)arm * (2.0f * FAST_PI / (float)GALAXY_ARMS);

        int last_x = 0, last_y = 0;

        for (int p = 0; p < PTS_PER_ARM; p++) {
            float t = (float)(p + 1) / (float)PTS_PER_ARM;
            float rad = 14.0f + 96.0f * t;
            // 对数螺线展开角 (开普勒旋转剪切)
            float theta = arm_phase + t * 3.6f;

            int sx = cx + (int)(rad * cosf(theta));
            int sy = cy + (int)(rad * sinf(theta) * cos_tilt);

            if (p > 0) {
                uint8_t hue = (uint8_t)((int)(anim_time * 25.0f + arm * 40 + p * 5) & 0xFF);
                uint8_t val = (uint8_t)(255.0f * (1.0f - t * 0.4f));
                uint16_t col = hsv2rgb565(hue, 220, val);

                display_draw_line(fb, last_x, last_y, sx, sy, col);

                // 伴随的亮星团节点
                if (p % 3 == 0) {
                    display_draw_pixel(fb, sx, sy, COLOR_WHITE);
                    display_draw_pixel(fb, sx + 1, sy, col);
                }
            }
            last_x = sx;
            last_y = sy;
        }
    }

    // 2. 绘制星系盘面跨臂流光网格 (Cross-Arm Tidal Ribbons)
    for (int ring = 1; ring <= 5; ring++) {
        float r_ring = (float)ring * 20.0f;
        int last_rx = 0, last_ry = 0;
        int first_rx = 0, first_ry = 0;

        for (int s = 0; s <= 24; s++) {
            float a = (float)s * (2.0f * FAST_PI / 24.0f) + gal_rot * 0.5f;
            int rx = cx + (int)(r_ring * cosf(a));
            int ry = cy + (int)(r_ring * sinf(a) * cos_tilt);

            if (s == 0) {
                first_rx = rx;
                first_ry = ry;
            } else {
                display_draw_line(fb, last_rx, last_ry, rx, ry, rgb565(0, 50, 80));
            }
            last_rx = rx;
            last_ry = ry;
        }
        display_draw_line(fb, last_rx, last_ry, first_rx, first_ry, rgb565(0, 50, 80));
    }

    // 3. 核心超大质量银心核球 (Galactic Bulge 白热光辉与吸积光晕)
    float pulse = sinf(anim_time * 5.0f);
    int core_r = (int)(12.0f + pulse * 2.5f);
    display_fill_circle(fb, cx, cy, core_r + 4, rgb565(0, 160, 240));
    display_fill_circle(fb, cx, cy, core_r, rgb565(255, 220, 120));
    display_fill_circle(fb, cx, cy, core_r / 2, COLOR_WHITE);
}

Effect fx_galaxy = {
    .id = "galaxy",
    .title = "3D银河旋涡引力臂",
    .subtitle = "对数螺线引力密度波与超大质量星系盘",
    .init = galaxy_init,
    .update = galaxy_update,
    .render = galaxy_render,
    .cleanup = galaxy_cleanup
};
