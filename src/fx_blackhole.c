#include "effects.h"
#include "fast_math.h"
#include <string.h>
#include <stdlib.h>

#define NUM_LENSING_STARS 45

typedef struct {
    float angle;
    float radius;
    float speed;
    int layer;
} DiskParticle;

typedef struct {
    int16_t px, py;
    uint16_t color;
} PreStar;

typedef struct {
    uint8_t px, py_up, py_dn;
    uint16_t col;
} HaloPoint;

static DiskParticle disk_pts[180];
static PreStar pre_stars[NUM_LENSING_STARS];
static int num_pre_stars = 0;
static HaloPoint halo_pts[900];
static int num_halo_pts = 0;
static float hole_rot = 0.0f;

static void blackhole_init(void) {
    hole_rot = 0.0f;
    for (int i = 0; i < 180; i++) {
        disk_pts[i].angle = (float)(fast_rand() % 628) / 100.0f;
        disk_pts[i].radius = 28.0f + (float)(fast_rand() % 75);
        disk_pts[i].speed = 1.8f / sqrtf(disk_pts[i].radius); // 开普勒轨道速度
        disk_pts[i].layer = fast_rand() % 3;
    }

    const int cx = 120;
    const int cy = 120;
    const int r_event = 24;

    // 预计算引力透镜背景扭曲星辰 (消除单帧浮点与 sqrtf)
    num_pre_stars = 0;
    for (int i = 0; i < NUM_LENSING_STARS; i++) {
        float bx = (float)(fast_rand() % 240);
        float by = (float)(fast_rand() % 240);
        float bright = 0.3f + (float)(fast_rand() % 70) / 100.0f;

        float dx = bx - (float)cx;
        float dy = by - (float)cy;
        float dist = sqrtf(dx * dx + dy * dy);
        if (dist > r_event + 6) {
            float lens_factor = 1.0f + (140.0f / (dist * dist));
            int px = cx + (int)(dx * lens_factor);
            int py = cy + (int)(dy * lens_factor);
            if (px >= 0 && px < DISP_W && py >= 0 && py < DISP_H) {
                uint8_t c = (uint8_t)(255 * bright);
                pre_stars[num_pre_stars].px = (int16_t)px;
                pre_stars[num_pre_stars].py = (int16_t)py;
                pre_stars[num_pre_stars].color = rgb565(c, c, (uint8_t)(c * 1.1f > 255 ? 255 : c * 1.1f));
                num_pre_stars++;
            }
        }
    }

    // 预计算引力透镜上下主光弧 (消除每帧 880 次浮点三角运算)
    num_halo_pts = 0;
    for (int r = r_event + 3; r <= r_event + 18; r++) {
        float norm_r = (float)(r - r_event) / 18.0f;
        uint8_t halo_r = (uint8_t)(255 * (1.0f - norm_r * 0.5f));
        uint8_t halo_g = (uint8_t)(190 * (1.0f - norm_r * 0.7f));
        uint8_t halo_b = (uint8_t)(110 * (1.0f - norm_r));
        uint16_t halo_col = rgb565(halo_r, halo_g, halo_b);

        for (float a = 0.15f * FAST_PI; a <= 0.85f * FAST_PI; a += 0.045f) {
            int px = cx + (int)((float)r * fast_cos(a) * 1.55f);
            int py_up = cy - (int)((float)r * fast_sin(a) * 1.1f);
            int py_dn = cy + (int)((float)r * fast_sin(a) * 1.1f);

            if (px >= 0 && px < DISP_W && py_up >= 0 && py_up < DISP_H && py_dn >= 0 && py_dn < DISP_H) {
                if (num_halo_pts < 900) {
                    halo_pts[num_halo_pts].px = (uint8_t)px;
                    halo_pts[num_halo_pts].py_up = (uint8_t)py_up;
                    halo_pts[num_halo_pts].py_dn = (uint8_t)py_dn;
                    halo_pts[num_halo_pts].col = halo_col;
                    num_halo_pts++;
                }
            }
        }
    }
}

static void blackhole_cleanup(void) {
}

