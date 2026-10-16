#include "effects.h"
#include "fast_math.h"
#include <string.h>
#include <stdlib.h>

#define PLASMA_W 120
#define PLASMA_H 120

static float t1 = 0.0f, t2 = 0.0f, t3 = 0.0f, t4 = 0.0f;
static uint16_t plasma_palette[256];
static uint8_t dist_phase[61][61];
static int8_t sin_i8[256];

static void plasma_init(void) {
    // 构造高饱和度绚丽等离子电浆色环表
    for (int i = 0; i < 256; i++) {
        float f = (float)i / 256.0f;
        uint8_t r = (uint8_t)(128.0f + 127.0f * fast_sin(f * 2.0f * FAST_PI));
        uint8_t g = (uint8_t)(128.0f + 127.0f * fast_sin(f * 2.0f * FAST_PI + 2.094f));
        uint8_t b = (uint8_t)(128.0f + 127.0f * fast_sin(f * 2.0f * FAST_PI + 4.188f));
        plasma_palette[i] = rgb565(r, g, b);

        // 整数正弦表 [-31, +31]
        sin_i8[i] = (int8_t)(31.0f * sinf(f * 2.0f * FAST_PI));
    }

    // 预计算四分之一对称径向距离表 (彻底消除每帧 14,400 次极慢的软浮点 sqrtf)
    for (int dy = 0; dy <= 60; dy++) {
        for (int dx = 0; dx <= 60; dx++) {
            float dist = sqrtf((float)(dx * dx + dy * dy));
            dist_phase[dy][dx] = (uint8_t)(fmodf(dist * 0.10f / (2.0f * FAST_PI), 1.0f) * 256.0f);
        }
    }
}

static void plasma_cleanup(void) {
}

static void plasma_update(float dt) {
    (void)dt;
    t1 += 0.045f;
    t2 += 0.038f;
    t3 += 0.052f;
    t4 += 0.031f;
}

static void plasma_render(uint16_t *fb) {
    // 1. 预计算 1D 扫描线缓存，消除内层循环重复三角计算
    int8_t s1_buf[PLASMA_W];
    for (int x = 0; x < PLASMA_W; x++) {
        int idx = (int)(( (float)x * 0.08f + t1 ) * (256.0f / (2.0f * FAST_PI))) & 255;
        s1_buf[x] = sin_i8[idx];
    }

    int8_t s3_buf[PLASMA_W + PLASMA_H];
    for (int xy = 0; xy < PLASMA_W + PLASMA_H; xy++) {
        int idx = (int)(( (float)xy * 0.05f + t3 ) * (256.0f / (2.0f * FAST_PI))) & 255;
        s3_buf[xy] = sin_i8[idx];
    }

    uint8_t t4_phase = (uint8_t)((t4 / (2.0f * FAST_PI)) * 256.0f);

    // 2. 纯定点数超高速内层渲染循环 (无软浮点，无 sqrtf，单帧耗时从 140ms 降至 <1ms)
    for (int y = 0; y < PLASMA_H; y++) {
        int y_idx = (int)(( (float)y * 0.08f + t2 ) * (256.0f / (2.0f * FAST_PI))) & 255;
        int s2 = sin_i8[y_idx];
        int sy = y * 2;
        int ady = abs(y - 60);
        if (ady > 60) ady = 60;
        const uint8_t *dist_row = dist_phase[ady];

        for (int x = 0; x < PLASMA_W; x++) {
            int adx = abs(x - 60);
            if (adx > 60) adx = 60;

            uint8_t r_idx = (dist_row[adx] - t4_phase) & 255;
            int s4 = sin_i8[r_idx];

            int val = s1_buf[x] + s2 + s3_buf[x + y] + s4 + 128;
            if (val < 0) val = 0;
            else if (val > 255) val = 255;

            uint16_t color = plasma_palette[(uint8_t)val];

            // 2x2 成对 32 位写入
            int sx = x * 2;
            uint32_t c32 = ((uint32_t)color << 16) | color;
            *(uint32_t *)&fb[sy * DISP_W + sx] = c32;
            *(uint32_t *)&fb[(sy + 1) * DISP_W + sx] = c32;
        }
    }
}

Effect fx_plasma = {
    .id = "plasma",
    .title = "» RETRO PLASMA WAVE «",
    .subtitle = "迷幻复古彩虹等离子电浆流光",
    .init = plasma_init,
    .update = plasma_update,
    .render = plasma_render,
    .cleanup = plasma_cleanup
};
