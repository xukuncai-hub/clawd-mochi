#include "effects.h"
#include "fast_math.h"
#include <string.h>
#include <math.h>

#define NUM_BARS 48
#define SPECTRUM_RINGS 4

static float bar_heights[NUM_BARS];
static float bar_peaks[NUM_BARS];
static float anim_time = 0.0f;

static void circular_spectrum_init(void) {
    anim_time = 0.0f;
    for (int i = 0; i < NUM_BARS; i++) {
        bar_heights[i] = 10.0f;
        bar_peaks[i] = 10.0f;
    }
}

static void circular_spectrum_cleanup(void) {
}

static void circular_spectrum_update(float dt) {
    anim_time += dt;

    for (int i = 0; i < NUM_BARS; i++) {
        float h = 14.0f + 28.0f * (0.5f + 0.5f * sinf(anim_time * 4.5f + (float)i * 0.4f))
                        + 18.0f * (0.5f + 0.5f * sinf(anim_time * 8.0f - (float)i * 0.6f));
        bar_heights[i] = h;

        if (h > bar_peaks[i]) {
            bar_peaks[i] = h;
        } else {
            bar_peaks[i] -= dt * 28.0f;
            if (bar_peaks[i] < h) bar_peaks[i] = h;
        }
    }
}

static void circular_spectrum_render(uint16_t *fb) {
    memset(fb, 0, DISP_W * DISP_H * sizeof(uint16_t));
    int cx = DISP_W / 2;
    int cy = DISP_H / 2;

    float base_r = 36.0f;

    // 1. 绘制声波共振蜘蛛网状同心多边形网格 (Web Mesh)
    for (int ring = 1; ring <= SPECTRUM_RINGS; ring++) {
        float r_ring = base_r + (float)ring * 16.0f;
        int last_wx = 0, last_wy = 0;
        int first_wx = 0, first_wy = 0;

        for (int i = 0; i < NUM_BARS; i += 2) {
            float a = (float)i * (2.0f * FAST_PI / (float)NUM_BARS) + anim_time * 0.25f;
            // 声波对网格的局部挤压
            float warp = bar_heights[i] * 0.25f;
            int wx = cx + (int)((r_ring + warp) * cosf(a));
            int wy = cy + (int)((r_ring + warp) * sinf(a));

            if (i == 0) {
                first_wx = wx;
                first_wy = wy;
            } else {
                display_draw_line(fb, last_wx, last_wy, wx, wy, rgb565(0, 50, 75));
            }
            last_wx = wx;
            last_wy = wy;
        }
        display_draw_line(fb, last_wx, last_wy, first_wx, first_wy, rgb565(0, 50, 75));
    }

    // 2. 绘制 48 根圆周频谱辐射柱与峰值回落点
    for (int i = 0; i < NUM_BARS; i++) {
        float angle = (float)i * (2.0f * FAST_PI / (float)NUM_BARS) + anim_time * 0.25f;
        float cos_a = cosf(angle);
        float sin_a = sinf(angle);

        int in_x = cx + (int)(base_r * cos_a);
        int in_y = cy + (int)(base_r * sin_a);

        int out_x = cx + (int)((base_r + bar_heights[i]) * cos_a);
        int out_y = cy + (int)((base_r + bar_heights[i]) * sin_a);

        uint8_t hue = (uint8_t)((int)(anim_time * 25.0f + i * 5) & 0xFF);
        uint16_t col = hsv2rgb565(hue, 230, 255);

        display_draw_line(fb, in_x, in_y, out_x, out_y, col);

        // 峰值悬浮发光点
        int peak_x = cx + (int)((base_r + bar_peaks[i] + 3.0f) * cos_a);
        int peak_y = cy + (int)((base_r + bar_peaks[i] + 3.0f) * sin_a);
        display_fill_circle(fb, peak_x, peak_y, 2, COLOR_WHITE);
    }

    // 3. 中央超重低音共振光球
    float bass = sinf(anim_time * 9.0f);
    int center_r = (int)(20.0f + bass * 3.5f);
    display_fill_circle(fb, cx, cy, center_r + 4, rgb565(0, 60, 100));
    display_fill_circle(fb, cx, cy, center_r, rgb565(0, 200, 255));
    display_fill_circle(fb, cx, cy, center_r / 2, COLOR_WHITE);
}

Effect fx_circular_spectrum = {
    .id = "circular_spectrum",
    .title = "赛博环形音频频谱仪",
    .subtitle = "360度声波共振蜘蛛网曲面与辐射柱",
    .init = circular_spectrum_init,
    .update = circular_spectrum_update,
    .render = circular_spectrum_render,
    .cleanup = circular_spectrum_cleanup
};
