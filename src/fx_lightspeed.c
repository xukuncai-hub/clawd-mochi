#include "effects.h"
#include "fast_math.h"
#include <string.h>
#include <stdlib.h>
#include <math.h>

#define NUM_RUNNERS 5
#define TRAIL_LEN 28

typedef struct {
    int x, y;
    int dir;
    int trail_x[TRAIL_LEN];
    int trail_y[TRAIL_LEN];
    float step_timer;
    uint16_t color;
} TronRunner;

static TronRunner runners[NUM_RUNNERS];
static float anim_time = 0.0f;

static void respawn_runner(TronRunner *r, int idx) {
    r->x = 25 + (idx % 3) * 80;
    r->y = 25 + (idx / 3) * 110;
    r->dir = idx % 4;
    r->step_timer = 0.0f;
    for (int i = 0; i < TRAIL_LEN; i++) {
        r->trail_x[i] = r->x;
        r->trail_y[i] = r->y;
    }
    if (idx == 0) r->color = rgb565(0, 240, 255);
    else if (idx == 1) r->color = rgb565(255, 40, 140);
    else if (idx == 2) r->color = rgb565(255, 230, 20);
    else if (idx == 3) r->color = rgb565(0, 255, 120);
    else r->color = rgb565(180, 80, 255);
}

static void lightspeed_init(void) {
    anim_time = 0.0f;
    for (int i = 0; i < NUM_RUNNERS; i++) {
        respawn_runner(&runners[i], i);
    }
}

static void lightspeed_cleanup(void) {
}

static void lightspeed_update(float dt) {
    anim_time += dt;

    int dx[4] = { 4, 0, -4, 0 };
    int dy[4] = { 0, 4, 0, -4 };

    for (int i = 0; i < NUM_RUNNERS; i++) {
        TronRunner *r = &runners[i];
        r->step_timer += dt;

        if (r->step_timer >= 0.03f) {
            r->step_timer = 0.0f;

            for (int t = TRAIL_LEN - 1; t > 0; t--) {
                r->trail_x[t] = r->trail_x[t - 1];
                r->trail_y[t] = r->trail_y[t - 1];
            }
            r->trail_x[0] = r->x;
            r->trail_y[0] = r->y;

            r->x += dx[r->dir];
            r->y += dy[r->dir];

            if (fast_rand() % 12 == 0 || r->x < 15 || r->x > DISP_W - 15 || r->y < 15 || r->y > DISP_H - 15) {
                int turn = (fast_rand() % 2 == 0) ? 1 : 3;
                r->dir = (r->dir + turn) % 4;

                if (r->x < 15) r->x = 15;
                if (r->x > DISP_W - 15) r->x = DISP_W - 15;
                if (r->y < 15) r->y = 15;
                if (r->y > DISP_H - 15) r->y = DISP_H - 15;
            }
        }
    }
}

static void lightspeed_render(uint16_t *fb) {
    memset(fb, 0, DISP_W * DISP_H * sizeof(uint16_t));

    // 1. 全屏高科技矩阵背景网格 (随时间微弱脉冲)
    uint8_t grid_val = (uint8_t)(15.0f + 10.0f * sinf(anim_time * 3.0f));
    uint16_t grid_col = rgb565(0, grid_val, (uint8_t)(grid_val * 1.5f));

    for (int x = 16; x < DISP_W; x += 16) {
        display_draw_line(fb, x, 0, x, DISP_H - 1, grid_col);
    }
    for (int y = 16; y < DISP_H; y += 16) {
        display_draw_line(fb, 0, y, DISP_W - 1, y, grid_col);
    }

    // 2. 绘制 5 组多色高速 Tron 光墙 (带双层粗光感与白热端头)
    for (int i = 0; i < NUM_RUNNERS; i++) {
        TronRunner *r = &runners[i];

        for (int t = 0; t < TRAIL_LEN - 1; t++) {
            int x1 = r->trail_x[t];
            int y1 = r->trail_y[t];
            int x2 = r->trail_x[t + 1];
            int y2 = r->trail_y[t + 1];

            display_draw_line(fb, x1, y1, x2, y2, r->color);
            if (t < TRAIL_LEN / 2) {
                // 前半段加粗加亮
                display_draw_line(fb, x1 + 1, y1, x2 + 1, y2, r->color);
            }
        }

        // 光速探针核心与高能火花
        display_fill_circle(fb, r->x, r->y, 3, COLOR_WHITE);
        display_draw_circle(fb, r->x, r->y, 6, r->color);
    }
}

Effect fx_lightspeed = {
    .id = "lightspeed",
    .title = "赛博光轨穿梭",
    .subtitle = "全息Tron电子网格高速光子流光墙",
    .init = lightspeed_init,
    .update = lightspeed_update,
    .render = lightspeed_render,
    .cleanup = lightspeed_cleanup
};
