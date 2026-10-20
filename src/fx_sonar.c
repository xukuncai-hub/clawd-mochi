#include "effects.h"
#include "fast_math.h"
#include <string.h>
#include <math.h>

#define SONAR_RINGS 6
#define SONAR_RAYS 16
#define NUM_TARGETS 4

typedef struct {
    float x, y;
    float dist;
    float angle;
    float glow;
} SonarTarget;

static SonarTarget targets[NUM_TARGETS];
static float sweep_angle = 0.0f;
static float anim_time = 0.0f;

static void sonar_init(void) {
    sweep_angle = 0.0f;
    anim_time = 0.0f;
    for (int i = 0; i < NUM_TARGETS; i++) {
        targets[i].dist = 35.0f + (float)(fast_rand() % 60);
        targets[i].angle = (float)(fast_rand() % 360) * (FAST_PI / 180.0f);
        targets[i].glow = 0.0f;
    }
}

static void sonar_cleanup(void) {
}

static void sonar_update(float dt) {
    anim_time += dt;
    sweep_angle += dt * 2.8f;
    if (sweep_angle > 2.0f * FAST_PI) sweep_angle -= 2.0f * FAST_PI;

    for (int i = 0; i < NUM_TARGETS; i++) {
        float diff = fabsf(sweep_angle - targets[i].angle);
        if (diff < 0.15f || diff > (2.0f * FAST_PI - 0.15f)) {
            targets[i].glow = 1.0f;
        } else if (targets[i].glow > 0.0f) {
            targets[i].glow -= dt * 0.75f;
            if (targets[i].glow < 0.0f) targets[i].glow = 0.0f;
        }
    }
}

static void sonar_render(uint16_t *fb) {
    memset(fb, 0, DISP_W * DISP_H * sizeof(uint16_t));
    int cx = DISP_W / 2;
    int cy = DISP_H / 2;

    static int grid_x[SONAR_RINGS][SONAR_RAYS];
    static int grid_y[SONAR_RINGS][SONAR_RAYS];

    // 1. 计算三维全息雷达碟形凹面网格 (Concave Dish Mesh)
    for (int r = 0; r < SONAR_RINGS; r++) {
        float rad = 18.0f + (float)r * 16.5f;

        // 声纳行进波纹对碟面的法向拉伸
        float wave = sinf(rad * 0.15f - anim_time * 5.0f) * 4.0f;
        float cur_rad = rad + wave;

        for (int a = 0; a < SONAR_RAYS; a++) {
            float th = (float)a * (2.0f * FAST_PI / (float)SONAR_RAYS);
            grid_x[r][a] = cx + (int)(cur_rad * cosf(th));
            grid_y[r][a] = cy + (int)(cur_rad * sinf(th) * 0.88f); // 略带倾角透视
        }
    }

    // 绘制同心环线
    for (int r = 0; r < SONAR_RINGS; r++) {
        uint8_t val = (uint8_t)(80 + r * 28);
        uint16_t ring_col = rgb565(0, val, (uint8_t)(val * 0.9f));
        for (int a = 0; a < SONAR_RAYS; a++) {
            int next_a = (a + 1) % SONAR_RAYS;
            display_draw_line(fb, grid_x[r][a], grid_y[r][a], grid_x[r][next_a], grid_y[r][next_a], ring_col);
        }
    }

    // 绘制径向辐射经线
    for (int a = 0; a < SONAR_RAYS; a++) {
        for (int r = 0; r < SONAR_RINGS - 1; r++) {
            display_draw_line(fb, grid_x[r][a], grid_y[r][a], grid_x[r + 1][a], grid_y[r + 1][a], rgb565(0, 70, 95));
        }
    }

    // 2. 旋转高能全息雷达扫描光刃 (带扇形拖尾)
    for (int t = 0; t < 10; t++) {
        float a = sweep_angle - (float)t * 0.04f;
        int sx = cx + (int)(105.0f * cosf(a));
        int sy = cy + (int)(105.0f * sinf(a) * 0.88f);
        uint8_t b = (uint8_t)(255 - t * 24);
        display_draw_line(fb, cx, cy, sx, sy, (t == 0) ? COLOR_WHITE : rgb565(0, b, b));
    }

    // 3. 扫描锁定目标 (高亮反光雷达角标)
    for (int i = 0; i < NUM_TARGETS; i++) {
        int tx = cx + (int)(targets[i].dist * cosf(targets[i].angle));
        int ty = cy + (int)(targets[i].dist * sinf(targets[i].angle) * 0.88f);

        if (targets[i].glow > 0.05f) {
            uint8_t val = (uint8_t)(targets[i].glow * 255.0f);
            display_fill_circle(fb, tx, ty, 3, rgb565(val, 20, 80));
            display_draw_box(fb, tx - 5, ty - 5, 11, 11, rgb565(val, val, 60));
        } else {
            display_draw_pixel(fb, tx, ty, rgb565(0, 90, 110));
        }
    }

    // 4. 中央全息声纳发射基座
    display_fill_circle(fb, cx, cy, 4, COLOR_WHITE);
    display_draw_circle(fb, cx, cy, 8, rgb565(0, 240, 255));
}

Effect fx_sonar = {
    .id = "sonar",
    .title = "赛博深空声纳",
    .subtitle = "极坐标凹面脉冲网格与隐匿战术目标探测",
    .init = sonar_init,
    .update = sonar_update,
    .render = sonar_render,
    .cleanup = sonar_cleanup
};
