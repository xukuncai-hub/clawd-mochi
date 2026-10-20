#include "effects.h"
#include "fast_math.h"
#include <string.h>
#include <math.h>

#define NUM_FIELD_RINGS 8
#define PTS_PER_RING 24

static float anim_time = 0.0f;
static float spin_angle = 0.0f;

static void pulsar_init(void) {
    anim_time = 0.0f;
    spin_angle = 0.0f;
}

static void pulsar_cleanup(void) {
}

static void pulsar_update(float dt) {
    anim_time += dt;
    spin_angle += dt * 3.8f;
    if (spin_angle > 2.0f * FAST_PI) spin_angle -= 2.0f * FAST_PI;
}

static void pulsar_render(uint16_t *fb) {
    memset(fb, 0, DISP_W * DISP_H * sizeof(uint16_t));
    int cx = DISP_W / 2;
    int cy = DISP_H / 2;

    // 倾斜自转轴与进动角
    float tilt = 0.55f;
    float cos_tilt = cosf(tilt), sin_tilt = sinf(tilt);
    float cos_spin = cosf(spin_angle), sin_spin = sinf(spin_angle);

    // 1. 密集三维偶极子磁力线网格 (Field Line Mesh)
    for (int r = 1; r <= NUM_FIELD_RINGS; r++) {
        float r_scale = 22.0f + (float)r * 11.0f;

        // 沿经线方向对称分布
        for (int m = 0; m < 4; m++) {
            float phi = (float)m * (FAST_PI / 2.0f) + spin_angle * 0.5f;
            float cos_phi = cosf(phi);
            float sin_phi = sinf(phi);

            int last_x = 0, last_y = 0;
            bool first = true;

            for (int p = 0; p <= PTS_PER_RING; p++) {
                // 偶极子参数方程: r_theta = r_scale * sin^2(theta)
                float theta = (float)p * (FAST_PI / (float)PTS_PER_RING);
                float sin_th = sinf(theta);
                float rad = r_scale * sin_th * sin_th;

                float x0 = rad * sin_th * cos_phi;
                float y0 = rad * cosf(theta);
                float z0 = rad * sin_th * sin_phi;

                // 倾斜轴旋转变换
                float x1 = x0;
                float y1 = y0 * cos_tilt - z0 * sin_tilt;
                float z1 = y0 * sin_tilt + z0 * cos_tilt;

                float fov = 180.0f;
                float scale = fov / (fov + z1);
                int sx = cx + (int)(x1 * scale);
                int sy = cy + (int)(y1 * scale);

                if (sx >= 0 && sx < DISP_W && sy >= 0 && sy < DISP_H) {
                    if (!first) {
                        uint8_t hue = (uint8_t)((int)(anim_time * 25.0f + r * 14 + p * 4) & 0xFF);
                        uint16_t col = hsv2rgb565(hue, 220, (z1 > 0) ? 240 : 120);
                        display_draw_line(fb, last_x, last_y, sx, sy, col);
                    }
                    last_x = sx;
                    last_y = sy;
                    first = false;
                }
            }
        }
    }

    // 2. 双向相对论高能喷流光锥 (带高能电弧粒子流)
    float jet_len = 115.0f;
    for (int side = -1; side <= 1; side += 2) {
        float j_angle = spin_angle + ((side < 0) ? FAST_PI : 0.0f);

        for (int c = -4; c <= 4; c++) {
            float cone_a = j_angle + (float)c * 0.035f;
            float jx0 = jet_len * cosf(cone_a);
            float jy0 = jet_len * sinf(cone_a);

            // 倾斜变换
            float jx = (float)cx + jx0;
            float jy = (float)cy + jy0 * cos_tilt;

            float facing = cosf(cone_a);
            uint16_t j_col = (facing > 0.65f) ? COLOR_WHITE : rgb565(0, 240, 255);
            display_draw_line(fb, cx, cy, (int)jx, (int)jy, j_col);
        }
    }

    // 3. 超高密度脉冲中子星核心 (极端白热闪烁耀斑)
    float flash = powf(fabsf(cosf(spin_angle)), 4.0f);
    int core_r = 7 + (int)(flash * 9.0f);
    display_fill_circle(fb, cx, cy, core_r + 4, rgb565(0, 160, 255));
    display_fill_circle(fb, cx, cy, core_r, COLOR_WHITE);
    display_draw_circle(fb, cx, cy, core_r + 7, rgb565(255, 60, 180));
}

Effect fx_pulsar = {
    .id = "pulsar",
    .title = "赛博脉冲星射流",
    .subtitle = "高能相对论自转中子星磁层力线网格",
    .init = pulsar_init,
    .update = pulsar_update,
    .render = pulsar_render,
    .cleanup = pulsar_cleanup
};
