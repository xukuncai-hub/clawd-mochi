#include "effects.h"
#include "fast_math.h"
#include <string.h>
#include <math.h>

#define LISSAJOUS_PTS 72

static float phase_x = 0.0f;
static float phase_y = 0.0f;
static float phase_z = 0.0f;
static float anim_time = 0.0f;

static void oscilloscope_init(void) {
    phase_x = 0.0f;
    phase_y = 0.0f;
    phase_z = 0.0f;
    anim_time = 0.0f;
}

static void oscilloscope_cleanup(void) {
}

static void oscilloscope_update(float dt) {
    anim_time += dt;
    phase_x += dt * 0.95f;
    phase_y += dt * 1.45f;
    phase_z += dt * 0.65f;
}

static void oscilloscope_render(uint16_t *fb) {
    memset(fb, 0, DISP_W * DISP_H * sizeof(uint16_t));
    int cx = DISP_W / 2;
    int cy = DISP_H / 2;

    static int left_x[LISSAJOUS_PTS], left_y[LISSAJOUS_PTS];
    static int right_x[LISSAJOUS_PTS], right_y[LISSAJOUS_PTS];
    static float pts_z[LISSAJOUS_PTS];

    float wx = 3.0f, wy = 2.0f, wz = 4.0f;
    float ribbon_w = 9.0f;

    // 1. 计算三维立体李萨如带状网格左右两翼坐标
    for (int i = 0; i < LISSAJOUS_PTS; i++) {
        float t = (float)i * (2.0f * FAST_PI / (float)LISSAJOUS_PTS);

        float x0 = 85.0f * sinf(wx * t + phase_x);
        float y0 = 85.0f * sinf(wy * t + phase_y);
        float z0 = 60.0f * sinf(wz * t + phase_z);

        // 法向带状偏移向量
        float dx = -wy * cosf(wy * t + phase_y);
        float dy = wx * cosf(wx * t + phase_x);
        float len = sqrtf(dx * dx + dy * dy) + 0.01f;
        float nx = (dx / len) * ribbon_w;
        float ny = (dy / len) * ribbon_w;

        // 透视投影
        float fov = 180.0f;
        float scale = fov / (fov + z0);

        left_x[i] = cx + (int)((x0 - nx) * scale);
        left_y[i] = cy + (int)((y0 - ny) * scale);

        right_x[i] = cx + (int)((x0 + nx) * scale);
        right_y[i] = cy + (int)((y0 + ny) * scale);

        pts_z[i] = z0;
    }

    // 2. 绘制连续立体扭转彩带边线与横向肋骨网格
    for (int i = 0; i < LISSAJOUS_PTS; i++) {
        int next = (i + 1) % LISSAJOUS_PTS;
        float avg_z = (pts_z[i] + pts_z[next]) * 0.5f;

        uint8_t hue = (uint8_t)((int)(anim_time * 30.0f + i * 4) & 0xFF);
        uint8_t val = (uint8_t)(120.0f + 135.0f * ((avg_z + 60.0f) / 120.0f));
        uint16_t col = hsv2rgb565(hue, 230, val);

        // 左右两翼边线
        display_draw_line(fb, left_x[i], left_y[i], left_x[next], left_y[next], col);
        display_draw_line(fb, right_x[i], right_y[i], right_x[next], right_y[next], col);

        // 横向编织肋骨 (每 2 点一条)
        if (i % 2 == 0) {
            uint16_t rib_col = (pts_z[i] > 0) ? COLOR_WHITE : rgb565(0, 140, 200);
            display_draw_line(fb, left_x[i], left_y[i], right_x[i], right_y[i], rib_col);
        }
    }
}

Effect fx_oscilloscope = {
    .id = "oscilloscope",
    .title = "赛博激光李萨如示波器",
    .subtitle = "三维多谐波共振立体空间流光带",
    .init = oscilloscope_init,
    .update = oscilloscope_update,
    .render = oscilloscope_render,
    .cleanup = oscilloscope_cleanup
};
