#include "effects.h"
#include "fast_math.h"
#include <string.h>
#include <math.h>

#define MAX_BOLTS 4
#define MAX_SEGS 14

typedef struct {
    int x1, y1, x2, y2;
} ArcSeg;

typedef struct {
    ArcSeg segs[MAX_SEGS];
    int count;
    float life;
    uint16_t col;
} TeslaArc;

static TeslaArc arcs[MAX_BOLTS];
static float anim_time = 0.0f;

static void spawn_arc(TeslaArc *a, int sx, int sy, float angle, float total_len, uint16_t c) {
    a->count = 0;
    a->life = 0.14f + (float)(fast_rand() % 8) * 0.01f;
    a->col = c;

    int cx = sx;
    int cy = sy;
    float cur_a = angle;
    int num_s = 8 + (fast_rand() % (MAX_SEGS - 8));
    float s_len = total_len / (float)num_s;

    for (int i = 0; i < num_s && a->count < MAX_SEGS; i++) {
        cur_a += ((float)(fast_rand() % 81) - 40.0f) * (FAST_PI / 180.0f);
        int nx = cx + (int)(s_len * cosf(cur_a));
        int ny = cy + (int)(s_len * sinf(cur_a));

        a->segs[a->count].x1 = cx;
        a->segs[a->count].y1 = cy;
        a->segs[a->count].x2 = nx;
        a->segs[a->count].y2 = ny;
        a->count++;

        cx = nx;
        cy = ny;
    }
}

static void lightning_init(void) {
    anim_time = 0.0f;
    for (int i = 0; i < MAX_BOLTS; i++) arcs[i].life = 0.0f;
}

static void lightning_cleanup(void) {
}

static void lightning_update(float dt) {
    anim_time += dt;
    int cx = DISP_W / 2;
    int cy = DISP_H / 2;

    for (int i = 0; i < MAX_BOLTS; i++) {
        if (arcs[i].life > 0.0f) {
            arcs[i].life -= dt;
        } else {
            float angle = (float)(fast_rand() % 360) * (FAST_PI / 180.0f);
            float len = 75.0f + (float)(fast_rand() % 35);
            uint16_t col = (i % 2 == 0) ? rgb565(0, 240, 255) : rgb565(200, 80, 255);
            spawn_arc(&arcs[i], cx, cy, angle, len, col);
        }
    }
}

static void lightning_render(uint16_t *fb) {
    memset(fb, 0, DISP_W * DISP_H * sizeof(uint16_t));
    int cx = DISP_W / 2;
    int cy = DISP_H / 2;

    // 1. 外部法拉第笼防护网格 (Faraday Cage Mesh)
    display_draw_circle(fb, cx, cy, 108, rgb565(0, 70, 110));
    display_draw_circle(fb, cx, cy, 104, rgb565(0, 140, 190));
    for (int i = 0; i < 32; i++) {
        float th = (float)i * (2.0f * FAST_PI / 32.0f);
        int px1 = cx + (int)(100.0f * cosf(th));
        int py1 = cy + (int)(100.0f * sinf(th));
        int px2 = cx + (int)(108.0f * cosf(th));
        int py2 = cy + (int)(108.0f * sinf(th));
        display_draw_line(fb, px1, py1, px2, py2, (i % 4 == 0) ? COLOR_WHITE : rgb565(0, 100, 150));
    }

    // 2. 绘制分形高压电弧树
    for (int i = 0; i < MAX_BOLTS; i++) {
        if (arcs[i].life <= 0.0f) continue;

        for (int s = 0; s < arcs[i].count; s++) {
            int x1 = arcs[i].segs[s].x1;
            int y1 = arcs[i].segs[s].y1;
            int x2 = arcs[i].segs[s].x2;
            int y2 = arcs[i].segs[s].y2;

            // 电弧双层高光
            display_draw_line(fb, x1, y1, x2, y2, arcs[i].col);
            display_draw_line(fb, x1 + 1, y1, x2 + 1, y2, COLOR_WHITE);
        }

        // 击中末端等离子火花球
        int last = arcs[i].count - 1;
        if (last >= 0) {
            display_fill_circle(fb, arcs[i].segs[last].x2, arcs[i].segs[last].y2, 2, COLOR_WHITE);
        }
    }

    // 3. 中央高压特斯拉放电等离子球 (能量脉冲与高光核)
    float pulse = sinf(anim_time * 8.0f);
    int core_r = (int)(18.0f + pulse * 2.5f);
    display_fill_circle(fb, cx, cy, core_r + 4, rgb565(0, 90, 150));
    display_fill_circle(fb, cx, cy, core_r, rgb565(0, 240, 255));
    display_fill_circle(fb, cx, cy, core_r / 2, COLOR_WHITE);

    // 同心脉冲电晕
    float corona_r = fmodf(anim_time * 50.0f, 95.0f);
    display_draw_circle(fb, cx, cy, (int)corona_r, rgb565(0, 100, 160));
}

Effect fx_lightning = {
    .id = "lightning",
    .title = "赛博特斯拉电弧",
    .subtitle = "高压分形放电网络与法拉第防护网格",
    .init = lightning_init,
    .update = lightning_update,
    .render = lightning_render,
    .cleanup = lightning_cleanup
};
