#include "effects.h"
#include "fast_math.h"
#include <string.h>
#include <math.h>

#define SPHERE_LATS 9
#define SPHERE_LONS 14
#define PTS_PER_LAT 24

static float rot_x = 0.0f;
static float rot_y = 0.0f;
static float anim_time = 0.0f;

static void sphere_init(void) {
    rot_x = 0.45f;
    rot_y = 0.0f;
    anim_time = 0.0f;
}

static void sphere_cleanup(void) {
}

static void sphere_update(float dt) {
    anim_time += dt;
    rot_x += dt * 0.45f;
    rot_y += dt * 0.85f;
}

static inline void project_point(float x, float y, float z, int *sx, int *sy, float *sz) {
    float cos_y = cosf(rot_y), sin_y = sinf(rot_y);
    float x1 = x * cos_y + z * sin_y;
    float y1 = y;
    float z1 = -x * sin_y + z * cos_y;

    float cos_x = cosf(rot_x), sin_x = sinf(rot_x);
    float x2 = x1;
    float y2 = y1 * cos_x - z1 * sin_x;
    float z2 = y1 * sin_x + z1 * cos_x;

    float fov = 190.0f;
    float scale = fov / (fov + z2);

    *sx = DISP_W / 2 + (int)(x2 * scale);
    *sy = DISP_H / 2 + (int)(y2 * scale);
    *sz = z2;
}

static void sphere_render(uint16_t *fb) {
    memset(fb, 0, DISP_W * DISP_H * sizeof(uint16_t));
    float R = 72.0f;

    // 1. 绘制纬线连续圆环网格
    for (int lat = 1; lat < SPHERE_LATS; lat++) {
        float theta = -FAST_PI * 0.5f + (float)lat * (FAST_PI / (float)SPHERE_LATS);
        float r_lat = R * cosf(theta);
        float y_lat = R * sinf(theta);

        int last_sx = 0, last_sy = 0;
        int first_sx = 0, first_sy = 0;

        for (int p = 0; p < PTS_PER_LAT; p++) {
            float phi = (float)p * (2.0f * FAST_PI / (float)PTS_PER_LAT);
            float x = r_lat * cosf(phi);
            float z = r_lat * sinf(phi);

            int sx, sy;
            float sz;
            project_point(x, y_lat, z, &sx, &sy, &sz);

            if (p == 0) {
                first_sx = sx;
                first_sy = sy;
            } else {
                uint16_t col = (sz > 0.0f) ? rgb565(0, 220, 255) : rgb565(0, 45, 65);
                display_draw_line(fb, last_sx, last_sy, sx, sy, col);
            }
            last_sx = sx;
            last_sy = sy;
        }
        display_draw_line(fb, last_sx, last_sy, first_sx, first_sy, rgb565(0, 50, 75));
    }

    // 2. 绘制经线大圆连续网格
    for (int lon = 0; lon < SPHERE_LONS; lon++) {
        float phi = (float)lon * (FAST_PI / (float)SPHERE_LONS);
        int last_sx = 0, last_sy = 0;
        int first_sx = 0, first_sy = 0;

        for (int p = 0; p < PTS_PER_LAT; p++) {
            float theta = (float)p * (2.0f * FAST_PI / (float)PTS_PER_LAT);
            float x = R * sinf(theta) * cosf(phi);
            float y = R * cosf(theta);
            float z = R * sinf(theta) * sinf(phi);

            int sx, sy;
            float sz;
            project_point(x, y, z, &sx, &sy, &sz);

            if (p == 0) {
                first_sx = sx;
                first_sy = sy;
            } else {
                uint8_t hue = (uint8_t)((int)(anim_time * 25.0f + lon * 18) & 0xFF);
                uint16_t col = (sz > 0.0f) ? hsv2rgb565(hue, 220, 240) : rgb565(20, 30, 50);
                display_draw_line(fb, last_sx, last_sy, sx, sy, col);
            }
            last_sx = sx;
            last_sy = sy;
        }
        display_draw_line(fb, last_sx, last_sy, first_sx, first_sy, rgb565(20, 30, 50));
    }

    // 3. 赤道外层高能轨道光环
    int last_ox = 0, last_oy = 0;
    int first_ox = 0, first_oy = 0;
    for (int p = 0; p < 32; p++) {
        float phi = (float)p * (2.0f * FAST_PI / 32.0f);
        float ox = (R + 18.0f) * cosf(phi);
        float oy = 0.0f;
        float oz = (R + 18.0f) * sinf(phi);

        int sx, sy;
        float sz;
        project_point(ox, oy, oz, &sx, &sy, &sz);
        if (p == 0) {
            first_ox = sx;
            first_oy = sy;
        } else {
            display_draw_line(fb, last_ox, last_oy, sx, sy, (sz > 0) ? COLOR_WHITE : rgb565(0, 100, 160));
        }
        last_ox = sx;
        last_oy = sy;
    }
    display_draw_line(fb, last_ox, last_oy, first_ox, first_oy, rgb565(0, 100, 160));

    // 4. 核心聚变光核
    int cx = DISP_W / 2;
    int cy = DISP_H / 2;
    int core_r = (int)(8.0f + 2.0f * sinf(anim_time * 6.0f));
    display_fill_circle(fb, cx, cy, core_r, rgb565(0, 240, 255));
    display_fill_circle(fb, cx, cy, 3, COLOR_WHITE);
}

Effect fx_sphere = {
    .id = "sphere",
    .title = "赛博经纬三维球",
    .subtitle = "3D高密度全息自旋经纬线框与赤道环",
    .init = sphere_init,
    .update = sphere_update,
    .render = sphere_render,
    .cleanup = sphere_cleanup
};
