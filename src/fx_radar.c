#include "effects.h"
#include "fast_math.h"
#include <string.h>
#include <math.h>

#define RADAR_RINGS 5
#define RADAR_SPOKES 16
#define NUM_AIR_TARGETS 4

typedef struct {
    float x, y, z;
    float glow;
} AirTarget;

static float sweep_rot = 0.0f;
static float anim_time = 0.0f;
static AirTarget air_targets[NUM_AIR_TARGETS];

static void radar_init(void) {
    sweep_rot = 0.0f;
    anim_time = 0.0f;
    for (int i = 0; i < NUM_AIR_TARGETS; i++) {
        float a = (float)(fast_rand() % 360) * (FAST_PI / 180.0f);
        float d = 30.0f + (float)(fast_rand() % 60);
        air_targets[i].x = d * cosf(a);
        air_targets[i].y = d * sinf(a);
        air_targets[i].z = 10.0f + (float)(fast_rand() % 25);
        air_targets[i].glow = 0.0f;
    }
}

static void radar_cleanup(void) {
}

static void radar_update(float dt) {
    anim_time += dt;
    sweep_rot += dt * 3.2f;
    if (sweep_rot > 2.0f * FAST_PI) sweep_rot -= 2.0f * FAST_PI;

    for (int i = 0; i < NUM_AIR_TARGETS; i++) {
        float t_angle = atan2f(air_targets[i].y, air_targets[i].x);
        if (t_angle < 0.0f) t_angle += 2.0f * FAST_PI;
        float diff = fabsf(sweep_rot - t_angle);
        if (diff < 0.15f || diff > (2.0f * FAST_PI - 0.15f)) {
            air_targets[i].glow = 1.0f;
        } else if (air_targets[i].glow > 0.0f) {
            air_targets[i].glow -= dt * 0.8f;
            if (air_targets[i].glow < 0.0f) air_targets[i].glow = 0.0f;
        }
    }
}

static void radar_render(uint16_t *fb) {
    memset(fb, 0, DISP_W * DISP_H * sizeof(uint16_t));
    int cx = DISP_W / 2;
    int cy = DISP_H / 2;

    float pitch = 0.55f; // 透视仰角
    float cos_p = cosf(pitch);

    // 1. 战术穹顶多层同心网格 (Concentric Rings)
    for (int r = 1; r <= RADAR_RINGS; r++) {
        float rad = (float)r * 21.0f;
        int last_x = 0, last_y = 0;
        int first_x = 0, first_y = 0;

        for (int s = 0; s <= 32; s++) {
            float th = (float)s * (2.0f * FAST_PI / 32.0f);
            int sx = cx + (int)(rad * cosf(th));
            int sy = cy + (int)(rad * sinf(th) * cos_p);

            if (s == 0) {
                first_x = sx;
                first_y = sy;
            } else {
                display_draw_line(fb, last_x, last_y, sx, sy, (r == RADAR_RINGS) ? rgb565(0, 200, 240) : rgb565(0, 80, 110));
            }
            last_x = sx;
            last_y = sy;
        }
        display_draw_line(fb, last_x, last_y, first_x, first_y, (r == RADAR_RINGS) ? rgb565(0, 200, 240) : rgb565(0, 80, 110));
    }

    // 2. 径向战术辐射经线 (Spokes)
    for (int s = 0; s < RADAR_SPOKES; s++) {
        float th = (float)s * (2.0f * FAST_PI / (float)RADAR_SPOKES);
        int sx = cx + (int)(105.0f * cosf(th));
        int sy = cy + (int)(105.0f * sinf(th) * cos_p);
        display_draw_line(fb, cx, cy, sx, sy, (s % 4 == 0) ? rgb565(0, 150, 190) : rgb565(0, 50, 75));
    }

    // 3. 360 度高能扇形扫描光束 (带柔光拖尾)
    for (int t = 0; t < 12; t++) {
        float a = sweep_rot - (float)t * 0.035f;
        int sx = cx + (int)(105.0f * cosf(a));
        int sy = cy + (int)(105.0f * sinf(a) * cos_p);
        uint8_t val = (uint8_t)(255 - t * 20);
        uint16_t beam_col = (t == 0) ? COLOR_WHITE : rgb565(0, val, (uint8_t)(val * 0.75f));
        display_draw_line(fb, cx, cy, sx, sy, beam_col);
    }

    // 4. 三维立体空中锁定目标与高度垂线 (Tactical Altitude Stems)
    for (int i = 0; i < NUM_AIR_TARGETS; i++) {
        int ground_x = cx + (int)air_targets[i].x;
        int ground_y = cy + (int)(air_targets[i].y * cos_p);
        int air_y = ground_y - (int)air_targets[i].z;

        // 垂直高度指示线
        display_draw_line(fb, ground_x, ground_y, ground_x, air_y, rgb565(0, 100, 140));
        display_draw_pixel(fb, ground_x, ground_y, rgb565(0, 70, 90));

        if (air_targets[i].glow > 0.05f) {
            uint8_t g = (uint8_t)(air_targets[i].glow * 255.0f);
            display_fill_circle(fb, ground_x, air_y, 3, rgb565(g, 30, 80));
            display_draw_box(fb, ground_x - 5, air_y - 5, 11, 11, rgb565(g, g, 40));
        } else {
            display_draw_pixel(fb, ground_x, air_y, rgb565(0, 120, 160));
        }
    }

    // 5. 核心基站指示灯
    display_fill_circle(fb, cx, cy, 3, COLOR_WHITE);
}

Effect fx_radar = {
    .id = "radar",
    .title = "赛博全息战术雷达",
    .subtitle = "三维透视穹顶网格与多目标空中高度态势",
    .init = radar_init,
    .update = radar_update,
    .render = radar_render,
    .cleanup = radar_cleanup
};
