#include "effects.h"
#include "fast_math.h"
#include <string.h>
#include <stdlib.h>

#define TUNNEL_W 120
#define TUNNEL_H 120

// 采用四分之一象限对称静态查找表 (仅 7.4 KB 静态 BSS，彻底杜绝 malloc 堆内存分配失败导致的黑屏卡顿)
static uint8_t s_dist[61][61];
static uint8_t s_angle[61][61];
static uint16_t tunnel_palette[256];
static bool s_initialized = false;

static float shift_z = 0.0f;
static float shift_a = 0.0f;

static void tunnel_init(void) {
    if (!s_initialized) {
        // 预计算第一象限距离与极角表 (0 <= dx <= 60, 0 <= dy <= 60)
        for (int dy = 0; dy <= 60; dy++) {
            float fdy = (float)dy;
            for (int dx = 0; dx <= 60; dx++) {
                float fdx = (float)dx;
                float dist = sqrtf(fdx * fdx + fdy * fdy);
                float angle = atan2f(fdy, fdx); // 0 ~ PI/2

                int d_val = (dist > 0.5f) ? (int)(1800.0f / dist) : 255;
                if (d_val > 255) d_val = 255;
                s_dist[dy][dx] = (uint8_t)d_val;

                // 映射 [0, PI/2] 到 [0, 64]
                int a_val = (int)((angle / (2.0f * FAST_PI)) * 256.0f);
                if (a_val > 64) a_val = 64;
                s_angle[dy][dx] = (uint8_t)a_val;
            }
        }

        // 预热赛博时空流光调色板
        for (int i = 0; i < 256; i++) {
            float f = (float)i / 256.0f;
            uint8_t r = (uint8_t)(128.0f + 127.0f * fast_cos(f * 2.0f * FAST_PI));
            uint8_t g = (uint8_t)(100.0f + 100.0f * fast_sin(f * 2.0f * FAST_PI + 1.0f));
            uint8_t b = (uint8_t)(160.0f + 95.0f * fast_sin(f * 2.0f * FAST_PI + 2.5f));
            tunnel_palette[i] = rgb565(r, g, b);
        }

        s_initialized = true;
    }
}

static void tunnel_cleanup(void) {
}

static void tunnel_update(float dt) {
    (void)dt;
    shift_z += 3.2f; // 光速向前冲刺
    shift_a += 0.8f; // 空间自转
}

static void tunnel_render(uint16_t *fb) {
    uint8_t sz = (uint8_t)((int)shift_z & 255);
    uint8_t sa = (uint8_t)((int)shift_a & 255);

    for (int y = 0; y < TUNNEL_H; y++) {
        int sy = y * 2;
        int ady = abs(y - 60);
        if (ady > 60) ady = 60;
        const uint8_t *d_row = s_dist[ady];
        const uint8_t *a_row = s_angle[ady];
        bool y_upper = (y < 60);

        for (int x = 0; x < TUNNEL_W; x++) {
            int adx = abs(x - 60);
            if (adx > 60) adx = 60;

            uint8_t d = d_row[adx];
            uint8_t base_a = a_row[adx];
            uint8_t a;

            // 四象限极角对称展开
            if (x >= 60) {
                a = y_upper ? (256 - base_a) : base_a;
            } else {
                a = y_upper ? (128 + base_a) : (128 - base_a);
            }

            uint8_t u = (uint8_t)(d + sz);
            uint8_t v = (uint8_t)(a + sa);

            // 经典赛博黑客异或棋盘纹理 + 环状深度调制
            bool checker = ((u & 32) ^ (v & 32)) != 0;
            uint16_t color;

            if (checker) {
                // 亮斑：映射流光色谱
                color = tunnel_palette[(u + v) & 255];
            } else {
                // 暗槽：幽蓝暗影
                uint8_t shadow = (d > 100) ? (uint8_t)(d / 4) : 10;
                color = rgb565(shadow / 2, shadow / 3, shadow);
            }

            // 中心奇点炽白光晕
            if (d < 18) {
                color = COLOR_WHITE;
            }

            int sx = x * 2;
            uint32_t c32 = ((uint32_t)color << 16) | color;
            *(uint32_t *)&fb[sy * DISP_W + sx] = c32;
            *(uint32_t *)&fb[(sy + 1) * DISP_W + sx] = c32;
        }
    }
}

Effect fx_tunnel = {
    .id = "tunnel",
    .title = "» WARP TUNNEL VORTEX «",
    .subtitle = "时空跃迁虫洞光速飞驰隧道",
    .init = tunnel_init,
    .update = tunnel_update,
    .render = tunnel_render,
    .cleanup = tunnel_cleanup
};
