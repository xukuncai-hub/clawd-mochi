#include "effects.h"
#include "fast_math.h"
#include <string.h>
#include <math.h>

#define TORUS_U_SEGS 28
#define TORUS_V_SEGS 14

static float A = 0.0f;
static float B = 0.0f;
static float anim_time = 0.0f;

static void donut_init(void) {
    A = 0.4f;
    B = 0.2f;
    anim_time = 0.0f;
}

static void donut_cleanup(void) {
}

static void donut_update(float dt) {
    anim_time += dt;
    A += dt * 0.95f;
    B += dt * 0.65f;
}

static void donut_render(uint16_t *fb) {
    memset(fb, 0, DISP_W * DISP_H * sizeof(uint16_t));
    int cx = DISP_W / 2;
    int cy = DISP_H / 2;

    float R = 60.0f; // 主环半径
    float r = 24.0f; // 管半径

    float cosA = cosf(A), sinA = sinf(A);
    float cosB = cosf(B), sinB = sinf(B);

    static int sx[TORUS_U_SEGS][TORUS_V_SEGS];
    static int sy[TORUS_U_SEGS][TORUS_V_SEGS];
    static float sz[TORUS_U_SEGS][TORUS_V_SEGS];

    // 1. 计算 3D 环面经纬网格顶点
    for (int u = 0; u < TORUS_U_SEGS; u++) {
        float phi = (float)u * (2.0f * FAST_PI / (float)TORUS_U_SEGS);
        float cos_phi = cosf(phi), sin_phi = sinf(phi);

        for (int v = 0; v < TORUS_V_SEGS; v++) {
            float theta = (float)v * (2.0f * FAST_PI / (float)TORUS_V_SEGS);
            float cos_th = cosf(theta), sin_th = sinf(theta);

            float circlex = R + r * cos_th;
            float circley = r * sin_th;

            // 绕 X 轴和 Z 轴旋转矩阵
            float x = circlex * (cosB * cos_phi + sinA * sinB * sin_phi) - circley * cosA * sinB;
            float y = circlex * (sinB * cos_phi - sinA * cosB * sin_phi) + circley * cosA * cosB;
            float z = cosA * circlex * sin_phi + circley * sinA;

            float fov = 175.0f;
            float scale = fov / (fov + z + 20.0f);

            sx[u][v] = cx + (int)(x * scale);
            sy[u][v] = cy + (int)(y * scale);
            sz[u][v] = z;
        }
    }

    // 2. 绘制环向经线 (围绕管身的同心圆环)
    for (int u = 0; u < TORUS_U_SEGS; u++) {
        for (int v = 0; v < TORUS_V_SEGS; v++) {
            int next_v = (v + 1) % TORUS_V_SEGS;
            float avg_z = (sz[u][v] + sz[u][next_v]) * 0.5f;

            uint8_t hue = (uint8_t)((int)(anim_time * 30.0f + u * 8 + v * 12) & 0xFF);
            uint8_t val = (uint8_t)(110.0f + 145.0f * ((avg_z + 45.0f) / 90.0f));
            uint16_t col = hsv2rgb565(hue, 230, val);

            display_draw_line(fb, sx[u][v], sy[u][v], sx[u][next_v], sy[u][next_v], col);
        }
    }

    // 3. 绘制纵向纬线 (环绕大圆的流光长线)
    for (int v = 0; v < TORUS_V_SEGS; v += 2) {
        for (int u = 0; u < TORUS_U_SEGS; u++) {
            int next_u = (u + 1) % TORUS_U_SEGS;
            float avg_z = (sz[u][v] + sz[next_u][v]) * 0.5f;
            if (avg_z > -10.0f) {
                uint16_t rib_col = (avg_z > 15.0f) ? COLOR_WHITE : rgb565(0, 240, 255);
                display_draw_line(fb, sx[u][v], sy[u][v], sx[next_u][v], sy[next_u][v], rib_col);
            }
        }
    }

    // 4. 环绕甜甜圈轨道的光子能量流星
    float orb_angle = anim_time * 2.8f;
    float orb_x = (R + 10.0f) * cosf(orb_angle);
    float orb_y = (R + 10.0f) * sinf(orb_angle);
    int px = cx + (int)(orb_x * cosB - orb_y * sinB);
    int py = cy + (int)(orb_x * sinB + orb_y * cosB);
    display_fill_circle(fb, px, py, 3, COLOR_WHITE);
    display_draw_circle(fb, px, py, 6, rgb565(255, 60, 180));
}

Effect fx_donut = {
    .id = "donut",
    .title = "3D全息旋转甜甜圈",
    .subtitle = "三维环面流光经纬网格与光子轨道",
    .init = donut_init,
    .update = donut_update,
    .render = donut_render,
    .cleanup = donut_cleanup
};
