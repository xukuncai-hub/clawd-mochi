#include "effects.h"
#include "fast_math.h"
#include <string.h>
#include <math.h>

#define L_GRID_COLS 16
#define L_GRID_ROWS 16

static float anim_time = 0.0f;

static void laser_grid_init(void) {
    anim_time = 0.0f;
}

static void laser_grid_cleanup(void) {
}

static void laser_grid_update(float dt) {
    anim_time += dt;
}

static void laser_grid_render(uint16_t *fb) {
    memset(fb, 0, DISP_W * DISP_H * sizeof(uint16_t));
    int cx = DISP_W / 2;
    int cy = DISP_H / 2;

    static int px[L_GRID_ROWS][L_GRID_COLS];
    static int py[L_GRID_ROWS][L_GRID_COLS];
    static float pz[L_GRID_ROWS][L_GRID_COLS];

    // 透视俯仰变换 (3D 斜俯视角度)
    float pitch = 0.72f;
    float cos_p = cosf(pitch), sin_p = sinf(pitch);

    float col_w = (float)(DISP_W - 10) / (float)(L_GRID_COLS - 1);
    float row_h = (float)(DISP_H - 10) / (float)(L_GRID_ROWS - 1);

    for (int r = 0; r < L_GRID_ROWS; r++) {
        float y0 = ((float)r - (float)(L_GRID_ROWS - 1) * 0.5f) * row_h;

        for (int c = 0; c < L_GRID_COLS; c++) {
            float x0 = ((float)c - (float)(L_GRID_COLS - 1) * 0.5f) * col_w;

            // 李萨如高能激光空间波曲率高度
            float z0 = 24.0f * sinf(anim_time * 3.5f + (float)c * 0.45f)
                     * cosf(anim_time * 2.8f + (float)r * 0.45f);

            // 绕 X 轴俯仰旋转
            float y1 = y0 * cos_p - z0 * sin_p;
            float z1 = y0 * sin_p + z0 * cos_p;

            // 透视投影
            float fov = 170.0f;
            float scale = fov / (fov + z1);

            px[r][c] = cx + (int)(x0 * scale);
            py[r][c] = cy + (int)(y1 * scale);
            pz[r][c] = z0;
        }
    }

    // 绘制横向激光干涉线
    for (int r = 0; r < L_GRID_ROWS; r++) {
        uint8_t hue = (uint8_t)((int)(anim_time * 35.0f + r * 12) & 0xFF);
        uint16_t col = hsv2rgb565(hue, 240, 240);
        for (int c = 0; c < L_GRID_COLS - 1; c++) {
            display_draw_line(fb, px[r][c], py[r][c], px[r][c + 1], py[r][c + 1], col);
        }
    }

    // 绘制纵向激光干涉线
    for (int c = 0; c < L_GRID_COLS; c++) {
        uint8_t hue = (uint8_t)((int)(anim_time * 35.0f + c * 12 + 128) & 0xFF);
        uint16_t col = hsv2rgb565(hue, 240, 240);
        for (int r = 0; r < L_GRID_ROWS - 1; r++) {
            display_draw_line(fb, px[r][c], py[r][c], px[r + 1][c], py[r + 1][c], col);
        }
    }

    // 激光驻波高能交叉光斑
    for (int r = 0; r < L_GRID_ROWS; r += 2) {
        for (int c = 0; c < L_GRID_COLS; c += 2) {
            if (pz[r][c] > 12.0f) {
                display_fill_circle(fb, px[r][c], py[r][c], 2, COLOR_WHITE);
            }
        }
    }
}

Effect fx_laser_grid = {
    .id = "laser_grid",
    .title = "赛博激光阵列",
    .subtitle = "多维空间李萨如全息高能激光波动曲面",
    .init = laser_grid_init,
    .update = laser_grid_update,
    .render = laser_grid_render,
    .cleanup = laser_grid_cleanup
};
