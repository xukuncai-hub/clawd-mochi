#include "effects.h"
#include "fast_math.h"
#include <string.h>
#include <math.h>

#define NUM_3D_STREAMS 40

typedef struct {
    float x, y, z;
    float speed;
    int len;
} Stream3D;

static Stream3D streams[NUM_3D_STREAMS];
static float anim_time = 0.0f;

static void respawn_stream(Stream3D *s) {
    s->x = (float)((fast_rand() % 240) - 120);
    s->y = (float)((fast_rand() % 240) - 120);
    s->z = 190.0f + (float)(fast_rand() % 50);
    s->speed = 70.0f + (float)(fast_rand() % 85);
    s->len = 8 + (fast_rand() % 10);
}

static void matrix_3d_init(void) {
    anim_time = 0.0f;
    for (int i = 0; i < NUM_3D_STREAMS; i++) {
        respawn_stream(&streams[i]);
        streams[i].z = 15.0f + (float)(fast_rand() % 190);
    }
}

static void matrix_3d_cleanup(void) {
}

static void matrix_3d_update(float dt) {
    anim_time += dt;
    for (int i = 0; i < NUM_3D_STREAMS; i++) {
        streams[i].z -= streams[i].speed * dt;
        if (streams[i].z < 10.0f) {
            respawn_stream(&streams[i]);
        }
    }
}

static void matrix_3d_render(uint16_t *fb) {
    memset(fb, 0, DISP_W * DISP_H * sizeof(uint16_t));
    int cx = DISP_W / 2;
    int cy = DISP_H / 2;
    float fov = 115.0f;

    // 1. 空间深邃数据网格地平线
    for (int ring = 1; ring <= 3; ring++) {
        float z_ring = (float)ring * 45.0f;
        float scale = fov / z_ring;
        int r_disp = (int)(90.0f * scale);
        display_draw_circle(fb, cx, cy, r_disp, rgb565(0, 30, 45));
    }

    // 2. 绘制 40 条全景深 3D 数字流
    for (int i = 0; i < NUM_3D_STREAMS; i++) {
        Stream3D *s = &streams[i];

        for (int l = 0; l < s->len; l++) {
            float cur_z = s->z + (float)l * 6.0f;
            if (cur_z <= 5.0f || cur_z > 220.0f) continue;

            float scale = fov / cur_z;
            int px = cx + (int)(s->x * scale);
            int py = cy + (int)((s->y + (float)l * 5.0f) * scale);

            if (px < 1 || px >= DISP_W - 2 || py < 1 || py >= DISP_H - 2) continue;

            uint16_t col;
            if (l == 0) {
                col = COLOR_WHITE; // 头部白热
            } else {
                float fade = 1.0f - ((float)l / (float)s->len);
                uint8_t g = (uint8_t)(fade * 255.0f);
                col = rgb565(0, g, (uint8_t)(g * 0.4f));
            }

            display_draw_pixel(fb, px, py, col);
            if (cur_z < 55.0f) {
                // 近处双倍加粗光点
                display_draw_pixel(fb, px + 1, py, col);
                display_draw_pixel(fb, px, py + 1, col);
                display_draw_pixel(fb, px + 1, py + 1, col);
            }
        }
    }
}

Effect fx_matrix_3d = {
    .id = "matrix_3d",
    .title = "赛博3D矩阵代码瀑布",
    .subtitle = "全景深数字流时空网格高速冲屏",
    .init = matrix_3d_init,
    .update = matrix_3d_update,
    .render = matrix_3d_render,
    .cleanup = matrix_3d_cleanup
};
