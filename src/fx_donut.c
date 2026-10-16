#include "effects.h"
#include "fast_math.h"
#include <string.h>
#include <stdlib.h>

#define GRID_SIZE 120

static float A = 0.0f;
static float B = 0.0f;
static uint8_t z_buf[GRID_SIZE * GRID_SIZE];
static uint16_t rainbow_lut[256];

static void hsv_to_rgb_init(float h, float s, float v, uint8_t *r, uint8_t *g, uint8_t *b) {
    float c = v * s;
    float x = c * (1.0f - fabsf(fmodf(h / 60.0f, 2.0f) - 1.0f));
    float m = v - c;
    float rf = 0, gf = 0, bf = 0;

    if (h < 60)       { rf = c; gf = x; bf = 0; }
    else if (h < 120) { rf = x; gf = c; bf = 0; }
    else if (h < 180) { rf = 0; gf = c; bf = x; }
    else if (h < 240) { rf = 0; gf = x; bf = c; }
    else if (h < 300) { rf = x; gf = 0; bf = c; }
    else              { rf = c; gf = 0; bf = x; }

    *r = (uint8_t)((rf + m) * 255.0f);
    *g = (uint8_t)((gf + m) * 255.0f);
    *b = (uint8_t)((bf + m) * 255.0f);
}

static void donut_init(void) {
    // 预热 256 色炫彩虹光表
    for (int i = 0; i < 256; i++) {
        uint8_t r, g, b;
        float h = (float)i * 360.0f / 256.0f;
        hsv_to_rgb_init(h, 0.9f, 1.0f, &r, &g, &b);
        rainbow_lut[i] = rgb565(r, g, b);
    }
}

static void donut_cleanup(void) {
}

static void donut_update(float dt) {
    (void)dt;
    A += 0.065f;
    B += 0.038f;
    if (A > 2.0f * FAST_PI) A -= 2.0f * FAST_PI;
    if (B > 2.0f * FAST_PI) B -= 2.0f * FAST_PI;
}

static void donut_render(uint16_t *fb) {
    memset(z_buf, 0, sizeof(z_buf));
    memset(fb, 0, DISP_W * DISP_H * sizeof(uint16_t));

    float cosA = fast_cos(A), sinA = fast_sin(A);
    float cosB = fast_cos(B), sinB = fast_sin(B);

    const float R1 = 1.0f;
    const float R2 = 2.0f;
    const float K2 = 5.0f;
    const float K1 = 145.0f;

    // 预提外层与常数项，代数合并光照与三维旋转方程
    const float THETA_STEP = 0.22f;
    const float PHI_STEP = 0.09f;

    for (float theta = 0; theta < 2.0f * FAST_PI; theta += THETA_STEP) {
        float costheta = fast_cos(theta), sintheta = fast_sin(theta);
        float circlex = R2 + R1 * costheta;
        float circley = R1 * sintheta;

        float circlex_cosB = circlex * cosB;
        float circlex_sinB = circlex * sinB;
        float circlex_sinA_sinB = circlex * sinA * sinB;
        float circlex_sinA_cosB = circlex * sinA * cosB;
        float circlex_cosA = circlex * cosA;
        float cy_cosA_sinB = circley * cosA * sinB;
        float cy_cosA_cosB = circley * cosA * cosB;
        float cy_sinA = circley * sinA;

        // 光照项代数分解提纯：L = cosphi * L_cos + sinphi * L_sin + L_const
        float L_cos = costheta * sinB;
        float L_sin = -cosA * costheta - cosB * costheta * sinA;
        float L_const = -sinA * sintheta + cosB * cosA * sintheta;

        for (float phi = 0; phi < 2.0f * FAST_PI; phi += PHI_STEP) {
            float cosphi = fast_cos(phi), sinphi = fast_sin(phi);

            float L = cosphi * L_cos + sinphi * L_sin + L_const;
            if (L <= 0.0f) continue; // 背面与暗面直接提前剔除

            float z = K2 + circlex_cosA * sinphi + cy_sinA;
            float ooz = 1.0f / z;

            float x = circlex_cosB * cosphi + circlex_sinA_sinB * sinphi - cy_cosA_sinB;
            float y = circlex_sinB * cosphi - circlex_sinA_cosB * sinphi + cy_cosA_cosB;

            int xp = (GRID_SIZE / 2) + (int)(K1 * ooz * x);
            int yp = (GRID_SIZE / 2) - (int)(K1 * ooz * y);

            if (xp >= 0 && xp < GRID_SIZE && yp >= 0 && yp < GRID_SIZE) {
                int idx = yp * GRID_SIZE + xp;
                uint8_t ooz_u8 = (uint8_t)(ooz * 500.0f);
                if (ooz_u8 > z_buf[idx]) {
                    z_buf[idx] = ooz_u8;

                    uint8_t hue_idx = (uint8_t)((phi + A * 2.0f) * (256.0f / (2.0f * FAST_PI)));
                    uint16_t color = rainbow_lut[hue_idx];

                    // 2x 极速双像素 32 位成对写入
                    int sx = xp * 2;
                    int sy = yp * 2;
                    uint32_t c32 = ((uint32_t)color << 16) | color;
                    *(uint32_t *)&fb[sy * DISP_W + sx] = c32;
                    *(uint32_t *)&fb[(sy + 1) * DISP_W + sx] = c32;
                }
            }
        }
    }
}

Effect fx_donut = {
    .id = "donut",
    .title = "» 3D SHADED TORUS «",
    .subtitle = "3D 极速光影旋转甜甜圈",
    .init = donut_init,
    .update = donut_update,
    .render = donut_render,
    .cleanup = donut_cleanup
};
