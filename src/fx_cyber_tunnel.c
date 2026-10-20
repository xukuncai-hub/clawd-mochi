#include "effects.h"
#include "fast_math.h"
#include <string.h>
#include <math.h>

#define HEX_RINGS 10
#define HEX_SIDES 6

typedef struct {
    float z; // 0.1 ~ 1.0 深度
} TunnelRing;

static TunnelRing rings[HEX_RINGS];
static float anim_time = 0.0f;

static void cyber_tunnel_init(void) {
    anim_time = 0.0f;
    for (int i = 0; i < HEX_RINGS; i++) {
        rings[i].z = (float)(i + 1) / (float)HEX_RINGS;
    }
}

static void cyber_tunnel_cleanup(void) {
}

static void cyber_tunnel_update(float dt) {
    anim_time += dt;
    float speed = 0.45f;
    for (int i = 0; i < HEX_RINGS; i++) {
        rings[i].z += speed * dt;
        if (rings[i].z > 1.0f) {
            rings[i].z -= 1.0f;
        }
    }
}

static void cyber_tunnel_render(uint16_t *fb) {
    memset(fb, 0, DISP_W * DISP_H * sizeof(uint16_t));
    int cx = DISP_W / 2;
    int cy = DISP_H / 2;

    float base_rot = anim_time * 0.4f;

    // 存储各个环的顶点坐标，方便连线形成纵深肋骨
    int ring_x[HEX_RINGS][HEX_SIDES];
    int ring_y[HEX_RINGS][HEX_SIDES];

    for (int r = 0; r < HEX_RINGS; r++) {
        float z = rings[r].z;
        if (z < 0.05f) continue;

        // 非线性透视放大 (类似于 1/z 或者指数)
        float scale = z * z * 135.0f;
        float ring_rot = base_rot + z * 1.2f;

        // 计算 6 个六边形顶点
        for (int s = 0; s < HEX_SIDES; s++) {
            float angle = ring_rot + (float)s * (2.0f * FAST_PI / (float)HEX_SIDES);
            ring_x[r][s] = cx + (int)(scale * cosf(angle));
            ring_y[r][s] = cy + (int)(scale * sinf(angle));
        }

        // 颜色根据深度 z 进行渐变 (近处高亮青紫，远处暗蓝)
        uint8_t bright = (uint8_t)(z * 255.0f);
        uint16_t col;
        if (r % 2 == 0) {
            col = rgb565(0, bright, (uint8_t)(bright * 0.95f)); // 青绿
        } else {
            col = rgb565(bright, 0, (uint8_t)(bright * 0.85f)); // 霓虹品红
        }

        // 绘制当前环的六边形边框
        for (int s = 0; s < HEX_SIDES; s++) {
            int next_s = (s + 1) % HEX_SIDES;
            display_draw_line(fb, ring_x[r][s], ring_y[r][s], ring_x[r][next_s], ring_y[r][next_s], col);
        }
    }

    // 绘制穿透隧道的 6 条纵向骨架光束线 (从中心向各个角放射)
    for (int s = 0; s < HEX_SIDES; s++) {
        float angle = base_rot + 1.0f * 1.2f + (float)s * (2.0f * FAST_PI / (float)HEX_SIDES);
        int out_x = cx + (int)(135.0f * cosf(angle));
        int out_y = cy + (int)(135.0f * sinf(angle));
        display_draw_line(fb, cx, cy, out_x, out_y, rgb565(0, 100, 150));
    }

    // 核心时空奇点光斑
    float core_pulse = 4.0f + 2.0f * sinf(anim_time * 8.0f);
    display_fill_circle(fb, cx, cy, (int)core_pulse, COLOR_WHITE);
    display_draw_circle(fb, cx, cy, (int)(core_pulse + 5.0f), rgb565(0, 220, 255));
}

Effect fx_cyber_tunnel = {
    .id = "cyber_tunnel",
    .title = "赛博六角隧道",
    .subtitle = "高能几何光子跃迁通道",
    .init = cyber_tunnel_init,
    .update = cyber_tunnel_update,
    .render = cyber_tunnel_render,
    .cleanup = cyber_tunnel_cleanup
};
