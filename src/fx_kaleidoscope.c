#include "effects.h"
#include "fast_math.h"
#include <string.h>
#include <math.h>

#define NUM_NODES 8
#define SYMMETRY 12

typedef struct {
    float angle_speed;
    float radius_speed;
    float base_radius;
    float radius_amp;
    float phase;
    uint8_t color_offset;
} KaleidoNode;

static KaleidoNode nodes[NUM_NODES];
static float anim_time = 0.0f;

static void kaleidoscope_init(void) {
    anim_time = 0.0f;
    for (int i = 0; i < NUM_NODES; i++) {
        nodes[i].angle_speed = 0.35f + (float)(i + 1) * 0.2f * ((i % 2 == 0) ? 1.0f : -1.0f);
        nodes[i].radius_speed = 0.75f + (float)i * 0.25f;
        nodes[i].base_radius = 18.0f + (float)i * 13.0f;
        nodes[i].radius_amp = 12.0f + (float)i * 3.5f;
        nodes[i].phase = (float)i * (FAST_PI / (float)NUM_NODES);
        nodes[i].color_offset = i * 32;
    }
}

static void kaleidoscope_cleanup(void) {
}

static void kaleidoscope_update(float dt) {
    anim_time += dt;
}

static void kaleidoscope_render(uint16_t *fb) {
    memset(fb, 0, DISP_W * DISP_H * sizeof(uint16_t));
    int cx = DISP_W / 2;
    int cy = DISP_H / 2;

    float node_r[NUM_NODES];
    float node_th[NUM_NODES];

    for (int i = 0; i < NUM_NODES; i++) {
        node_r[i] = nodes[i].base_radius + nodes[i].radius_amp * sinf(anim_time * nodes[i].radius_speed + nodes[i].phase);
        node_th[i] = anim_time * nodes[i].angle_speed + nodes[i].phase;
    }

    float sym_step = (2.0f * FAST_PI) / (float)SYMMETRY;

    for (int s = 0; s < SYMMETRY; s++) {
        float rot = (float)s * sym_step;

        int px[NUM_NODES];
        int py[NUM_NODES];

        for (int i = 0; i < NUM_NODES; i++) {
            float th = node_th[i] + rot;
            px[i] = cx + (int)(node_r[i] * cosf(th));
            py[i] = cy + (int)(node_r[i] * sinf(th));
        }

        // 连线与复杂几何花瓣
        for (int i = 0; i < NUM_NODES - 1; i++) {
            uint8_t hue = (uint8_t)((int)(anim_time * 28.0f + nodes[i].color_offset + s * 14) & 0xFF);
            uint16_t col = hsv2rgb565(hue, 230, 240);

            display_draw_line(fb, px[i], py[i], px[i + 1], py[i + 1], col);

            // 跨层交叉连线形成全屏曼陀罗编织网格
            if (i + 2 < NUM_NODES) {
                uint16_t cross_col = rgb565(0, 70, 110);
                display_draw_line(fb, px[i], py[i], px[i + 2], py[i + 2], cross_col);
            }

            if (i == 0) {
                display_draw_line(fb, cx, cy, px[0], py[0], rgb565(0, 160, 220));
            }
        }

        // 外圈与内圈高亮点
        display_fill_circle(fb, px[NUM_NODES - 1], py[NUM_NODES - 1], 2, COLOR_WHITE);
        display_fill_circle(fb, px[3], py[3], 2, rgb565(255, 60, 180));
    }

    // 核心璀璨几何宝石
    float pulse = 7.0f + 2.5f * sinf(anim_time * 6.0f);
    uint8_t core_hue = (uint8_t)((int)(anim_time * 45.0f) & 0xFF);
    display_fill_circle(fb, cx, cy, (int)pulse, hsv2rgb565(core_hue, 220, 255));
    display_draw_circle(fb, cx, cy, (int)(pulse + 5.0f), COLOR_WHITE);
}

Effect fx_kaleidoscope = {
    .id = "kaleidoscope",
    .title = "迷幻光影万花筒",
    .subtitle = "12重轴对称曼陀罗激光编织几何魔毯",
    .init = kaleidoscope_init,
    .update = kaleidoscope_update,
    .render = kaleidoscope_render,
    .cleanup = kaleidoscope_cleanup
};
