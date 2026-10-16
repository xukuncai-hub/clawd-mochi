#include "effects.h"
#include "fast_math.h"
#include <string.h>
#include <stdlib.h>

#define NUM_POINTS 96

static float phase_x = 0.0f;
static float phase_y = 0.0f;
static float phase_z = 0.0f;
static float freq_ratio = 1.0f;
static float mode_timer = 0.0f;
static int color_cycle = 0;

static void oscilloscope_init(void) {
    phase_x = 0.0f;
    phase_y = 0.0f;
    phase_z = 0.0f;
    freq_ratio = 1.0f;
    mode_timer = 0.0f;
    color_cycle = 0;
}

static void oscilloscope_cleanup(void) {
}

static void oscilloscope_update(float dt) {
    (void)dt;
    phase_x += 0.048f;
    phase_y += 0.073f;
    phase_z += 0.031f;
    if (phase_x > 2.0f * FAST_PI) phase_x -= 2.0f * FAST_PI;
    if (phase_y > 2.0f * FAST_PI) phase_y -= 2.0f * FAST_PI;
    if (phase_z > 2.0f * FAST_PI) phase_z -= 2.0f * FAST_PI;

    mode_timer += 0.015f;
    freq_ratio = 1.0f + 1.5f * (0.5f + 0.5f * fast_sin(mode_timer * 0.4f)); // 频率比平滑调制 (1.0 ~ 2.5)
    color_cycle = (color_cycle + 1) & 255;
}

// 快速 16 位 RGB565 余辉衰减函数 (模拟 CRT 示波管荧光粉衰变残影)
static inline uint16_t fade_phosphor(uint16_t col) {
    // 快速将每个色彩分量乘以约 0.78 (向暗黑渐变)
    uint32_t r = (col >> 11) & 0x1F;
    uint32_t g = (col >> 5) & 0x3F;
    uint32_t b = col & 0x1F;

    r = (r * 25) >> 5;
    g = (g * 25) >> 5;
    b = (b * 25) >> 5;

    return (uint16_t)((r << 11) | (g << 5) | b);
}

static void oscilloscope_render(uint16_t *fb) {
    // 1. 全屏荧光残影衰减 (32 位成对快速处理)
    uint32_t *fb32 = (uint32_t *)fb;
    int words = (DISP_W * DISP_H) / 2;
    for (int i = 0; i < words; i++) {
        uint32_t p = fb32[i];
        if (p != 0) {
            uint16_t c0 = fade_phosphor((uint16_t)(p & 0xFFFF));
            uint16_t c1 = fade_phosphor((uint16_t)(p >> 16));
            fb32[i] = ((uint32_t)c1 << 16) | c0;
        }
    }

    const int cx = 120;
    const int cy = 120;
    const float radius = 95.0f;

    // 2. 计算 3D 李萨如空间轨迹点
    int px_prev = 0, py_prev = 0;
    bool first = true;

    for (int i = 0; i <= NUM_POINTS; i++) {
        float t = (float)i * (2.0f * FAST_PI / (float)NUM_POINTS);

        float x = fast_sin(t * 2.0f + phase_x);
        float y = fast_sin(t * freq_ratio * 3.0f + phase_y);
        float z = fast_cos(t + phase_z);

        // 3D 透视缩放与坐标投影
        float pz = z * 0.35f + 1.0f; // 0.65 ~ 1.35
        int sx = cx + (int)(x * radius * pz);
        int sy = cy + (int)(y * radius * pz);

        if (sx < 2) sx = 2; else if (sx >= DISP_W - 2) sx = DISP_W - 3;
        if (sy < 2) sy = 2; else if (sy >= DISP_H - 2) sy = DISP_H - 3;

        // 动态色环彩虹光束 (根据点位置与全局色彩循环生成)
        uint8_t hue = (uint8_t)(color_cycle + i * 2);
        uint16_t beam_col;
        if (hue < 85) {
            beam_col = rgb565(0, (uint8_t)(hue * 3), 255); // 蓝 -> 青
        } else if (hue < 170) {
            uint8_t sub = hue - 85;
            beam_col = rgb565(0, 255, (uint8_t)(255 - sub * 3)); // 青 -> 翠绿
        } else {
            uint8_t sub = hue - 170;
            beam_col = rgb565((uint8_t)(sub * 3), (uint8_t)(255 - sub * 2), (uint8_t)(sub * 2)); // 绿 -> 金粉
        }

        if (!first) {
            // 绘制主激光束与光晕
            display_draw_line(fb, px_prev, py_prev, sx, sy, beam_col);
            display_draw_line(fb, px_prev + 1, py_prev, sx + 1, sy, COLOR_WHITE); // 核心炽白束
        }

        px_prev = sx;
        py_prev = sy;
        first = false;
    }

    // 3. 示波器中心战术辅助十字微标
    fb[cy * DISP_W + (cx - 2)] = rgb565(0, 80, 50);
    fb[cy * DISP_W + (cx - 1)] = rgb565(0, 150, 90);
    fb[cy * DISP_W + cx]       = COLOR_WHITE;
    fb[cy * DISP_W + (cx + 1)] = rgb565(0, 150, 90);
    fb[cy * DISP_W + (cx + 2)] = rgb565(0, 80, 50);
    fb[(cy - 2) * DISP_W + cx] = rgb565(0, 80, 50);
    fb[(cy - 1) * DISP_W + cx] = rgb565(0, 150, 90);
    fb[(cy + 1) * DISP_W + cx] = rgb565(0, 150, 90);
    fb[(cy + 2) * DISP_W + cx] = rgb565(0, 80, 50);
}

Effect fx_oscilloscope = {
    .id = "oscilloscope",
    .title = "» LASER LISSAJOUS «",
    .subtitle = "赛博激光李萨如谐波示波器",
    .init = oscilloscope_init,
    .update = oscilloscope_update,
    .render = oscilloscope_render,
    .cleanup = oscilloscope_cleanup
};
