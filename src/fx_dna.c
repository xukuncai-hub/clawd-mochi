#include "effects.h"
#include "fast_math.h"
#include <string.h>
#include <math.h>

#define DNA_NODES 28

static float anim_time = 0.0f;
static float rot_angle = 0.0f;

static void dna_init(void) {
    anim_time = 0.0f;
    rot_angle = 0.0f;
}

static void dna_cleanup(void) {
}

static void dna_update(float dt) {
    anim_time += dt;
    rot_angle += dt * 1.6f;
}

static void dna_render(uint16_t *fb) {
    memset(fb, 0, DISP_W * DISP_H * sizeof(uint16_t));
    int cx = DISP_W / 2;

    static int ax[DNA_NODES], ay[DNA_NODES];
    static float az[DNA_NODES];
    static int bx[DNA_NODES], by[DNA_NODES];
    static float bz[DNA_NODES];

    float y_spacing = (float)(DISP_H + 20) / (float)(DNA_NODES - 1);
    float helix_r = 54.0f;

    // 1. 计算三维连续双螺旋骨架节点
    for (int i = 0; i < DNA_NODES; i++) {
        float y_pos = -10.0f + (float)i * y_spacing;
        float phase = (float)i * 0.32f + rot_angle;

        float x1 = helix_r * cosf(phase);
        float z1 = helix_r * sinf(phase);

        float x2 = helix_r * cosf(phase + FAST_PI);
        float z2 = helix_r * sinf(phase + FAST_PI);

        // 弱透视
        float fov = 180.0f;
        float scale1 = fov / (fov + z1);
        float scale2 = fov / (fov + z2);

        ax[i] = cx + (int)(x1 * scale1);
        ay[i] = (int)y_pos;
        az[i] = z1;

        bx[i] = cx + (int)(x2 * scale2);
        by[i] = (int)y_pos;
        bz[i] = z2;
    }

    // 2. 绘制碱基对高能连杆横梁 (Base Pairs)
    for (int i = 0; i < DNA_NODES; i++) {
        float avg_z = (az[i] + bz[i]) * 0.5f;
        uint16_t rung_col;
        int type = (i / 2) % 4;
        if (type == 0) rung_col = rgb565(0, 240, 255);      // 腺嘌呤 A - 青
        else if (type == 1) rung_col = rgb565(255, 50, 140); // 胸腺嘧啶 T - 粉
        else if (type == 2) rung_col = rgb565(255, 230, 40); // 鸟嘌呤 G - 金
        else rung_col = rgb565(0, 255, 130);                // 胞嘧啶 C - 翠绿

        display_draw_line(fb, ax[i], ay[i], bx[i], by[i], rung_col);

        // 核心氢键连接点
        int mid_x = (ax[i] + bx[i]) / 2;
        int mid_y = (ay[i] + by[i]) / 2;
        display_fill_circle(fb, mid_x, mid_y, 2, COLOR_WHITE);
    }

    // 3. 绘制两条连续流光骨架带 (Strand A & Strand B)
    for (int i = 0; i < DNA_NODES - 1; i++) {
        // 链 A
        uint16_t col_a = (az[i] > 0) ? rgb565(0, 240, 255) : rgb565(0, 70, 110);
        display_draw_line(fb, ax[i], ay[i], ax[i + 1], ay[i + 1], col_a);
        display_draw_line(fb, ax[i] + 1, ay[i], ax[i + 1] + 1, ay[i + 1], col_a);

        // 链 B
        uint16_t col_b = (bz[i] > 0) ? rgb565(255, 60, 180) : rgb565(90, 10, 60);
        display_draw_line(fb, bx[i], by[i], bx[i + 1], by[i + 1], col_b);
        display_draw_line(fb, bx[i] + 1, by[i], bx[i + 1] + 1, by[i + 1], col_b);

        // 正面高亮磷酸基团圆球
        if (az[i] > 10.0f) {
            display_fill_circle(fb, ax[i], ay[i], 3, COLOR_WHITE);
        }
        if (bz[i] > 10.0f) {
            display_fill_circle(fb, bx[i], by[i], 3, COLOR_WHITE);
        }
    }
}

Effect fx_dna = {
    .id = "dna",
    .title = "3D全息立体双螺旋DNA",
    .subtitle = "连续空间编织骨架带与碱基对光桥",
    .init = dna_init,
    .update = dna_update,
    .render = dna_render,
    .cleanup = dna_cleanup
};
