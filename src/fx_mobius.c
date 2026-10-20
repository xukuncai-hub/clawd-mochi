#include "effects.h"
#include "fast_math.h"
#include <string.h>
#include <math.h>

#define MOBIUS_U_SEGS 40
#define MOBIUS_V_SEGS 6

static float rot_x = 0.0f;
static float rot_y = 0.0f;
static float anim_time = 0.0f;

static void mobius_init(void) {
    rot_x = 0.4f;
    rot_y = 0.0f;
    anim_time = 0.0f;
}

static void mobius_cleanup(void) {
}

static void mobius_update(float dt) {
    anim_time += dt;
    rot_x += dt * 0.45f;
    rot_y += dt * 0.75f;
}

static void mobius_render(uint16_t *fb) {
    memset(fb, 0, DISP_W * DISP_H * sizeof(uint16_t));
    int cx = DISP_W / 2;
    int cy = DISP_H / 2;

    float R = 68.0f;
    float W = 28.0f;

    float cx_rot = cosf(rot_x), sx_rot = sinf(rot_x);
    float cy_rot = cosf(rot_y), sy_rot = sinf(rot_y);

    static int sx[MOBIUS_V_SEGS][MOBIUS_U_SEGS];
    static int sy[MOBIUS_V_SEGS][MOBIUS_U_SEGS];
    static float sz[MOBIUS_V_SEGS][MOBIUS_U_SEGS];

    // 计算三维莫比乌斯曲率网格
    for (int vi = 0; vi < MOBIUS_V_SEGS; vi++) {
        float v = -W + (float)vi * (2.0f * W / (float)(MOBIUS_V_SEGS - 1));

        for (int ui = 0; ui < MOBIUS_U_SEGS; ui++) {
            float u = (float)ui * (2.0f * FAST_PI / (float)MOBIUS_U_SEGS);
            float cos_u = cosf(u), sin_u = sinf(u);
            float cos_half = cosf(u * 0.5f), sin_half = sinf(u * 0.5f);

            float x0 = (R + v * cos_half) * cos_u;
            float y0 = (R + v * cos_half) * sin_u;
            float z0 = v * sin_half;

            // 绕 Y 轴
            float x1 = x0 * cy_rot + z0 * sy_rot;
            float y1 = y0;
            float z1 = -x0 * sy_rot + z0 * cy_rot;

            // 绕 X 轴
            float x2 = x1;
            float y2 = y1 * cx_rot - z1 * sx_rot;
            float z2 = y1 * sx_rot + z1 * cx_rot;

            // 透视投影
            float fov = 190.0f;
            float scale = fov / (fov + z2);

            sx[vi][ui] = cx + (int)(x2 * scale);
            sy[vi][ui] = cy + (int)(y2 * scale);
            sz[vi][ui] = z2;
        }
    }

    // 绘制莫比乌斯曲面网格：经线 (沿带延伸的连续环线)
    for (int vi = 0; vi < MOBIUS_V_SEGS; vi++) {
        for (int ui = 0; ui < MOBIUS_U_SEGS; ui++) {
            int next_u = (ui + 1) % MOBIUS_U_SEGS;
            float avg_z = (sz[vi][ui] + sz[vi][next_u]) * 0.5f;

            uint8_t hue = (uint8_t)((int)(anim_time * 28.0f + ui * 6 + vi * 16) & 0xFF);
            uint8_t val = (uint8_t)(130.0f + 125.0f * ((avg_z + 40.0f) / 80.0f));
            uint16_t col = hsv2rgb565(hue, 220, val);

            display_draw_line(fb, sx[vi][ui], sy[vi][ui], sx[vi][next_u], sy[vi][next_u], col);
        }
    }

    // 绘制莫比乌斯曲面网格：纬线 (横向肋骨切面)
    for (int ui = 0; ui < MOBIUS_U_SEGS; ui += 2) {
        for (int vi = 0; vi < MOBIUS_V_SEGS - 1; vi++) {
            uint16_t rib_col = (sz[vi][ui] > 0.0f) ? rgb565(0, 240, 255) : rgb565(0, 60, 100);
            display_draw_line(fb, sx[vi][ui], sy[vi][ui], sx[vi + 1][ui], sy[vi + 1][ui], rib_col);
        }
    }
}

Effect fx_mobius = {
    .id = "mobius",
    .title = "赛博莫比乌斯环",
    .subtitle = "3D单侧拓扑曲面高密度流光网格",
    .init = mobius_init,
    .update = mobius_update,
    .render = mobius_render,
    .cleanup = mobius_cleanup
};
