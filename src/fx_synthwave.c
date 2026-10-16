#include "effects.h"
#include "fast_math.h"
#include <string.h>
#include <stdlib.h>

#define HORIZON_Y 110
#define NUM_GRID_LINES 18
#define NUM_SYNTH_STARS 24

static float grid_scroll = 0.0f;
static float sun_pulse = 0.0f;

static void synthwave_init(void) {
    grid_scroll = 0.0f;
    sun_pulse = 0.0f;
}

static void synthwave_cleanup(void) {
}

static void synthwave_update(float dt) {
    (void)dt;
    grid_scroll += 0.055f;
    if (grid_scroll >= 1.0f) grid_scroll -= 1.0f;

    sun_pulse += 0.04f;
    if (sun_pulse > 2.0f * FAST_PI) sun_pulse -= 2.0f * FAST_PI;
}

static void synthwave_render(uint16_t *fb) {
    // 1. 天空渐变 (暗黑紫过渡至地平线暗红霓虹)
    for (int y = 0; y < HORIZON_Y; y++) {
        float f = (float)y / (float)HORIZON_Y;
        uint8_t r = (uint8_t)(45 * f * f);
        uint8_t g = (uint8_t)(10 * f);
        uint8_t b = (uint8_t)(35 + 40 * f);
        uint16_t sky_col = rgb565(r, g, b);
        uint32_t c32 = ((uint32_t)sky_col << 16) | sky_col;
        uint32_t *row = (uint32_t *)(fb + y * DISP_W);
        for (int x = 0; x < DISP_W / 2; x++) {
            row[x] = c32;
        }
    }

    // 2. 闪烁霓虹星光 (天空区域)
    for (int i = 0; i < NUM_SYNTH_STARS; i++) {
        int sx = (i * 37 + 13) % (DISP_W - 10) + 5;
        int sy = (i * 19 + 7) % (HORIZON_Y - 30) + 5;
        float twinkle = fast_sin(sun_pulse * 1.5f + (float)i);
        if (twinkle > 0.3f) {
            uint16_t star_c = (twinkle > 0.85f) ? COLOR_WHITE : rgb565(140, 160, 255);
            fb[sy * DISP_W + sx] = star_c;
        }
    }

    // 3. 绘制复古巨型霓虹落日 (中心 120, 85, 半径 38)
    const int sun_cx = 120;
    const int sun_cy = 85;
    const int sun_r = 38;
    int r2 = sun_r * sun_r;

    for (int dy = -sun_r; dy <= sun_r; dy++) {
        int py = sun_cy + dy;
        if (py < 0 || py >= HORIZON_Y) continue;

        // 经典百叶窗水平条纹 (在落日下半部逐渐加宽)
        if (dy > 0) {
            int stripe = (dy + (int)(sun_pulse * 4.0f)) % 9;
            int cutoff = 2 + (dy * 5) / sun_r;
            if (stripe < cutoff) {
                continue; // 镂空黑条
            }
        }

        int dx_max = (int)sqrtf((float)(r2 - dy * dy));
        float t = (float)(dy + sun_r) / (float)(2 * sun_r);

        // 垂直渐变：上部炽金黄 -> 下部荧光粉紫
        uint8_t sr = 255;
        uint8_t sg = (uint8_t)(240 * (1.0f - t) + 20 * t);
        uint8_t sb = (uint8_t)(40 * (1.0f - t) + 200 * t);
        uint16_t sun_col = rgb565(sr, sg, sb);

        for (int px = sun_cx - dx_max; px <= sun_cx + dx_max; px++) {
            if (px >= 0 && px < DISP_W) {
                fb[py * DISP_W + px] = sun_col;
            }
        }
    }

    // 4. 地平线暗夜山脉剪影 (两侧黑紫山峰)
    static const int mountain_pts[][2] = {
        {0, 110}, {25, 96}, {55, 105}, {75, 92}, {95, 110},
        {145, 110}, {165, 94}, {190, 104}, {215, 95}, {240, 110}
    };
    uint16_t mtn_line = rgb565(140, 0, 180);
    for (int i = 0; i < 4; i++) {
        display_draw_line(fb, mountain_pts[i][0], mountain_pts[i][1], mountain_pts[i+1][0], mountain_pts[i+1][1], mtn_line);
    }
    for (int i = 5; i < 9; i++) {
        display_draw_line(fb, mountain_pts[i][0], mountain_pts[i][1], mountain_pts[i+1][0], mountain_pts[i+1][1], mtn_line);
    }

    // 5. 地平线暗夜地面清除
    for (int y = HORIZON_Y; y < DISP_H; y++) {
        uint32_t *row = (uint32_t *)(fb + y * DISP_W);
        for (int x = 0; x < DISP_W / 2; x++) {
            row[x] = 0;
        }
    }

    // 6. 绘制透视纵深网格线 (向地平线收敛)
    for (int i = -8; i <= 8; i++) {
        int bottom_x = 120 + i * 26;
        int top_x = 120 + i * 2;
        display_draw_line(fb, top_x, HORIZON_Y, bottom_x, DISP_H - 1, rgb565(20, 160, 240));
    }

    // 7. 绘制向前滚动的横向透视网格线 (距离越近间隔越大、越明亮)
    for (int i = 0; i < NUM_GRID_LINES; i++) {
        float norm_d = (float)i + grid_scroll;
        float depth = norm_d / (float)NUM_GRID_LINES;
        // 指数透视投影
        int y = HORIZON_Y + (int)((float)(DISP_H - 1 - HORIZON_Y) * (depth * depth));
        if (y >= HORIZON_Y && y < DISP_H) {
            uint8_t intensity = (uint8_t)(60 + 195 * depth);
            uint16_t line_col = rgb565(intensity, 0, (uint8_t)(intensity * 0.9f));
            uint32_t c32 = ((uint32_t)line_col << 16) | line_col;
            uint32_t *row = (uint32_t *)(fb + y * DISP_W);
            for (int x = 0; x < DISP_W / 2; x++) {
                row[x] = c32;
            }
        }
    }

    // 8. 地平线强光高亮霓虹线
    uint16_t horizon_col = rgb565(120, 255, 255);
    uint32_t h32 = ((uint32_t)horizon_col << 16) | horizon_col;
    uint32_t *hrow = (uint32_t *)(fb + HORIZON_Y * DISP_W);
    for (int x = 0; x < DISP_W / 2; x++) {
        hrow[x] = h32;
    }
}

Effect fx_synthwave = {
    .id = "synthwave",
    .title = "» CYBER SYNTHWAVE «",
    .subtitle = "赛博朋克 80s 霓虹落日与透视网格",
    .init = synthwave_init,
    .update = synthwave_update,
    .render = synthwave_render,
    .cleanup = synthwave_cleanup
};
