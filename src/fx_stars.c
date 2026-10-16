#include "effects.h"
#include "fast_math.h"
#include <stdlib.h>
#include <string.h>

#define NUM_STARS 140

typedef struct {
    float x, y, z;
} Star;

typedef struct {
    float x, y, z;
} Vec3;

static Star stars[NUM_STARS];
static float rot_x = 0.0f;
static float rot_y = 0.0f;
static float rot_z = 0.0f;

static void reset_star(Star *s) {
    s->x = (float)(fast_rand_range(0, 2400)) - 1200.0f;
    s->y = (float)(fast_rand_range(0, 2400)) - 1200.0f;
    s->z = 400.0f + (float)(fast_rand_range(0, 600));
}

static void stars_init(void) {
    for (int i = 0; i < NUM_STARS; i++) {
        stars[i].x = (float)(fast_rand_range(0, 2400)) - 1200.0f;
        stars[i].y = (float)(fast_rand_range(0, 2400)) - 1200.0f;
        stars[i].z = (float)(fast_rand_range(50, 1000));
    }
}

static void stars_cleanup(void) {
}

static void stars_update(float dt) {
    (void)dt;

    // 更新星空飞掠
    for (int i = 0; i < NUM_STARS; i++) {
        stars[i].z -= 26.0f;
        if (stars[i].z <= 15.0f) {
            reset_star(&stars[i]);
        }
    }

    // 更新立方体三轴自转
    rot_x += 0.040f;
    rot_y += 0.052f;
    rot_z += 0.028f;
    if (rot_x > 2.0f * FAST_PI) rot_x -= 2.0f * FAST_PI;
    if (rot_y > 2.0f * FAST_PI) rot_y -= 2.0f * FAST_PI;
    if (rot_z > 2.0f * FAST_PI) rot_z -= 2.0f * FAST_PI;
}

static Vec3 rotate_v(Vec3 v, float rx, float ry, float rz) {
    // 绕 X
    float cosX = fast_cos(rx), sinX = fast_sin(rx);
    float y1 = v.y * cosX - v.z * sinX;
    float z1 = v.y * sinX + v.z * cosX;
    // 绕 Y
    float cosY = fast_cos(ry), sinY = fast_sin(ry);
    float x2 = v.x * cosY + z1 * sinY;
    float z2 = -v.x * sinY + z1 * cosY;
    // 绕 Z
    float cosZ = fast_cos(rz), sinZ = fast_sin(rz);
    float x3 = x2 * cosZ - y1 * sinZ;
    float y3 = x2 * sinZ + y1 * cosZ;

    Vec3 res = { x3, y3, z2 };
    return res;
}

static bool project_pt(Vec3 p, int *sx, int *sy) {
    float dist = 2.8f;
    float z = p.z + dist;
    if (z <= 0.2f) return false;

    float fov = 160.0f;
    *sx = DISP_W / 2 + (int)(p.x * fov / z);
    *sy = DISP_H / 2 - (int)(p.y * fov / z);
    return (*sx >= 0 && *sx < DISP_W && *sy >= 0 && *sy < DISP_H);
}

