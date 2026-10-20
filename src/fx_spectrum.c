#include "effects.h"
#include "fast_math.h"
#include <string.h>
#include <math.h>

#define SPEC_ROWS 12
#define SPEC_COLS 20

static float anim_time = 0.0f;

static void spectrum_init(void) {
    anim_time = 0.0f;
}

static void spectrum_cleanup(void) {
}

static void spectrum_update(float dt) {
    anim_time += dt;
}

static void spectrum_render(uint16_t *fb) {
    memset(fb, 0, DISP_W * DISP_H * sizeof(uint16_t));
    int cx = DISP_W / 2;

    static int px[SPEC_ROWS][SPEC_COLS];
    static int py[SPEC_ROWS][SPEC_COLS];

    float col_w = 230.0f / (float)(SPEC_COLS - 1);

    // 1. 计算三维透视声波山脉曲面
    for (int r = 0; r < SPEC_ROWS; r++) {
        // 从远景向近景排列
        float depth = (float)(r + 1) / (float)SPEC_ROWS;
        float base_y = 40.0f + depth * 170.0f;
        float width_scale = 0.35f + depth * 0.75f;

        for (int c = 0; c < SPEC_COLS; c++) {
            float x0 = ((float)c - (float)(SPEC_COLS - 1) * 0.5f) * col_w * width_scale;

            // 中央高能音频声波脉冲包络
            float center_dist = fabsf((float)c - (float)(SPEC_COLS - 1) * 0.5f);
            float envelope = expf(-center_dist * center_dist * 0.08f);

            // 多频音频合成震荡
            float wave = 32.0f * envelope * (0.5f + 0.5f * sinf(anim_time * 6.5f - (float)r * 0.6f + (float)c * 0.7f))
                       + 16.0f * (0.5f + 0.5f * sinf(anim_time * 11.0f + (float)c * 1.2f)) * envelope;

            px[r][c] = cx + (int)x0;
            py[r][c] = (int)(base_y - wave * depth);
        }
    }

    // 2. 从远到近绘制山脊山脉横向声波曲率线
    for (int r = 0; r < SPEC_ROWS; r++) {
        float depth = (float)(r + 1) / (float)SPEC_ROWS;
        uint8_t hue = (uint8_t)((int)(anim_time * 28.0f + r * 16) & 0xFF);
        uint8_t val = (uint8_t)(100.0f + 155.0f * depth);
        uint16_t col = hsv2rgb565(hue, 220, val);

        for (int c = 0; c < SPEC_COLS - 1; c++) {
            display_draw_line(fb, px[r][c], py[r][c], px[r][c + 1], py[r][c + 1], col);
        }
    }

    // 3. 绘制声波纵向脉冲导轨经线
    for (int c = 0; c < SPEC_COLS; c += 2) {
        for (int r = 0; r < SPEC_ROWS - 1; r++) {
            uint16_t rib_col = rgb565(0, 70, 110);
            display_draw_line(fb, px[r][c], py[r][c], px[r + 1][c], py[r + 1][c], rib_col);
        }
    }
}

Effect fx_spectrum = {
    .id = "spectrum",
    .title = "赛博3D声波山脉曲面",
    .subtitle = "全息音频频谱多层流光声波山脊网格",
    .init = spectrum_init,
    .update = spectrum_update,
    .render = spectrum_render,
    .cleanup = spectrum_cleanup
};
