#include "effects.h"
#include "fast_math.h"
#include <stdlib.h>
#include <string.h>

#define FIRE_W 120
#define FIRE_H 120
#define PALETTE_SIZE 37

static uint8_t fire_pixels[FIRE_W * FIRE_H];
static uint16_t fire_palette[PALETTE_SIZE];

static const uint8_t doom_rgb[PALETTE_SIZE][3] = {
    {  7,   7,   7}, { 31,   7,   7}, { 47,  15,   7}, { 71,  15,   7},
    { 87,  23,   7}, {103,  31,   7}, {119,  31,   7}, {143,  39,   7},
    {159,  47,   7}, {175,  63,   7}, {191,  71,   7}, {199,  71,   7},
    {223,  79,   7}, {223,  87,   7}, {223,  87,   7}, {215,  95,   7},
    {215,  95,   7}, {215, 103,  15}, {207, 111,  15}, {207, 119,  15},
    {207, 127,  15}, {207, 135,  23}, {199, 135,  23}, {199, 143,  23},
    {199, 151,  31}, {191, 159,  31}, {191, 159,  31}, {191, 167,  39},
    {191, 167,  39}, {191, 175,  47}, {183, 175,  47}, {183, 183,  55},
    {207, 207, 111}, {223, 223, 159}, {239, 239, 199}, {255, 255, 255},
    {255, 255, 255}
};

static void fire_init(void) {
    memset(fire_pixels, 0, sizeof(fire_pixels));
    // 预热调色板
    for (int i = 0; i < PALETTE_SIZE; i++) {
        fire_palette[i] = rgb565(doom_rgb[i][0], doom_rgb[i][1], doom_rgb[i][2]);
    }
    // 初始化底部火源
    for (int x = 0; x < FIRE_W; x++) {
        fire_pixels[(FIRE_H - 1) * FIRE_W + x] = 36;
        fire_pixels[(FIRE_H - 2) * FIRE_W + x] = 36;
    }
}

static void fire_cleanup(void) {
}

static void fire_update(float dt) {
    (void)dt;

    // 维持底部火源与自然跳动
    for (int x = 0; x < FIRE_W; x++) {
        fire_pixels[(FIRE_H - 1) * FIRE_W + x] = 36;
        if ((fast_rand() & 15) < 13) {
            fire_pixels[(FIRE_H - 2) * FIRE_W + x] = 34 + (fast_rand() % 3);
        }
    }

    // 极速火焰向上扩散与衰减算法
    for (int x = 0; x < FIRE_W; x++) {
        for (int y = 1; y < FIRE_H; y++) {
            int src = y * FIRE_W + x;
            int pixel = fire_pixels[src];

            if (pixel == 0) {
                if (src >= FIRE_W) {
                    fire_pixels[src - FIRE_W] = 0;
                }
            } else {
                uint32_t r = fast_rand();
                int rand_decay = r & 3;
                int decay = rand_decay & 1;
                int dst_x = x - rand_decay + 1;
                int dst_y = y - 1;

                if (dst_x >= 0 && dst_x < FIRE_W && dst_y >= 0 && dst_y < FIRE_H) {
                    int dst = dst_y * FIRE_W + dst_x;
                    fire_pixels[dst] = (pixel > decay) ? (pixel - decay) : 0;
                }
            }
        }
    }
}

static void fire_render(uint16_t *fb) {
    for (int y = 0; y < FIRE_H; y++) {
        int sy = y * 2;
        int row_offset = y * FIRE_W;
        for (int x = 0; x < FIRE_W; x++) {
            uint8_t heat = fire_pixels[row_offset + x];
            if (heat >= PALETTE_SIZE) heat = PALETTE_SIZE - 1;
            uint16_t color = fire_palette[heat];

            // 2x 缩放极速 32 位成对写入
            int sx = x * 2;
            uint32_t c32 = ((uint32_t)color << 16) | color;
            *(uint32_t *)&fb[sy * DISP_W + sx] = c32;
            *(uint32_t *)&fb[(sy + 1) * DISP_W + sx] = c32;
        }
    }
}

Effect fx_fire = {
    .id = "fire",
    .title = "» DOOM INFERNO FIRE «",
    .subtitle = "经典毁灭战士热量流体火焰",
    .init = fire_init,
    .update = fire_update,
    .render = fire_render,
    .cleanup = fire_cleanup
};
