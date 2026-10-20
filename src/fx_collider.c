#include "effects.h"
#include "fast_math.h"
#include <string.h>
#include <math.h>

#define COLLIDER_SEGS 20
#define NUM_COLLIDE_SPARKS 36

typedef struct {
    float x, y;
    float vx, vy;
    float life;
    uint16_t color;
} CollideSpark;

static float beam_a1 = 0.0f;
static float beam_a2 = 0.0f;
static float shock_r = 0.0f;
static float anim_time = 0.0f;
static CollideSpark sparks[NUM_COLLIDE_SPARKS];

static void collider_init(void) {
    beam_a1 = 0.0f;
    beam_a2 = FAST_PI;
    shock_r = 0.0f;
    anim_time = 0.0f;
    for (int i = 0; i < NUM_COLLIDE_SPARKS; i++) sparks[i].life = 0.0f;
}

static void collider_cleanup(void) {
}

static void trigger_boom(float x, float y) {
    shock_r = 1.0f;
    for (int i = 0; i < NUM_COLLIDE_SPARKS; i++) {
        float a = (float)(fast_rand() % 360) * (FAST_PI / 180.0f);
        float spd = 50.0f + (float)(fast_rand() % 120);
        sparks[i].x = x;
        sparks[i].y = y;
        sparks[i].vx = cosf(a) * spd;
        sparks[i].vy = sinf(a) * spd;
        sparks[i].life = 0.7f + (float)(fast_rand() % 30) * 0.01f;
        sparks[i].color = (i % 2 == 0) ? rgb565(255, 240, 80) : COLOR_WHITE;
    }
}

static void collider_update(float dt) {
    anim_time += dt;

    float prev1 = beam_a1;
    beam_a1 += dt * 6.5f;
    beam_a2 -= dt * 6.5f;

    // 对撞触发
    if (sinf(prev1) * sinf(beam_a1) <= 0.0f && cosf(beam_a1) > 0.0f) {
        trigger_boom((float)(DISP_W / 2 + 76), (float)(DISP_H / 2));
    } else if (sinf(prev1) * sinf(beam_a1) <= 0.0f && cosf(beam_a1) < 0.0f) {
        trigger_boom((float)(DISP_W / 2 - 76), (float)(DISP_H / 2));
    }

    if (shock_r > 0.0f) {
        shock_r -= dt * 2.2f;
        if (shock_r < 0.0f) shock_r = 0.0f;
    }

    for (int i = 0; i < NUM_COLLIDE_SPARKS; i++) {
        if (sparks[i].life > 0.0f) {
            sparks[i].life -= dt;
            sparks[i].x += sparks[i].vx * dt;
            sparks[i].y += sparks[i].vy * dt;
        }
    }
}

static void collider_render(uint16_t *fb) {
    memset(fb, 0, DISP_W * DISP_H * sizeof(uint16_t));
    int cx = DISP_W / 2;
    int cy = DISP_H / 2;

    float R = 76.0f;
    float r_tube = 14.0f;

    // 1. 绘制 3D 环形磁约束真空管道肋骨网格 (Torus Vacuum Chamber Mesh)
    for (int i = 0; i < COLLIDER_SEGS; i++) {
        float a = (float)i * (2.0f * FAST_PI / (float)COLLIDER_SEGS);
        float cos_a = cosf(a), sin_a = sinf(a);

        int ox = cx + (int)(R * cos_a);
        int oy = cy + (int)(R * sin_a);

        // 磁轭肋骨线框
        int in_x = cx + (int)((R - r_tube) * cos_a);
        int in_y = cy + (int)((R - r_tube) * sin_a);
        int out_x = cx + (int)((R + r_tube) * cos_a);
        int out_y = cy + (int)((R + r_tube) * sin_a);

        display_draw_line(fb, in_x, in_y, out_x, out_y, rgb565(0, 70, 100));
        display_fill_circle(fb, out_x, out_y, 2, rgb565(0, 180, 240));

        // 环向连续外壳
        float next_a = (float)(i + 1) * (2.0f * FAST_PI / (float)COLLIDER_SEGS);
        int next_out_x = cx + (int)((R + r_tube) * cosf(next_a));
        int next_out_y = cy + (int)((R + r_tube) * sinf(next_a));
        display_draw_line(fb, out_x, out_y, next_out_x, next_out_y, rgb565(0, 120, 160));

        int next_in_x = cx + (int)((R - r_tube) * cosf(next_a));
        int next_in_y = cy + (int)((R - r_tube) * sinf(next_a));
        display_draw_line(fb, in_x, in_y, next_in_x, next_in_y, rgb565(0, 120, 160));
    }

    // 2. 顺时针高速强子流 (青绿流光拖尾)
    for (int t = 0; t < 16; t++) {
        float a = beam_a1 - (float)t * 0.05f;
        int px = cx + (int)(R * cosf(a));
        int py = cy + (int)(R * sinf(a));
        uint8_t val = (uint8_t)(255 - t * 15);
        display_draw_pixel(fb, px, py, (t == 0) ? COLOR_WHITE : rgb565(0, val, val));
        display_draw_pixel(fb, px + 1, py, (t == 0) ? COLOR_WHITE : rgb565(0, val, val));
    }

    // 3. 逆时针高速强子流 (品红流光拖尾)
    for (int t = 0; t < 16; t++) {
        float a = beam_a2 + (float)t * 0.05f;
        int px = cx + (int)(R * cosf(a));
        int py = cy + (int)(R * sinf(a));
        uint8_t val = (uint8_t)(255 - t * 15);
        display_draw_pixel(fb, px, py, (t == 0) ? COLOR_WHITE : rgb565(val, 0, val));
        display_draw_pixel(fb, px + 1, py, (t == 0) ? COLOR_WHITE : rgb565(val, 0, val));
    }

    // 4. 碰撞撕裂冲击波与希格斯粒子碎片
    if (shock_r > 0.0f) {
        int r_wave = (int)((1.0f - shock_r) * 55.0f);
        display_draw_circle(fb, cx + (int)R, cy, r_wave, rgb565(255, 220, 60));
        display_draw_circle(fb, cx - (int)R, cy, r_wave, rgb565(255, 220, 60));
    }

    for (int i = 0; i < NUM_COLLIDE_SPARKS; i++) {
        if (sparks[i].life > 0.0f) {
            int sx = (int)sparks[i].x;
            int sy = (int)sparks[i].y;
            if (sx >= 0 && sx < DISP_W && sy >= 0 && sy < DISP_H) {
                display_draw_pixel(fb, sx, sy, sparks[i].color);
                display_draw_pixel(fb, sx + 1, sy, sparks[i].color);
            }
        }
    }

    // 5. 中央全息探测器多层同心刻度
    display_draw_circle(fb, cx, cy, 32, rgb565(0, 150, 200));
    display_draw_circle(fb, cx, cy, 18, rgb565(0, 240, 255));
    display_fill_circle(fb, cx, cy, 4, COLOR_WHITE);
}

Effect fx_collider = {
    .id = "collider",
    .title = "赛博强子对撞机",
    .subtitle = "3D真空管道超导磁约束与高能碰撞激波",
    .init = collider_init,
    .update = collider_update,
    .render = collider_render,
    .cleanup = collider_cleanup
};
