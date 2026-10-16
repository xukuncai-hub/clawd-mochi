#include "effects.h"
#include "fast_math.h"
#include <string.h>
#include <stdlib.h>

#define NUM_GALAXY_STARS 220

typedef struct {
    float dist;
    float base_angle;
    float arm_offset;
    float z_offset;
    float speed;
    uint16_t color;
    uint8_t size;
} GalaxyStar;

static GalaxyStar stars[NUM_GALAXY_STARS];
static float galaxy_rot = 0.0f;
static float tilt_pitch = 0.65f; // 倾角视角

static void galaxy_init(void) {
    galaxy_rot = 0.0f;
    for (int i = 0; i < NUM_GALAXY_STARS; i++) {
        // 双主旋臂对映：0 或 PI
        float arm = (i % 2 == 0) ? 0.0f : FAST_PI;
        float d = 8.0f + (float)(fast_rand() % 98);
        stars[i].dist = d;
        stars[i].base_angle = arm + (d * 0.055f); // 对数旋臂展角
        stars[i].arm_offset = ((float)(fast_rand() % 100) - 50.0f) * 0.006f;
        stars[i].z_offset = ((float)(fast_rand() % 100) - 50.0f) * 0.12f;
        stars[i].speed = 1.6f / sqrtf(d); // 旋转曲线速度

        // 根据与核球距离决定星体光谱颜色
        if (d < 22.0f) {
            // 核心高密核球 (炽白与金黄)
            stars[i].color = (fast_rand() % 3 == 0) ? COLOR_WHITE : rgb565(255, 230, 110);
            stars[i].size = 2;
        } else if (d < 65.0f) {
            // 旋臂主序恒星 (荧光青与湛蓝)
            stars[i].color = (fast_rand() % 2 == 0) ? rgb565(0, 240, 255) : rgb565(80, 150, 255);
            stars[i].size = 1;
        } else {
            // 外缘星际尘埃与红矮星 (紫粉与暗红)
            stars[i].color = (fast_rand() % 2 == 0) ? rgb565(240, 60, 200) : rgb565(180, 40, 120);
            stars[i].size = 1;
        }
    }
}

static void galaxy_cleanup(void) {
}

static void galaxy_update(float dt) {
    (void)dt;
    galaxy_rot += 0.038f;
    if (galaxy_rot > 2.0f * FAST_PI) galaxy_rot -= 2.0f * FAST_PI;
}

static void galaxy_render(uint16_t *fb) {
    memset(fb, 0, DISP_W * DISP_H * sizeof(uint16_t));

    const int cx = 120;
    const int cy = 120;

    float cos_pitch = fast_cos(tilt_pitch);
    float sin_pitch = fast_sin(tilt_pitch);

    // 1. 绘制极向相对论喷流 (中心垂直射出的细长微弱极光)
    for (int j = -35; j <= 35; j++) {
        int py = cy + (int)((float)j * cos_pitch);
        int px = cx - (int)((float)j * sin_pitch * 0.15f);
        if (px >= 0 && px < DISP_W && py >= 0 && py < DISP_H) {
            fb[py * DISP_W + px] = rgb565(80, 160, 255);
        }
    }

    // 2. 绘制 220 颗 3D 旋转银河恒星
    for (int i = 0; i < NUM_GALAXY_STARS; i++) {
        float cur_angle = stars[i].base_angle + stars[i].arm_offset + galaxy_rot * stars[i].speed * 1.5f;
        float x_plane = stars[i].dist * fast_cos(cur_angle);
        float y_plane = stars[i].dist * fast_sin(cur_angle);
        float z_plane = stars[i].z_offset;

        // 3D 俯仰视角投影
        float x_proj = x_plane;
        float y_proj = y_plane * cos_pitch - z_plane * sin_pitch;

        int sx = cx + (int)x_proj;
        int sy = cy + (int)y_proj;

        if (sx >= 1 && sx < DISP_W - 1 && sy >= 1 && sy < DISP_H - 1) {
            uint16_t c = stars[i].color;
            fb[sy * DISP_W + sx] = c;
            if (stars[i].size > 1) {
                fb[sy * DISP_W + sx + 1] = c;
                fb[(sy + 1) * DISP_W + sx] = c;
                fb[(sy + 1) * DISP_W + sx + 1] = c;
            }
        }
    }

    // 3. 中心核球超密集发光体
    display_fill_rect(fb, cx - 2, cy - 2, 5, 5, COLOR_WHITE);
    fb[(cy - 3) * DISP_W + cx] = rgb565(255, 240, 150);
    fb[(cy + 3) * DISP_W + cx] = rgb565(255, 240, 150);
    fb[cy * DISP_W + (cx - 3)] = rgb565(255, 240, 150);
    fb[cy * DISP_W + (cx + 3)] = rgb565(255, 240, 150);
}

Effect fx_galaxy = {
    .id = "galaxy",
    .title = "» SPIRAL GALAXY «",
    .subtitle = "3D 银河旋涡与星云粒子潮",
    .init = galaxy_init,
    .update = galaxy_update,
    .render = galaxy_render,
    .cleanup = galaxy_cleanup
};
