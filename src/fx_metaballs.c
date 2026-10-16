#include "effects.h"
#include "fast_math.h"
#include <string.h>
#include <stdlib.h>

#define MB_W 120
#define MB_H 120
#define NUM_BALLS 4

typedef struct {
    float x, y;
    float vx, vy;
    int radius_sq;
} MetaBall;

static MetaBall balls[NUM_BALLS];

static void metaballs_init(void) {
    float speeds[NUM_BALLS][2] = {
        { 1.2f,  0.9f},
        {-1.1f,  1.3f},
        { 0.8f, -1.2f},
        {-0.9f, -0.8f}
    };
    int r[NUM_BALLS] = {14, 18, 15, 20};

    for (int i = 0; i < NUM_BALLS; i++) {
        balls[i].x = 30.0f + (float)(i * 20);
        balls[i].y = 30.0f + (float)(i * 18);
        balls[i].vx = speeds[i][0];
        balls[i].vy = speeds[i][1];
        balls[i].radius_sq = r[i] * r[i] * 320;
    }
}

static void metaballs_cleanup(void) {
}

static void metaballs_update(float dt) {
    (void)dt;
    for (int i = 0; i < NUM_BALLS; i++) {
        balls[i].x += balls[i].vx;
        balls[i].y += balls[i].vy;

        if (balls[i].x < 15.0f)  { balls[i].x = 15.0f;  balls[i].vx = -balls[i].vx; }
        if (balls[i].x > 105.0f) { balls[i].x = 105.0f; balls[i].vx = -balls[i].vx; }
        if (balls[i].y < 15.0f)  { balls[i].y = 15.0f;  balls[i].vy = -balls[i].vy; }
        if (balls[i].y > 105.0f) { balls[i].y = 105.0f; balls[i].vy = -balls[i].vy; }
    }
}

static void metaballs_render(uint16_t *fb) {
    int bx[NUM_BALLS], by[NUM_BALLS], br[NUM_BALLS];
    for (int i = 0; i < NUM_BALLS; i++) {
        bx[i] = (int)balls[i].x;
        by[i] = (int)balls[i].y;
        br[i] = balls[i].radius_sq;
    }

    for (int y = 0; y < MB_H; y++) {
        int sy = y * 2;
        int dy0 = y - by[0]; int dy0_sq = dy0 * dy0;
        int dy1 = y - by[1]; int dy1_sq = dy1 * dy1;
        int dy2 = y - by[2]; int dy2_sq = dy2 * dy2;
        int dy3 = y - by[3]; int dy3_sq = dy3 * dy3;

        for (int x = 0; x < MB_W; x++) {
            int dx0 = x - bx[0];
            int dx1 = x - bx[1];
            int dx2 = x - bx[2];
            int dx3 = x - bx[3];

            int d0 = dx0 * dx0 + dy0_sq + 8;
            int d1 = dx1 * dx1 + dy1_sq + 8;
            int d2 = dx2 * dx2 + dy2_sq + 8;
            int d3 = dx3 * dx3 + dy3_sq + 8;

            int sum = (br[0] / d0) + (br[1] / d1) + (br[2] / d2) + (br[3] / d3);

            uint16_t color;
            if (sum > 680) {
                // 核心炽白高光
                color = COLOR_WHITE;
            } else if (sum > 480) {
                // 内部高能流体 (金黄 ~ 橙红渐变)
                int t = sum - 480;
                color = rgb565(255, (uint8_t)(240 - t * 0.4f), (uint8_t)(t * 0.8f));
            } else if (sum > 300) {
                // 流体外缘 (霓虹荧光粉紫)
                int t = sum - 300;
                color = rgb565((uint8_t)(160 + t * 0.5f), 20, (uint8_t)(200 + t * 0.3f));
            } else if (sum > 180) {
                // 等离子外晕 (荧光青蓝)
                int t = sum - 180;
                color = rgb565(0, (uint8_t)(80 + t), (uint8_t)(150 + t * 0.8f));
            } else {
                // 背景微弱环境暗光
                color = COLOR_BLACK;
            }

            int sx = x * 2;
            uint32_t c32 = ((uint32_t)color << 16) | color;
            *(uint32_t *)&fb[sy * DISP_W + sx] = c32;
            *(uint32_t *)&fb[(sy + 1) * DISP_W + sx] = c32;
        }
    }
}

Effect fx_metaballs = {
    .id = "metaballs",
    .title = "» NEON METABALLS «",
    .subtitle = "霓虹流体熔岩双态融球",
    .init = metaballs_init,
    .update = metaballs_update,
    .render = metaballs_render,
    .cleanup = metaballs_cleanup
};
