#include "effects.h"
#include "fast_math.h"
#include <string.h>
#include <stdlib.h>

#define NUM_SNOW 35
#define NUM_STARS 50

typedef struct {
    float x, y;
    float speed;
} SnowFlake;

typedef struct {
    int16_t x, y;
    uint8_t bright;
} SkyStar;

static SnowFlake snowflakes[NUM_SNOW];
static SkyStar stars[NUM_STARS];
static uint8_t mountain_h[DISP_W];
static float aurora_t1 = 0.0f;
static float aurora_t2 = 0.0f;
static float aurora_t3 = 0.0f;

static void aurora_init(void) {
    aurora_t1 = 0.0f;
    aurora_t2 = 0.0f;
    aurora_t3 = 0.0f;

    // 预生成山脉剪影轮廓 (分形多频叠加，永久固定)
    for (int x = 0; x < DISP_W; x++) {
        float fx = (float)x;
        float h = 150.0f + 25.0f * fast_sin(fx * 0.022f) 
                         + 14.0f * fast_cos(fx * 0.055f + 1.2f)
                         + 6.0f * fast_sin(fx * 0.12f);
        if (h < 130.0f) h = 130.0f;
        if (h > 185.0f) h = 185.0f;
        mountain_h[x] = (uint8_t)h;
    }

    // 初始化背景繁星
    for (int i = 0; i < NUM_STARS; i++) {
        stars[i].x = fast_rand() % DISP_W;
        stars[i].y = fast_rand() % 140;
        stars[i].bright = 100 + (fast_rand() % 155);
    }

    // 初始化飘落雪花
    for (int i = 0; i < NUM_SNOW; i++) {
        snowflakes[i].x = (float)(fast_rand() % DISP_W);
        snowflakes[i].y = (float)(fast_rand() % DISP_H);
        snowflakes[i].speed = 0.4f + (float)(fast_rand() % 100) / 100.0f;
    }
}

static void aurora_cleanup(void) {
}

static void aurora_update(float dt) {
    (void)dt;
    aurora_t1 += 0.024f;
    aurora_t2 += 0.017f;
    aurora_t3 += 0.031f;

    if (aurora_t1 > 2.0f * FAST_PI) aurora_t1 -= 2.0f * FAST_PI;
    if (aurora_t2 > 2.0f * FAST_PI) aurora_t2 -= 2.0f * FAST_PI;
    if (aurora_t3 > 2.0f * FAST_PI) aurora_t3 -= 2.0f * FAST_PI;

    for (int i = 0; i < NUM_SNOW; i++) {
        snowflakes[i].y += snowflakes[i].speed;
        snowflakes[i].x += 0.15f * fast_sin(snowflakes[i].y * 0.08f + aurora_t1);
        if (snowflakes[i].y >= (float)DISP_H) {
            snowflakes[i].y = 0.0f;
            snowflakes[i].x = (float)(fast_rand() % DISP_W);
        }
    }
}

