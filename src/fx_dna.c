#include "effects.h"
#include "fast_math.h"
#include <string.h>
#include <stdlib.h>

#define NUM_DNA_NODES 28
#define NUM_DNA_SPARKS 40

typedef struct {
    float x, y, z;
    float speed;
} DnaSpark;

static float dna_rot = 0.0f;
static DnaSpark sparks[NUM_DNA_SPARKS];

static void dna_init(void) {
    dna_rot = 0.0f;
    for (int i = 0; i < NUM_DNA_SPARKS; i++) {
        sparks[i].x = (float)fast_rand_range(-100, 100);
        sparks[i].y = (float)fast_rand_range(-110, 110);
        sparks[i].z = (float)fast_rand_range(-100, 100);
        sparks[i].speed = 0.5f + (float)(fast_rand() % 100) / 100.0f;
    }
}

static void dna_cleanup(void) {
}

static void dna_update(float dt) {
    (void)dt;
    dna_rot += 0.042f;
    if (dna_rot > 2.0f * FAST_PI) dna_rot -= 2.0f * FAST_PI;

    for (int i = 0; i < NUM_DNA_SPARKS; i++) {
        sparks[i].y -= sparks[i].speed;
        if (sparks[i].y < -115.0f) {
            sparks[i].y = 115.0f;
            sparks[i].x = (float)fast_rand_range(-100, 100);
            sparks[i].z = (float)fast_rand_range(-100, 100);
        }
    }
}

static void project_dna_pt(float x, float y, float z, int *sx, int *sy, float *scale) {
    float cam_dist = 220.0f;
    float pz = z + cam_dist;
    if (pz < 10.0f) pz = 10.0f;
    *scale = 160.0f / pz;
    *sx = 120 + (int)(x * (*scale));
    *sy = 120 + (int)(y * (*scale));
}

static void dna_render(uint16_t *fb) {
    memset(fb, 0, DISP_W * DISP_H * sizeof(uint16_t));

    // 1. 渲染周围漂浮的量子微粒
    for (int i = 0; i < NUM_DNA_SPARKS; i++) {
        int sx, sy;
        float sc;
        project_dna_pt(sparks[i].x, sparks[i].y, sparks[i].z, &sx, &sy, &sc);
        if (sx >= 0 && sx < DISP_W && sy >= 0 && sy < DISP_H) {
            uint16_t sc_color = (sparks[i].z > 0) ? rgb565(120, 200, 255) : rgb565(40, 70, 120);
            display_draw_pixel(fb, sx, sy, sc_color);
        }
    }

    // 2. 渲染 3D DNA 双螺旋链
    const float radius = 55.0f;

    for (int i = 0; i < NUM_DNA_NODES; i++) {
        float norm_y = (float)i / (float)(NUM_DNA_NODES - 1);
        float py = (norm_y - 0.5f) * 210.0f;
        float angle = norm_y * 4.5f * FAST_PI + dna_rot;

        // 链 A 坐标
        float ax = radius * fast_cos(angle);
        float az = radius * fast_sin(angle);
        int asx, asy;
        float asc;
        project_dna_pt(ax, py, az, &asx, &asy, &asc);

        // 链 B 坐标 (相隔 180 度)
        float bx = radius * fast_cos(angle + FAST_PI);
        float bz = radius * fast_sin(angle + FAST_PI);
        int bsx, bsy;
        float bsc;
        project_dna_pt(bx, py, bz, &bsx, &bsy, &bsc);

        // 绘制碱基对横梁阶梯 (颜色交替：A-T 与 C-G)
        uint16_t rung_col;
        if (i % 2 == 0) {
            rung_col = rgb565(0, 240, 220); // 荧光青
        } else {
            rung_col = rgb565(255, 60, 180); // 荧光粉红
        }
        display_draw_line(fb, asx, asy, bsx, bsy, rung_col);

        // 绘制链 A 螺旋节点球 (带深度远近尺寸)
        int rad_a = (az > 0) ? 3 : 2;
        uint16_t col_a = (az > 0) ? COLOR_WHITE : rgb565(40, 140, 255);
        display_fill_rect(fb, asx - rad_a / 2, asy - rad_a / 2, rad_a, rad_a, col_a);

        // 绘制链 B 螺旋节点球
        int rad_b = (bz > 0) ? 3 : 2;
        uint16_t col_b = (bz > 0) ? COLOR_YELLOW : rgb565(200, 40, 100);
        display_fill_rect(fb, bsx - rad_b / 2, bsy - rad_b / 2, rad_b, rad_b, col_b);
    }
}

Effect fx_dna = {
    .id = "dna",
    .title = "» 3D DNA HELIX «",
    .subtitle = "3D 炫彩旋转 DNA 双螺旋粒子链",
    .init = dna_init,
    .update = dna_update,
    .render = dna_render,
    .cleanup = dna_cleanup
};
