#include "effects.h"
#include "fast_math.h"
#include <string.h>
#include <stdlib.h>
#include <math.h>

static float anim_time = 0.0f;
static float heading_angle = 0.0f;
static float pitch_offset = 0.0f;
static float roll_angle = 0.0f;

static void compass_init(void) {
    anim_time = 0.0f;
    heading_angle = 0.0f;
    pitch_offset = 0.0f;
    roll_angle = 0.0f;
}

static void compass_cleanup(void) {
}

static void compass_update(float dt) {
    anim_time += dt;
    heading_angle += dt * 0.45f;
    pitch_offset = 22.0f * sinf(anim_time * 1.3f);
    roll_angle = 0.35f * sinf(anim_time * 0.9f);
}

static void compass_render(uint16_t *fb) {
    memset(fb, 0, DISP_W * DISP_H * sizeof(uint16_t));
    int cx = DISP_W / 2;
    int cy = DISP_H / 2;

    // 1. 最外层 360 度精密航空刻度盘
    display_draw_circle(fb, cx, cy, 108, rgb565(0, 70, 100));
    display_draw_circle(fb, cx, cy, 104, rgb565(0, 160, 220));

    for (int i = 0; i < 36; i++) {
        float a = heading_angle + (float)i * (FAST_PI / 18.0f);
        int r_in = (i % 9 == 0) ? 90 : ((i % 3 == 0) ? 94 : 98);
        uint16_t col = (i % 9 == 0) ? COLOR_WHITE : ((i % 3 == 0) ? rgb565(0, 240, 255) : rgb565(0, 100, 140));

        int x1 = cx + (int)(r_in * cosf(a));
        int y1 = cy + (int)(r_in * sinf(a));
        int x2 = cx + (int)(104.0f * cosf(a));
        int y2 = cy + (int)(104.0f * sinf(a));
        display_draw_line(fb, x1, y1, x2, y2, col);
    }

    // 2. 战机 HUD 姿态球三维俯仰经纬阶梯 (Pitch & Roll Ladder)
    float c_roll = cosf(roll_angle), s_roll = sinf(roll_angle);
    int pitch_y = (int)pitch_offset;

    for (int p = -4; p <= 4; p++) {
        int py_local = pitch_y + p * 18;
        if (abs(py_local) > 65) continue;

        int ladder_w = (p == 0) ? 52 : ((abs(p) % 2 == 0) ? 32 : 18);
        uint16_t l_col = (p == 0) ? rgb565(0, 255, 180) : ((p > 0) ? rgb565(0, 200, 240) : rgb565(255, 140, 40));

        int lx1 = -ladder_w, ly1 = py_local;
        int lx2 = ladder_w, ly2 = py_local;

        // 旋转变换
        int rx1 = cx + (int)(lx1 * c_roll - ly1 * s_roll);
        int ry1 = cy + (int)(lx1 * s_roll + ly1 * c_roll);
        int rx2 = cx + (int)(lx2 * c_roll - ly2 * s_roll);
        int ry2 = cy + (int)(lx2 * s_roll + ly2 * c_roll);

        display_draw_line(fb, rx1, ry1, rx2, ry2, l_col);

        // 两侧小折角
        if (p != 0) {
            int flap_len = (p > 0) ? 5 : -5;
            int fx1 = rx1 + (int)(-flap_len * s_roll);
            int fy1 = ry1 + (int)(flap_len * c_roll);
            display_draw_line(fb, rx1, ry1, fx1, fy1, l_col);

            int fx2 = rx2 + (int)(-flap_len * s_roll);
            int fy2 = ry2 + (int)(flap_len * c_roll);
            display_draw_line(fb, rx2, ry2, fx2, fy2, l_col);
        }
    }

    // 3. 飞行速度矢量锁定十字准星
    display_draw_circle(fb, cx, cy, 32, rgb565(0, 180, 240));
    display_draw_line(fb, cx - 22, cy, cx - 8, cy, rgb565(255, 230, 40));
    display_draw_line(fb, cx + 8, cy, cx + 22, cy, rgb565(255, 230, 40));
    display_draw_line(fb, cx, cy - 22, cx, cy - 8, rgb565(255, 230, 40));
    display_draw_line(fb, cx, cy + 8, cx, cy + 22, rgb565(255, 230, 40));
    display_fill_circle(fb, cx, cy, 2, COLOR_WHITE);

    // 4. 正北三角对齐信标
    display_draw_line(fb, cx - 7, 24, cx + 7, 24, rgb565(255, 50, 60));
    display_draw_line(fb, cx - 7, 24, cx, 14, rgb565(255, 50, 60));
    display_draw_line(fb, cx + 7, 24, cx, 14, rgb565(255, 50, 60));
}

Effect fx_compass = {
    .id = "compass",
    .title = "赛博全息罗盘HUD",
    .subtitle = "三维姿态俯仰阶梯网格与航空罗盘",
    .init = compass_init,
    .update = compass_update,
    .render = compass_render,
    .cleanup = compass_cleanup
};
