#include "effects.h"
#include "fast_math.h"
#include <string.h>
#include <math.h>

#define CLIFFORD_U 24
#define CLIFFORD_V 16

static float rot_4d_1 = 0.0f;
static float rot_4d_2 = 0.0f;
static float anim_time = 0.0f;

static void tesseract_init(void) {
    rot_4d_1 = 0.0f;
    rot_4d_2 = 0.0f;
    anim_time = 0.0f;
}

static void tesseract_cleanup(void) {
}

static void tesseract_update(float dt) {
    anim_time += dt;
    rot_4d_1 += dt * 0.75f;
    rot_4d_2 += dt * 0.45f;
}

static void tesseract_render(uint16_t *fb) {
    memset(fb, 0, DISP_W * DISP_H * sizeof(uint16_t));
    int cx = DISP_W / 2;
    int cy = DISP_H / 2;

    static int sx[CLIFFORD_V][CLIFFORD_U];
    static int sy[CLIFFORD_V][CLIFFORD_U];
    static float sz[CLIFFORD_V][CLIFFORD_U];

    float c1 = cosf(rot_4d_1), s1 = sinf(rot_4d_1);
    float c2 = cosf(rot_4d_2), s2 = sinf(rot_4d_2);

    for (int v_idx = 0; v_idx < CLIFFORD_V; v_idx++) {
        float v = (float)v_idx * (2.0f * FAST_PI / (float)CLIFFORD_V);
        float cos_v = cosf(v), sin_v = sinf(v);

        for (int u_idx = 0; u_idx < CLIFFORD_U; u_idx++) {
            float u = (float)u_idx * (2.0f * FAST_PI / (float)CLIFFORD_U);
            float cos_u = cosf(u), sin_u = sinf(u);

            // 4D 克利福德环面坐标: (x1, y1, x2, y2)
            float x1 = cos_u;
            float y1 = sin_u;
            float x2 = cos_v;
            float y2 = sin_v;

            // 4D 双平面旋转
            float rx1 = x1 * c1 - y2 * s1;
            float ry2 = x1 * s1 + y2 * c1;

            float rx2 = x2 * c2 - y1 * s2;
            float ry1 = x2 * s2 + y1 * c2;

            // 4D -> 3D 球极立体投影 (从 (0, 0, 0, 2) 投影)
            float p_dist = 2.2f;
            float s_proj = 1.0f / (p_dist - ry2);
            float px3 = rx1 * s_proj;
            float py3 = ry1 * s_proj;
            float pz3 = rx2 * s_proj;

            // 3D -> 2D 透视投影
            float fov = 170.0f;
            float scale = fov / (1.8f - pz3 * 0.5f);

            sx[v_idx][u_idx] = cx + (int)(px3 * scale);
            sy[v_idx][u_idx] = cy + (int)(py3 * scale);
            sz[v_idx][u_idx] = pz3;
        }
    }

    // 绘制 4D 克利福德曲率网格：U 环线 (内层漏斗经线)
    for (int v_idx = 0; v_idx < CLIFFORD_V; v_idx++) {
        uint8_t hue = (uint8_t)((int)(anim_time * 25.0f + v_idx * 16) & 0xFF);
        uint16_t col = hsv2rgb565(hue, 230, 220);

        for (int u_idx = 0; u_idx < CLIFFORD_U; u_idx++) {
            int next_u = (u_idx + 1) % CLIFFORD_U;
            display_draw_line(fb, sx[v_idx][u_idx], sy[v_idx][u_idx], sx[v_idx][next_u], sy[v_idx][next_u], col);
        }
    }

    // 绘制 4D 克利福德曲率网格：V 环线 (外层翻转纬线)
    for (int u_idx = 0; u_idx < CLIFFORD_U; u_idx += 2) {
        for (int v_idx = 0; v_idx < CLIFFORD_V; v_idx++) {
            int next_v = (v_idx + 1) % CLIFFORD_V;
            uint16_t rib_col = (sz[v_idx][u_idx] > 0.0f) ? rgb565(0, 240, 255) : rgb565(0, 70, 110);
            display_draw_line(fb, sx[v_idx][u_idx], sy[v_idx][u_idx], sx[next_v][u_idx], sy[next_v][u_idx], rib_col);
        }
    }

    // 中心高维奇点光核
    float p_pulse = sinf(anim_time * 6.0f);
    int core_r = (int)(6.0f + p_pulse * 2.0f);
    display_fill_circle(fb, cx, cy, core_r, COLOR_WHITE);
    display_draw_circle(fb, cx, cy, core_r + 4, rgb565(0, 220, 255));
}

Effect fx_tesseract = {
    .id = "tesseract",
    .title = "赛博4D超环面曲率",
    .subtitle = "4D克利福德高维双自旋嵌套曲面",
    .init = tesseract_init,
    .update = tesseract_update,
    .render = tesseract_render,
    .cleanup = tesseract_cleanup
};
