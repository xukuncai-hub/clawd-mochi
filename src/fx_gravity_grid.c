#include "effects.h"
#include "fast_math.h"
#include <string.h>
#include <math.h>

#define GRID_COLS 18
#define GRID_ROWS 18

static float anim_time = 0.0f;

static void gravity_grid_init(void) {
    anim_time = 0.0f;
}

static void gravity_grid_cleanup(void) {
}

static void gravity_grid_update(float dt) {
    anim_time += dt;
}

static void gravity_grid_render(uint16_t *fb) {
    memset(fb, 0, DISP_W * DISP_H * sizeof(uint16_t));
    int cx = DISP_W / 2;
    int cy = DISP_H / 2;

    // 双黑洞天体引力源 (绕双星质心旋转)
    float g1_x = (float)cx + 62.0f * cosf(anim_time * 1.5f);
    float g1_y = (float)cy + 48.0f * sinf(anim_time * 1.5f);

    float g2_x = (float)cx + 52.0f * cosf(-anim_time * 1.8f + 2.2f);
    float g2_y = (float)cy + 62.0f * sinf(-anim_time * 1.8f + 2.2f);

    static int px[GRID_ROWS][GRID_COLS];
    static int py[GRID_ROWS][GRID_COLS];

    float col_step = (float)(DISP_W - 16) / (float)(GRID_COLS - 1);
    float row_step = (float)(DISP_H - 16) / (float)(GRID_ROWS - 1);

    for (int r = 0; r < GRID_ROWS; r++) {
        for (int c = 0; c < GRID_COLS; c++) {
            float ox = 8.0f + (float)c * col_step;
            float oy = 8.0f + (float)r * row_step;

            // 引力源 1 的吸积漏斗位移
            float dx1 = g1_x - ox;
            float dy1 = g1_y - oy;
            float d2_1 = dx1 * dx1 + dy1 * dy1 + 160.0f;
            float pull1 = 700.0f / d2_1;

            // 引力源 2 的吸积漏斗位移
            float dx2 = g2_x - ox;
            float dy2 = g2_y - oy;
            float d2_2 = dx2 * dx2 + dy2 * dy2 + 160.0f;
            float pull2 = 580.0f / d2_2;

            // 双黑洞并合辐射出的引力波高频涟漪
            float d_center = sqrtf((ox - (float)cx) * (ox - (float)cx) + (oy - (float)cy) * (oy - (float)cy));
            float gw_wave = sinf(d_center * 0.12f - anim_time * 6.0f) * 3.5f;

            float nx = ox + dx1 * pull1 + dx2 * pull2 + (dx1 / (sqrtf(d2_1))) * gw_wave;
            float ny = oy + dy1 * pull1 + dy2 * pull2 + (dy1 / (sqrtf(d2_1))) * gw_wave;

            px[r][c] = (int)nx;
            py[r][c] = (int)ny;
        }
    }

    // 绘制横向曲率网格线 (流光彩虹光谱)
    for (int r = 0; r < GRID_ROWS; r++) {
        uint8_t hue = (uint8_t)((int)(anim_time * 25.0f + r * 7) & 0xFF);
        uint16_t col = hsv2rgb565(hue, 225, 220);
        for (int c = 0; c < GRID_COLS - 1; c++) {
            display_draw_line(fb, px[r][c], py[r][c], px[r][c + 1], py[r][c + 1], col);
        }
    }

    // 绘制纵向曲率网格线 (正交互补光谱)
    for (int c = 0; c < GRID_COLS; c++) {
        uint8_t hue = (uint8_t)((int)(anim_time * 25.0f + c * 7 + 128) & 0xFF);
        uint16_t col = hsv2rgb565(hue, 225, 220);
        for (int r = 0; r < GRID_ROWS - 1; r++) {
            display_draw_line(fb, px[r][c], py[r][c], px[r + 1][c], py[r + 1][c], col);
        }
    }

    // 核心网格交点晶莹微光
    for (int r = 2; r < GRID_ROWS - 2; r += 2) {
        for (int c = 2; c < GRID_COLS - 2; c += 2) {
            display_draw_pixel(fb, px[r][c], py[r][c], COLOR_WHITE);
        }
    }

    // 绘制主引力奇点黑洞 (事件视界纯黑、吸积盘白热高光、外层高能电浆光晕)
    display_fill_circle(fb, (int)g1_x, (int)g1_y, 7, rgb565(0, 180, 255));
    display_fill_circle(fb, (int)g1_x, (int)g1_y, 4, COLOR_WHITE);
    display_draw_circle(fb, (int)g1_x, (int)g1_y, 11, rgb565(0, 240, 255));

    display_fill_circle(fb, (int)g2_x, (int)g2_y, 6, rgb565(255, 60, 160));
    display_fill_circle(fb, (int)g2_x, (int)g2_y, 3, rgb565(255, 240, 120));
    display_draw_circle(fb, (int)g2_x, (int)g2_y, 9, rgb565(255, 120, 200));
}

Effect fx_gravity_grid = {
    .id = "gravity_grid",
    .title = "赛博引力时空曲率",
    .subtitle = "双黑洞相对论时空曲率漏斗拉扯",
    .init = gravity_grid_init,
    .update = gravity_grid_update,
    .render = gravity_grid_render,
    .cleanup = gravity_grid_cleanup
};