static void blackhole_update(float dt) {
    (void)dt;
    hole_rot += 0.045f;
    if (hole_rot > 2.0f * FAST_PI) hole_rot -= 2.0f * FAST_PI;

    for (int i = 0; i < 180; i++) {
        disk_pts[i].angle += disk_pts[i].speed * 0.08f;
        if (disk_pts[i].angle > 2.0f * FAST_PI) disk_pts[i].angle -= 2.0f * FAST_PI;
    }
}

static void blackhole_render(uint16_t *fb) {
    memset(fb, 0, DISP_W * DISP_H * sizeof(uint16_t));

    const int cx = 120;
    const int cy = 120;
    const int r_event = 24; // 视界半径 (黑洞阴影)

    // 1. 快速绘制静态引力扭曲星辰 (零浮点计算)
    for (int i = 0; i < num_pre_stars; i++) {
        fb[pre_stars[i].py * DISP_W + pre_stars[i].px] = pre_stars[i].color;
    }

    // 2. 快速绘制引力透镜上下光弧 (预计算快速直写)
    for (int i = 0; i < num_halo_pts; i++) {
        fb[halo_pts[i].py_up * DISP_W + halo_pts[i].px] = halo_pts[i].col;
        fb[halo_pts[i].py_dn * DISP_W + halo_pts[i].px] = halo_pts[i].col;
    }

    // 3. 绘制旋转高速相对论吸积盘粒子群 (带多普勒不对称增亮)
    for (int i = 0; i < 180; i++) {
        float a = disk_pts[i].angle;
        float rad = disk_pts[i].radius;

        // 倾斜椭圆投影视角
        float x_world = rad * fast_cos(a);
        float y_world = rad * fast_sin(a) * 0.28f; // 倾斜压扁

        int px = cx + (int)x_world;
        int py = cy + (int)y_world;

        // 多普勒效应：左侧朝向观察者飞来 (多普勒极度增亮、蓝移)，右侧远离 (红移渐暗)
        float doppler = 1.0f - (x_world * 0.00909f);
        if (doppler < 0.25f) doppler = 0.25f;

        uint8_t pr, pg, pb;
        if (doppler > 1.1f) {
            // 高能蓝白移
            pr = (uint8_t)(200 * doppler > 255 ? 255 : 200 * doppler);
            pg = (uint8_t)(230 * doppler > 255 ? 255 : 230 * doppler);
            pb = 255;
        } else {
            // 正常金橙至红移衰暗
            pr = (uint8_t)(255 * doppler);
            pg = (uint8_t)(140 * doppler * doppler);
            pb = (uint8_t)(30 * doppler);
        }

        uint16_t pt_col = rgb565(pr, pg, pb);

        if (px >= 1 && px < DISP_W - 1 && py >= 1 && py < DISP_H - 1) {
            fb[py * DISP_W + px] = pt_col;
            if (disk_pts[i].layer == 0 || doppler > 1.25f) {
                fb[py * DISP_W + px + 1] = pt_col;
                fb[(py + 1) * DISP_W + px] = pt_col;
            }
        }
    }

    // 4. 绘制中心绝对黑暗事件视界黑洞球心 (单圆半径 24px)
    for (int dy = -r_event; dy <= r_event; dy++) {
        int py = cy + dy;
        int max_dx = (int)sqrtf((float)(r_event * r_event - dy * dy));
        for (int dx = -max_dx; dx <= max_dx; dx++) {
            fb[py * DISP_W + (cx + dx)] = COLOR_BLACK;
        }
    }

    // 5. 极细视界边缘高能光子圈 (Photon Ring)
    for (float a = 0; a < 2.0f * FAST_PI; a += 0.08f) {
        int px = cx + (int)((float)r_event * fast_cos(a));
        int py = cy + (int)((float)r_event * fast_sin(a));
        if (px >= 0 && px < DISP_W && py >= 0 && py < DISP_H) {
            fb[py * DISP_W + px] = rgb565(255, 230, 180);
        }
    }
}

Effect fx_blackhole = {
    .id = "blackhole",
    .title = "» GRAVITATIONAL HOLE «",
    .subtitle = "星际穿越爱因斯坦相对论引力黑洞",
    .init = blackhole_init,
    .update = blackhole_update,
    .render = blackhole_render,
    .cleanup = blackhole_cleanup
};