static void aurora_render(uint16_t *fb) {
    memset(fb, 0, DISP_W * DISP_H * sizeof(uint16_t));

    // 1. 绘制深空背景微光与繁星
    for (int i = 0; i < NUM_STARS; i++) {
        uint8_t b = stars[i].bright;
        fb[stars[i].y * DISP_W + stars[i].x] = rgb565(b, b, (uint8_t)(b * 1.15f > 255 ? 255 : b * 1.15f));
    }

    // 2. 绘制多层波浪极光幕布 (翡翠绿、青蓝与荧光紫光幕流动)
    for (int x = 0; x < DISP_W; x++) {
        float fx = (float)x;

        // 三道极光幕的主峰高度
        int y_g = (int)(55.0f + 26.0f * fast_sin(fx * 0.028f + aurora_t1) + 12.0f * fast_cos(fx * 0.065f + aurora_t2));
        int y_b = (int)(75.0f + 22.0f * fast_sin(fx * 0.021f + aurora_t2) + 14.0f * fast_sin(fx * 0.052f + aurora_t3));
        int y_p = (int)(95.0f + 18.0f * fast_cos(fx * 0.033f + aurora_t3) + 10.0f * fast_sin(fx * 0.075f + aurora_t1));

        int m_lim = mountain_h[x];

        // 绿色主极光幕向下垂直光柱渐变
        for (int dy = -18; dy <= 45; dy++) {
            int py = y_g + dy;
            if (py >= 0 && py < m_lim) {
                int intensity = 45 - abs(dy);
                if (intensity > 0) {
                    uint8_t g = (uint8_t)(intensity * 5);
                    uint8_t b = (uint8_t)(intensity * 3);
                    uint16_t col = rgb565(0, g, b);
                    fb[py * DISP_W + x] |= col;
                }
            }
        }

        // 紫粉高空极光弧
        for (int dy = -15; dy <= 30; dy++) {
            int py = y_p + dy;
            if (py >= 0 && py < m_lim) {
                int intensity = 30 - abs(dy);
                if (intensity > 0) {
                    uint8_t r = (uint8_t)(intensity * 4);
                    uint8_t b = (uint8_t)(intensity * 6);
                    uint16_t col = rgb565(r, 10, b);
                    fb[py * DISP_W + x] |= col;
                }
            }
        }

        // 青蓝核心光带
        for (int dy = -8; dy <= 18; dy++) {
            int py = y_b + dy;
            if (py >= 0 && py < m_lim) {
                int intensity = 18 - abs(dy);
                if (intensity > 0) {
                    uint8_t g = (uint8_t)(intensity * 8);
                    uint8_t b = (uint8_t)(intensity * 12);
                    fb[py * DISP_W + x] |= rgb565(0, g, b);
                }
            }
        }
    }

    // 3. 绘制深黑冷峻山脉与白雪峰顶剪影
    for (int x = 0; x < DISP_W; x++) {
        int m_y = mountain_h[x];

        // 冰雪微光山脊线
        fb[m_y * DISP_W + x] = rgb565(140, 180, 210);
        if (m_y + 1 < DISP_H) fb[(m_y + 1) * DISP_W + x] = rgb565(70, 100, 140);

        // 山体内部渐变暗夜深黑
        for (int y = m_y + 2; y < 192; y++) {
            int d = y - m_y;
            uint8_t v = (uint8_t)((18 - d) > 4 ? (18 - d) : 4);
            fb[y * DISP_W + x] = rgb565(v / 2, v, (uint8_t)(v * 1.5f));
        }
    }

    // 4. 底部冰湖镜面倒影与水波潋滟
    const int lake_y = 192;
    for (int y = lake_y; y < DISP_H; y++) {
        int dy = y - lake_y;
        int src_y = lake_y - 1 - (dy / 2); // 镜像采样
        if (src_y < 0) src_y = 0;

        float ripple = fast_sin((float)y * 0.45f + aurora_t3 * 3.0f);
        int offset_x = (int)(ripple * 2.5f);

        for (int x = 0; x < DISP_W; x++) {
            int sx = x + offset_x;
            if (sx < 0) sx = 0; else if (sx >= DISP_W) sx = DISP_W - 1;

            uint16_t src_col = fb[src_y * DISP_W + sx];
            // 冰水暗化与深青色调制
            uint8_t r = (uint8_t)(((src_col >> 11) & 0x1F) * 3);
            uint8_t g = (uint8_t)(((src_col >> 5) & 0x3F) * 3);
            uint8_t b = (uint8_t)(((src_col & 0x1F) * 4) + 10);
            fb[y * DISP_W + x] = rgb565(r, g, b);
        }
    }

    // 5. 飘落冰晶雪花
    for (int i = 0; i < NUM_SNOW; i++) {
        int sx = (int)snowflakes[i].x;
        int sy = (int)snowflakes[i].y;
        if (sx >= 0 && sx < DISP_W && sy >= 0 && sy < DISP_H) {
            fb[sy * DISP_W + sx] = COLOR_WHITE;
        }
    }
}

Effect fx_aurora = {
    .id = "aurora",
    .title = "» ARCTIC AURORA «",
    .subtitle = "极地深空极光与雪山倒影",
    .init = aurora_init,
    .update = aurora_update,
    .render = aurora_render,
    .cleanup = aurora_cleanup
};