static void stars_render(uint16_t *fb) {
    memset(fb, 0, DISP_W * DISP_H * sizeof(uint16_t));
    int cx = DISP_W / 2;
    int cy = DISP_H / 2;

    // 1. 渲染超空间飞掠星空与速度拖尾
    for (int i = 0; i < NUM_STARS; i++) {
        float z = stars[i].z;
        if (z <= 0.0f) continue;

        int sx = cx + (int)(stars[i].x * 90.0f / z);
        int sy = cy + (int)(stars[i].y * 90.0f / z);

        float pz = z + 35.0f;
        int px = cx + (int)(stars[i].x * 90.0f / pz);
        int py = cy + (int)(stars[i].y * 90.0f / pz);

        if (sx >= 0 && sx < DISP_W && sy >= 0 && sy < DISP_H) {
            float depth = 1.0f - (z / 1000.0f);
            if (depth < 0.1f) depth = 0.1f;

            uint8_t r = (uint8_t)(80 * depth);
            uint8_t g = (uint8_t)(180 * depth);
            uint8_t b = (uint8_t)(255 * depth);
            uint16_t star_col = rgb565(r, g, b);

            if (depth > 0.4f && (px != sx || py != sy)) {
                display_draw_line(fb, px, py, sx, sy, rgb565(r / 2, g / 2, b / 2));
            }
            display_draw_pixel(fb, sx, sy, depth > 0.8f ? COLOR_WHITE : star_col);
        }
    }

    // 2. 渲染中心 3D 旋转超立方体
    static const Vec3 cube_verts[8] = {
        {-0.9f, -0.9f, -0.9f}, { 0.9f, -0.9f, -0.9f},
        { 0.9f,  0.9f, -0.9f}, {-0.9f,  0.9f, -0.9f},
        {-0.9f, -0.9f,  0.9f}, { 0.9f, -0.9f,  0.9f},
        { 0.9f,  0.9f,  0.9f}, {-0.9f,  0.9f,  0.9f}
    };

    static const int edges[12][2] = {
        {0, 1}, {1, 2}, {2, 3}, {3, 0},
        {4, 5}, {5, 6}, {6, 7}, {7, 4},
        {0, 4}, {1, 5}, {2, 6}, {3, 7}
    };

    int px[8], py[8];
    bool valid[8];

    for (int i = 0; i < 8; i++) {
        Vec3 rot = rotate_v(cube_verts[i], rot_x, rot_y, rot_z);
        valid[i] = project_pt(rot, &px[i], &py[i]);
    }

    // 绘制 12 条发光霓虹彩棱边
    for (int i = 0; i < 12; i++) {
        int u = edges[i][0];
        int v = edges[i][1];
        if (valid[u] && valid[v]) {
            uint8_t er = (uint8_t)(128 + 127 * fast_sin(rot_x + i * 0.5f));
            uint8_t eg = (uint8_t)(128 + 127 * fast_cos(rot_y + i * 0.5f));
            uint8_t eb = 255;
            display_draw_line(fb, px[u], py[u], px[v], py[v], rgb565(er, eg, eb));
        }
    }

    // 绘制 8 个外层顶点核
    for (int i = 0; i < 8; i++) {
        if (valid[i]) {
            display_fill_rect(fb, px[i] - 1, py[i] - 1, 3, 3, COLOR_WHITE);
        }
    }

    // 3. 内部自转的脉冲核心八面体
    static const Vec3 core_verts[6] = {
        { 0.0f,  0.5f,  0.0f}, { 0.0f, -0.5f,  0.0f},
        { 0.5f,  0.0f,  0.0f}, {-0.5f,  0.0f,  0.0f},
        { 0.0f,  0.0f,  0.5f}, { 0.0f,  0.0f, -0.5f}
    };
    static const int core_edges[12][2] = {
        {0, 2}, {0, 3}, {0, 4}, {0, 5},
        {1, 2}, {1, 3}, {1, 4}, {1, 5},
        {2, 4}, {4, 3}, {3, 5}, {5, 2}
    };

    int c_px[6], c_py[6];
    bool c_valid[6];
    for (int i = 0; i < 6; i++) {
        Vec3 rot = rotate_v(core_verts[i], -rot_y * 1.5f, -rot_x * 1.5f, rot_z * 2.0f);
        c_valid[i] = project_pt(rot, &c_px[i], &c_py[i]);
    }
    for (int i = 0; i < 12; i++) {
        int u = core_edges[i][0];
        int v = core_edges[i][1];
        if (c_valid[u] && c_valid[v]) {
            display_draw_line(fb, c_px[u], c_py[u], c_px[v], c_py[v], COLOR_YELLOW);
        }
    }
}

Effect fx_stars = {
    .id = "stars",
    .title = "» HYPERSPACE & CUBE «",
    .subtitle = "超空间星空与 3D 旋转立方体",
    .init = stars_init,
    .update = stars_update,
    .render = stars_render,
    .cleanup = stars_cleanup
};
