#include "effects.h"
#include "fast_math.h"
#include <string.h>
#include <math.h>

#define NUM_PLANETS 4

typedef struct {
    float orbit_r;
    float planet_r;
    int teeth;
    float angle;
    float speed;
    uint16_t color;
} PlanetGear;

static float anim_time = 0.0f;
static PlanetGear planets[NUM_PLANETS];

static void clockwork_init(void) {
    anim_time = 0.0f;
    for (int i = 0; i < NUM_PLANETS; i++) {
        planets[i].orbit_r = 56.0f;
        planets[i].planet_r = 18.0f;
        planets[i].teeth = 12;
        planets[i].angle = (float)i * (2.0f * FAST_PI / (float)NUM_PLANETS);
        planets[i].speed = 0.7f;
        if (i == 0) planets[i].color = rgb565(0, 240, 255);
        else if (i == 1) planets[i].color = rgb565(255, 60, 180);
        else if (i == 2) planets[i].color = rgb565(255, 220, 30);
        else planets[i].color = rgb565(0, 255, 140);
    }
}

static void clockwork_cleanup(void) {
}

static void clockwork_update(float dt) {
    anim_time += dt;
}

static void draw_gear(uint16_t *fb, int cx, int cy, float r, int teeth, float rot, uint16_t col) {
    int pts = teeth * 4;
    int last_x = 0, last_y = 0;
    int first_x = 0, first_y = 0;

    for (int i = 0; i < pts; i++) {
        float a = rot + (float)i * (2.0f * FAST_PI / (float)pts);
        float cur_r = ((i % 4 == 1 || i % 4 == 2) ? (r + 4.0f) : (r - 2.0f));
        int x = cx + (int)(cur_r * cosf(a));
        int y = cy + (int)(cur_r * sinf(a));

        if (i == 0) {
            first_x = x;
            first_y = y;
        } else {
            display_draw_line(fb, last_x, last_y, x, y, col);
        }
        last_x = x;
        last_y = y;
    }
    display_draw_line(fb, last_x, last_y, first_x, first_y, col);
    display_draw_circle(fb, cx, cy, (int)(r * 0.45f), col);
    display_fill_circle(fb, cx, cy, 3, COLOR_WHITE);
}

static void clockwork_render(uint16_t *fb) {
    memset(fb, 0, DISP_W * DISP_H * sizeof(uint16_t));
    int cx = DISP_W / 2;
    int cy = DISP_H / 2;

    // 1. 最外层 60 秒精密机械日历刻度环
    display_draw_circle(fb, cx, cy, 108, rgb565(0, 70, 100));
    display_draw_circle(fb, cx, cy, 104, rgb565(0, 150, 200));
    for (int i = 0; i < 60; i++) {
        float a = (float)i * (2.0f * FAST_PI / 60.0f);
        int r_in = (i % 5 == 0) ? 95 : 99;
        int x1 = cx + (int)(r_in * cosf(a));
        int y1 = cy + (int)(r_in * sinf(a));
        int x2 = cx + (int)(104.0f * cosf(a));
        int y2 = cy + (int)(104.0f * sinf(a));
        display_draw_line(fb, x1, y1, x2, y2, (i % 5 == 0) ? COLOR_WHITE : rgb565(0, 130, 170));
    }

    // 2. 居中主太阳齿轮 (20齿，顺时针自转)
    float sun_rot = anim_time * 0.85f;
    draw_gear(fb, cx, cy, 34.0f, 18, sun_rot, rgb565(0, 220, 255));

    // 3. 围绕公转的 4 组行星齿轮 (带连杆支架网格)
    for (int i = 0; i < NUM_PLANETS; i++) {
        float orbit_a = planets[i].angle + anim_time * planets[i].speed;
        int px = cx + (int)(planets[i].orbit_r * cosf(orbit_a));
        int py = cy + (int)(planets[i].orbit_r * sinf(orbit_a));

        float self_rot = -orbit_a * (34.0f / planets[i].planet_r);
        draw_gear(fb, px, py, planets[i].planet_r, planets[i].teeth, self_rot, planets[i].color);

        // 连杆支架
        display_draw_line(fb, cx, cy, px, py, rgb565(0, 80, 120));

        // 4 个行星齿轮相互连线构成四边形动力笼
        int next_i = (i + 1) % NUM_PLANETS;
        float next_a = planets[next_i].angle + anim_time * planets[next_i].speed;
        int npx = cx + (int)(planets[next_i].orbit_r * cosf(next_a));
        int npy = cy + (int)(planets[next_i].orbit_r * sinf(next_a));
        display_draw_line(fb, px, py, npx, npy, rgb565(0, 60, 90));
    }

    // 4. 陀飞轮机械擒纵秒针脉冲
    float sec_rot = anim_time * 2.8f;
    int sx = cx + (int)(95.0f * cosf(sec_rot));
    int sy = cy + (int)(95.0f * sinf(sec_rot));
    display_draw_line(fb, cx, cy, sx, sy, rgb565(255, 50, 70));
    display_fill_circle(fb, sx, sy, 3, COLOR_WHITE);
}

Effect fx_clockwork = {
    .id = "clockwork",
    .title = "赛博机械时钟齿轮",
    .subtitle = "陀飞轮四行星齿轮差速动力笼与精密刻度",
    .init = clockwork_init,
    .update = clockwork_update,
    .render = clockwork_render,
    .cleanup = clockwork_cleanup
};
