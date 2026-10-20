#include "effects.h"
#include "fast_math.h"
#include <string.h>
#include <stdlib.h>
#include <math.h>

#define HEX_COLS 10
#define HEX_ROWS 10

static float anim_time = 0.0f;

static void hex_wave_init(void) {
    anim_time = 0.0f;
}

static void hex_wave_cleanup(void) {
}

static void hex_wave_update(float dt) {
    anim_time += dt;
}

static void hex_wave_render(uint16_t *fb) {
    memset(fb, 0, DISP_W * DISP_H * sizeof(uint16_t));
    int cx = DISP_W / 2;
    int cy = DISP_H / 2;

    float r_base = 16.0f;
    float dx = r_base * 1.732f;
    float dy = r_base * 1.5f;

    // 两个行进扰动波源
    float w1_x = (float)cx + 60.0f * cosf(anim_time * 1.8f);
    float w1_y = (float)cy + 45.0f * sinf(anim_time * 1.8f);

    for (int r = -HEX_ROWS / 2; r <= HEX_ROWS / 2; r++) {
        float offset_x = (abs(r) % 2 != 0) ? (dx * 0.5f) : 0.0f;
        for (int c = -HEX_COLS / 2; c <= HEX_COLS / 2; c++) {
            float ox = (float)cx + (float)c * dx + offset_x;
            float oy = (float)cy + (float)r * dy;

            if (ox < -15.0f || ox > (float)(DISP_W + 15) || oy < -15.0f || oy > (float)(DISP_H + 15)) continue;

            // 计算该六角单元受到的波形扰动
            float d1 = sqrtf((ox - w1_x) * (ox - w1_x) + (oy - w1_y) * (oy - w1_y));
            float d_center = sqrtf((ox - (float)cx) * (ox - (float)cx) + (oy - (float)cy) * (oy - (float)cy));

            float wave = sinf(d1 * 0.09f - anim_time * 5.0f) * 0.6f
                       + cosf(d_center * 0.08f - anim_time * 3.5f) * 0.4f;

            // 径向拉伸与缩放
            float cur_r = r_base * (0.65f + 0.35f * wave);
            int cur_x = (int)(ox + (ox - (float)cx) * 0.08f * wave);
            int cur_y = (int)(oy + (oy - (float)cy) * 0.08f * wave);

            // 赋色：随波形振幅与空间全彩渐变
            uint8_t hue = (uint8_t)((int)(anim_time * 25.0f + d_center * 0.8f) & 0xFF);
            uint8_t val = (uint8_t)(140.0f + 115.0f * (wave * 0.5f + 0.5f));
            uint16_t col = hsv2rgb565(hue, 220, val);

            // 绘制六边形线框
            int px[6], py[6];
            for (int i = 0; i < 6; i++) {
                float a = (float)i * (FAST_PI / 3.0f);
                px[i] = cur_x + (int)(cur_r * cosf(a));
                py[i] = cur_y + (int)(cur_r * sinf(a));
            }
            for (int i = 0; i < 6; i++) {
                int next = (i + 1) % 6;
                display_draw_line(fb, px[i], py[i], px[next], py[next], col);
            }

            // 波峰高亮点
            if (wave > 0.5f) {
                display_fill_circle(fb, cur_x, cur_y, 2, COLOR_WHITE);
            }
        }
    }

    // 扰动源高光核心
    display_fill_circle(fb, (int)w1_x, (int)w1_y, 4, COLOR_WHITE);
    display_draw_circle(fb, (int)w1_x, (int)w1_y, 8, rgb565(0, 240, 255));
}

Effect fx_hex_wave = {
    .id = "hex_wave",
    .title = "赛博蜂巢矩阵波",
    .subtitle = "六角蜂窝弹性张力曲面动力场",
    .init = hex_wave_init,
    .update = hex_wave_update,
    .render = hex_wave_render,
    .cleanup = hex_wave_cleanup
};
