#include "effects.h"
#include "fast_math.h"
#include <string.h>
#include <math.h>

#define WORMHOLE_RINGS 14
#define WORMHOLE_SECTORS 18

static float anim_time = 0.0f;

static void tunnel_init(void) {
    anim_time = 0.0f;
}

static void tunnel_cleanup(void) {
}

static void tunnel_update(float dt) {
    anim_time += dt;
}

static void tunnel_render(uint16_t *fb) {
    memset(fb, 0, DISP_W * DISP_H * sizeof(uint16_t));
    int cx = DISP_W / 2;
    int cy = DISP_H / 2;

    static int sx[WORMHOLE_RINGS][WORMHOLE_SECTORS];
    static int sy[WORMHOLE_RINGS][WORMHOLE_SECTORS];

    // 1. 计算时空虫洞经纬网格 (非线性几何级数放大)
    for (int r = 0; r < WORMHOLE_RINGS; r++) {
        // 动态移动深度
        float z_offset = fmodf(anim_time * 0.65f, 1.0f / (float)WORMHOLE_RINGS);
        float depth = ((float)(r + 1) / (float)WORMHOLE_RINGS) - z_offset;
        if (depth < 0.05f) depth = 0.05f;

        // 非线性幂次展开
        float rad = powf(depth, 2.2f) * 135.0f;
        // 随深度产生黑洞角动量自旋扭转 (Frame Dragging)
        float twist = anim_time * 1.5f + (1.0f / depth) * 0.25f;

        for (int s = 0; s < WORMHOLE_SECTORS; s++) {
            float angle = twist + (float)s * (2.0f * FAST_PI / (float)WORMHOLE_SECTORS);
            sx[r][s] = cx + (int)(rad * cosf(angle));
            sy[r][s] = cy + (int)(rad * sinf(angle));
        }
    }

    // 2. 绘制虫洞纬线同心圆环
    for (int r = 0; r < WORMHOLE_RINGS; r++) {
        float depth = (float)(r + 1) / (float)WORMHOLE_RINGS;
        uint8_t hue = (uint8_t)((int)(anim_time * 30.0f + r * 14) & 0xFF);
        uint8_t val = (uint8_t)(depth * 255.0f);
        uint16_t col = hsv2rgb565(hue, 230, val);

        for (int s = 0; s < WORMHOLE_SECTORS; s++) {
            int next_s = (s + 1) % WORMHOLE_SECTORS;
            display_draw_line(fb, sx[r][s], sy[r][s], sx[r][next_s], sy[r][next_s], col);
        }
    }

    // 3. 绘制螺旋时空骨架肋骨经线 (纵深穿透向奇点收拢)
    for (int s = 0; s < WORMHOLE_SECTORS; s++) {
        for (int r = 0; r < WORMHOLE_RINGS - 1; r++) {
            uint16_t rib_col = (s % 2 == 0) ? rgb565(0, 240, 255) : rgb565(255, 40, 160);
            display_draw_line(fb, sx[r][s], sy[r][s], sx[r + 1][s], sy[r + 1][s], rib_col);
        }
    }

    // 4. 奇点深渊能量光核
    display_fill_circle(fb, cx, cy, 5, COLOR_WHITE);
    display_draw_circle(fb, cx, cy, 9, rgb565(0, 220, 255));
}

Effect fx_tunnel = {
    .id = "tunnel",
    .title = "赛博扭曲虫洞网格",
    .subtitle = "非线性时空收拢螺旋经纬跃迁管道",
    .init = tunnel_init,
    .update = tunnel_update,
    .render = tunnel_render,
    .cleanup = tunnel_cleanup
};
