#include "effects.h"
#include "fast_math.h"
#include <string.h>
#include <stdlib.h>

#define RADAR_R 105
#define NUM_TARGETS 5

typedef struct {
    float x, y;
    float vx, vy;
    float alert_timer;
    bool locked;
} RadarTarget;

static float sweep_angle = 0.0f;
static float pulse_radius = 0.0f;
static RadarTarget targets[NUM_TARGETS];

static void radar_init(void) {
    sweep_angle = 0.0f;
    pulse_radius = 0.0f;

    for (int i = 0; i < NUM_TARGETS; i++) {
        targets[i].x = ((float)(fast_rand() % 140) - 70.0f);
        targets[i].y = ((float)(fast_rand() % 140) - 70.0f);
        targets[i].vx = ((float)(fast_rand() % 100) - 50.0f) * 0.008f;
        targets[i].vy = ((float)(fast_rand() % 100) - 50.0f) * 0.008f;
        targets[i].alert_timer = 0.0f;
        targets[i].locked = false;
    }
}

static void radar_cleanup(void) {
}

static void radar_update(float dt) {
    (void)dt;
    sweep_angle += 0.052f;
    if (sweep_angle > 2.0f * FAST_PI) sweep_angle -= 2.0f * FAST_PI;

    pulse_radius += 1.8f;
    if (pulse_radius > (float)RADAR_R) pulse_radius = 0.0f;

    for (int i = 0; i < NUM_TARGETS; i++) {
        targets[i].x += targets[i].vx;
        targets[i].y += targets[i].vy;

        float dist = sqrtf(targets[i].x * targets[i].x + targets[i].y * targets[i].y);
        if (dist > (float)(RADAR_R - 12)) {
            targets[i].vx = -targets[i].vx;
            targets[i].vy = -targets[i].vy;
        }

        // 检测雷达扫描线是否扫过目标
        float tgt_angle = atan2f(targets[i].y, targets[i].x);
        if (tgt_angle < 0) tgt_angle += 2.0f * FAST_PI;

        float diff = fabsf(sweep_angle - tgt_angle);
        if (diff < 0.08f) {
            targets[i].alert_timer = 1.0f;
            targets[i].locked = (i == 1 || i == 3);
        } else if (targets[i].alert_timer > 0.0f) {
            targets[i].alert_timer -= 0.02f;
            if (targets[i].alert_timer < 0.0f) targets[i].alert_timer = 0.0f;
        }
    }
}

static void draw_radar_circle(uint16_t *fb, int cx, int cy, int r, uint16_t color) {
    int x = 0, y = r;
    int d = 3 - 2 * r;
    while (x <= y) {
        display_draw_pixel(fb, cx + x, cy + y, color);
        display_draw_pixel(fb, cx - x, cy + y, color);
        display_draw_pixel(fb, cx + x, cy - y, color);
        display_draw_pixel(fb, cx - x, cy - y, color);
        display_draw_pixel(fb, cx + y, cy + x, color);
        display_draw_pixel(fb, cx - y, cy + x, color);
        display_draw_pixel(fb, cx + y, cy - x, color);
        display_draw_pixel(fb, cx - y, cy - x, color);
        if (d < 0) d += 4 * x + 6;
        else { d += 4 * (x - y) + 10; y--; }
        x++;
    }
}

static void radar_render(uint16_t *fb) {
    memset(fb, 0, DISP_W * DISP_H * sizeof(uint16_t));

    const int cx = 120;
    const int cy = 120;

    uint16_t grid_dim = rgb565(0, 50, 30);
    uint16_t grid_bright = rgb565(0, 160, 90);
    uint16_t beam_col = rgb565(120, 255, 180);

    // 1. 同心测距圆环 (35, 70, 105)
    draw_radar_circle(fb, cx, cy, 35, grid_dim);
    draw_radar_circle(fb, cx, cy, 70, grid_dim);
    draw_radar_circle(fb, cx, cy, RADAR_R, grid_bright);

    // 2. 战术十字刻度瞄准基线
    display_draw_line(fb, cx - RADAR_R, cy, cx + RADAR_R, cy, grid_dim);
    display_draw_line(fb, cx, cy - RADAR_R, cx, cy + RADAR_R, grid_dim);

    // 45 度对角虚点
    for (int r = 10; r < RADAR_R; r += 14) {
        int d = (int)((float)r * 0.707f);
        display_draw_pixel(fb, cx + d, cy + d, grid_dim);
        display_draw_pixel(fb, cx - d, cy + d, grid_dim);
        display_draw_pixel(fb, cx + d, cy - d, grid_dim);
        display_draw_pixel(fb, cx - d, cy - d, grid_dim);
    }

    // 3. 动态扩散脉冲声纳波
    if (pulse_radius > 5.0f) {
        uint8_t pulse_fade = (uint8_t)(90 * (1.0f - pulse_radius / (float)RADAR_R));
        draw_radar_circle(fb, cx, cy, (int)pulse_radius, rgb565(0, pulse_fade, (uint8_t)(pulse_fade * 0.8f)));
    }

    // 4. 旋转扫描光束与拖尾余辉
    for (int trail = 0; trail < 18; trail++) {
        float a = sweep_angle - (float)trail * 0.025f;
        int sx = cx + (int)((float)RADAR_R * fast_cos(a));
        int sy = cy + (int)((float)RADAR_R * fast_sin(a));

        if (trail == 0) {
            // 主扫描明线
            display_draw_line(fb, cx, cy, sx, sy, beam_col);
        } else {
            // 荧光余辉渐暗衰减扇区
            uint8_t glow = (uint8_t)(140 * (18 - trail) / 18);
            display_draw_line(fb, cx, cy, sx, sy, rgb565(0, glow, (uint8_t)(glow / 3)));
        }
    }

    // 5. 战术目标点与锁定跟踪框
    for (int i = 0; i < NUM_TARGETS; i++) {
        int tx = cx + (int)targets[i].x;
        int ty = cy + (int)targets[i].y;

        if (targets[i].alert_timer > 0.05f) {
            // 扫描扫过，明亮显现
            if (targets[i].locked) {
                // 红色警报敌对目标 + 方框锁定十字
                display_draw_box(fb, tx - 6, ty - 6, 13, 13, COLOR_RED);
                display_draw_pixel(fb, tx, ty, COLOR_WHITE);
                display_draw_line(fb, tx - 9, ty, tx - 6, ty, COLOR_RED);
                display_draw_line(fb, tx + 6, ty, tx + 9, ty, COLOR_RED);
                display_draw_string(fb, tx + 8, ty - 4, "LK", COLOR_RED, COLOR_BLACK, true);
            } else {
                // 绿色友军侦测目标点 + 光圈
                display_draw_pixel(fb, tx, ty, COLOR_WHITE);
                draw_radar_circle(fb, tx, ty, 4, COLOR_YELLOW);
            }
        } else {
            // 微弱暗影残留目标
            display_draw_pixel(fb, tx, ty, rgb565(0, 80, 40));
        }
    }

    // 6. 中心基座指示核
    display_fill_rect(fb, cx - 2, cy - 2, 5, 5, beam_col);
}

Effect fx_radar = {
    .id = "radar",
    .title = "» TACTICAL RADAR «",
    .subtitle = "赛博全息战术雷达与目标锁定",
    .init = radar_init,
    .update = radar_update,
    .render = radar_render,
    .cleanup = radar_cleanup
};
