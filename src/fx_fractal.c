#include "effects.h"
#include "fast_math.h"
#include <string.h>
#include <stdlib.h>

#define JULIA_W 120
#define JULIA_H 120
#define MAX_ITER 18

static float julia_angle = 0.0f;
static uint16_t julia_palette[MAX_ITER + 1];

static void fractal_init(void) {
    julia_angle = 0.0f;

    // 预热电光分形调色板 (深邃幽蓝 -> 荧光青 -> 亮金 -> 烈焰粉紫 -> 炽白核心)
    for (int i = 0; i <= MAX_ITER; i++) {
        if (i == MAX_ITER) {
            julia_palette[i] = COLOR_BLACK; // 内部稳定核
        } else {
            float t = (float)i / (float)MAX_ITER;
            uint8_t r, g, b;
            if (t < 0.35f) {
                // 深蓝 -> 荧光青
                float f = t / 0.35f;
                r = 0;
                g = (uint8_t)(255 * f);
                b = (uint8_t)(140 + 115 * f);
            } else if (t < 0.70f) {
                // 荧光青 -> 亮金
                float f = (t - 0.35f) / 0.35f;
                r = (uint8_t)(255 * f);
                g = 240;
                b = (uint8_t)(255 * (1.0f - f));
            } else {
                // 亮金 -> 赛博紫粉
                float f = (t - 0.70f) / 0.30f;
                r = 255;
                g = (uint8_t)(240 * (1.0f - f));
                b = (uint8_t)(250 * f);
            }
            julia_palette[i] = rgb565(r, g, b);
        }
    }
}

static void fractal_cleanup(void) {
}

static void fractal_update(float dt) {
    (void)dt;
    julia_angle += 0.028f;
    if (julia_angle > 2.0f * FAST_PI) julia_angle -= 2.0f * FAST_PI;
}

static void fractal_render(uint16_t *fb) {
    // 动态在复平面轨道旋转复数参数 C: C = cr + i*ci
    float cr_f = -0.785f + 0.12f * fast_cos(julia_angle);
    float ci_f = 0.156f + 0.14f * fast_sin(julia_angle * 1.5f);

    // Q11 定点数转换 (1.0 = 2048)
    const int SCALE = 2048;
    int cr = (int)(cr_f * (float)SCALE);
    int ci = (int)(ci_f * (float)SCALE);
    const int ESCAPE_RADIUS_SQ = 4 * SCALE; // 4.0 in Q11

    // 预计算 X 坐标 Q11 网格点
    int x_grid[JULIA_W];
    for (int x = 0; x < JULIA_W; x++) {
        // x 从 -1.45 到 +1.45
        float fx = ((float)x - 60.0f) * (2.9f / 120.0f);
        x_grid[x] = (int)(fx * (float)SCALE);
    }

    // 120x120 极速定点数分形迭代 (单周期硬件乘法，无软浮点)
    for (int y = 0; y < JULIA_H; y++) {
        float fy = ((float)y - 60.0f) * (2.9f / 120.0f);
        int z0_i = (int)(fy * (float)SCALE);
        int sy = y * 2;

        for (int x = 0; x < JULIA_W; x++) {
            int zr = x_grid[x];
            int zi = z0_i;
            int iter = 0;

            while (iter < MAX_ITER) {
                int zr2 = (zr * zr) >> 11;
                int zi2 = (zi * zi) >> 11;

                if (zr2 + zi2 > ESCAPE_RADIUS_SQ) {
                    break;
                }

                // z = z^2 + c -> (zr^2 - zi^2 + cr) + i*(2*zr*zi + ci)
                int next_zr = zr2 - zi2 + cr;
                int next_zi = ((zr * zi) >> 10) + ci;

                zr = next_zr;
                zi = next_zi;
                iter++;
            }

            uint16_t color = julia_palette[iter];

            // 2x2 极速成对 32 位显存写入
            int sx = x * 2;
            uint32_t c32 = ((uint32_t)color << 16) | color;
            *(uint32_t *)&fb[sy * DISP_W + sx] = c32;
            *(uint32_t *)&fb[(sy + 1) * DISP_W + sx] = c32;
        }
    }
}

Effect fx_fractal = {
    .id = "fractal",
    .title = "» ELECTRIC JULIA «",
    .subtitle = "电光朱利亚全动态复变分形云",
    .init = fractal_init,
    .update = fractal_update,
    .render = fractal_render,
    .cleanup = fractal_cleanup
};
