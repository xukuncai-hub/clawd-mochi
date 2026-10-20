#include "effects.h"
#include "fast_math.h"
#include <string.h>
#include <math.h>

#define TERRAIN_COLS 16
#define TERRAIN_ROWS 10

static float anim_time = 0.0f;

static void synthwave_init(void) {
    anim_time = 0.0f;
}

static void synthwave_cleanup(void) {
}

static void synthwave_update(float dt) {
    anim_time += dt;
}

static void synthwave_render(uint16_t *fb) {
    memset(fb, 0, DISP_W * DISP_H * sizeof(uint16_t));
    int cx = DISP_W / 2;
    int horizon_y = 100;

    // 1. 上半部深空渐变与璀璨霓虹落日 (带有横向盲盒切片扫描)
    for (int y = 0; y < horizon_y; y++) {
        uint8_t r = (uint8_t)(y * 0.45f);
        uint8_t b = (uint8_t)(35 + y * 0.5f);
        display_fill_rect(fb, 0, y, DISP_W, 1, rgb565(r, 0, b));
    }

    // 绘制 80s 经典赛博落日
    int sun_r = 38;
    int sun_cy = horizon_y - 10;
    for (int y = sun_cy - sun_r; y <= horizon_y; y++) {
        if (y < 0) continue;
        int dy = y - sun_cy;
        int dx = (int)sqrtf((float)(sun_r * sun_r - dy * dy));

        // 越往下切片越宽
        int slice_phase = (y + (int)(anim_time * 20.0f)) % 9;
        if (y > sun_cy && slice_phase < 3) continue; // 黑色水平光栅横纹

        float factor = (float)(y - (sun_cy - sun_r)) / (float)(sun_r * 2);
        uint8_t sr = 255;
        uint8_t sg = (uint8_t)(240.0f * (1.0f - factor * 0.8f));
        uint8_t sb = (uint8_t)(40.0f + 160.0f * factor);
        display_draw_line(fb, cx - dx, y, cx + dx, y, rgb565(sr, sg, sb));
    }

    // 地平线高光激光条
    display_fill_rect(fb, 0, horizon_y, DISP_W, 2, rgb565(0, 240, 255));

    // 2. 下半部 3D 动态波浪地形曲面网格 (Terrain Wave Mesh)
    static int px[TERRAIN_ROWS][TERRAIN_COLS];
    static int py[TERRAIN_ROWS][TERRAIN_COLS];

    float col_w = 260.0f / (float)(TERRAIN_COLS - 1);

    for (int r = 0; r < TERRAIN_ROWS; r++) {
        float depth = (float)(r + 1) / (float)TERRAIN_ROWS; // 0.1 ~ 1.0
        float z_scale = depth * depth; // 非线性透视

        float cur_y_base = (float)horizon_y + z_scale * (float)(DISP_H - horizon_y);
        float width_scale = 0.25f + 0.95f * z_scale;

        for (int c = 0; c < TERRAIN_COLS; c++) {
            float x0 = ((float)c - (float)(TERRAIN_COLS - 1) * 0.5f) * col_w * width_scale;

            // 地形起伏正弦波浪 (两侧山峰高，中间公路平坦)
            float side_factor = fabsf((float)c - (float)(TERRAIN_COLS - 1) * 0.5f);
            float mountain = (side_factor > 2.5f) ? (side_factor * 2.8f) : 0.0f;
            float wave = mountain * sinf(anim_time * 4.0f - (float)r * 0.65f);

            px[r][c] = cx + (int)x0;
            py[r][c] = (int)(cur_y_base - wave);
        }
    }

    // 绘制横向起伏网格线 (向近处扑面而来)
    for (int r = 0; r < TERRAIN_ROWS; r++) {
        float depth = (float)(r + 1) / (float)TERRAIN_ROWS;
        uint8_t val = (uint8_t)(depth * 255.0f);
        uint16_t col = rgb565(val, 0, (uint8_t)(val * 0.85f)); // 霓虹粉紫

        for (int c = 0; c < TERRAIN_COLS - 1; c++) {
            display_draw_line(fb, px[r][c], py[r][c], px[r][c + 1], py[r][c + 1], col);
        }
    }

    // 绘制纵向透视网格线 (从地平线放射延伸)
    for (int c = 0; c < TERRAIN_COLS; c++) {
        uint16_t line_col = (c == TERRAIN_COLS / 2 || c == TERRAIN_COLS / 2 - 1) ? rgb565(0, 240, 255) : rgb565(0, 150, 210);
        for (int r = 0; r < TERRAIN_ROWS - 1; r++) {
            display_draw_line(fb, px[r][c], py[r][c], px[r + 1][c], py[r + 1][c], line_col);
        }
    }
}

Effect fx_synthwave = {
    .id = "synthwave",
    .title = "赛博朋克霓虹落日",
    .subtitle = "3D动态合成波山脉地形曲面网格与落日",
    .init = synthwave_init,
    .update = synthwave_update,
    .render = synthwave_render,
    .cleanup = synthwave_cleanup
};
