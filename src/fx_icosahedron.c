#include "effects.h"
#include "fast_math.h"
#include <string.h>
#include <math.h>

#define NUM_ICO_VERTS 12
#define NUM_ICO_EDGES 30

typedef struct {
    float x, y, z;
} Vec3;

typedef struct {
    int u, v;
} Edge;

static Vec3 ico_verts[NUM_ICO_VERTS];
static Edge ico_edges[NUM_ICO_EDGES];
static float rot_x = 0.0f;
static float rot_y = 0.0f;
static float rot_z = 0.0f;
static float anim_time = 0.0f;

static void icosahedron_init(void) {
    anim_time = 0.0f;
    rot_x = 0.3f;
    rot_y = 0.0f;
    rot_z = 0.0f;

    float phi = 1.6180339887f;
    float scale = 48.0f;

    ico_verts[0]  = (Vec3){ -1.0f * scale,  phi * scale, 0.0f };
    ico_verts[1]  = (Vec3){  1.0f * scale,  phi * scale, 0.0f };
    ico_verts[2]  = (Vec3){ -1.0f * scale, -phi * scale, 0.0f };
    ico_verts[3]  = (Vec3){  1.0f * scale, -phi * scale, 0.0f };

    ico_verts[4]  = (Vec3){ 0.0f, -1.0f * scale,  phi * scale };
    ico_verts[5]  = (Vec3){ 0.0f,  1.0f * scale,  phi * scale };
    ico_verts[6]  = (Vec3){ 0.0f, -1.0f * scale, -phi * scale };
    ico_verts[7]  = (Vec3){ 0.0f,  1.0f * scale, -phi * scale };

    ico_verts[8]  = (Vec3){  phi * scale, 0.0f, -1.0f * scale };
    ico_verts[9]  = (Vec3){  phi * scale, 0.0f,  1.0f * scale };
    ico_verts[10] = (Vec3){ -phi * scale, 0.0f, -1.0f * scale };
    ico_verts[11] = (Vec3){ -phi * scale, 0.0f,  1.0f * scale };

    float edge_len_sq = (2.0f * scale) * (2.0f * scale) * 1.05f;
    int edge_idx = 0;
    for (int i = 0; i < NUM_ICO_VERTS; i++) {
        for (int j = i + 1; j < NUM_ICO_VERTS; j++) {
            float dx = ico_verts[i].x - ico_verts[j].x;
            float dy = ico_verts[i].y - ico_verts[j].y;
            float dz = ico_verts[i].z - ico_verts[j].z;
            float d2 = dx * dx + dy * dy + dz * dz;
            if (d2 <= edge_len_sq && edge_idx < NUM_ICO_EDGES) {
                ico_edges[edge_idx].u = i;
                ico_edges[edge_idx].v = j;
                edge_idx++;
            }
        }
    }
}

static void icosahedron_cleanup(void) {
}

static void icosahedron_update(float dt) {
    anim_time += dt;
    rot_x += dt * 0.65f;
    rot_y += dt * 0.95f;
    rot_z += dt * 0.45f;
}

static void icosahedron_render(uint16_t *fb) {
    memset(fb, 0, DISP_W * DISP_H * sizeof(uint16_t));
    int cx = DISP_W / 2;
    int cy = DISP_H / 2;

    static int sx[NUM_ICO_VERTS], sy[NUM_ICO_VERTS];
    static float sz[NUM_ICO_VERTS];

    static int sx_in[NUM_ICO_VERTS], sy_in[NUM_ICO_VERTS];

    float cx_rot = cosf(rot_x), sx_rot = sinf(rot_x);
    float cy_rot = cosf(rot_y), sy_rot = sinf(rot_y);

    for (int i = 0; i < NUM_ICO_VERTS; i++) {
        Vec3 v = ico_verts[i];

        // 旋转
        float x1 = v.x * cy_rot + v.z * sy_rot;
        float y1 = v.y;
        float z1 = -v.x * sy_rot + v.z * cy_rot;

        float x2 = x1;
        float y2 = y1 * cx_rot - z1 * sx_rot;
        float z2 = y1 * sx_rot + z1 * cx_rot;

        float fov = 200.0f;
        float scale = fov / (fov + z2);

        sx[i] = cx + (int)(x2 * scale);
        sy[i] = cy + (int)(y2 * scale);
        sz[i] = z2;

        // 内层反向自旋晶核 (0.55倍大小)
        sx_in[i] = cx + (int)(x2 * 0.55f * scale);
        sy_in[i] = cy + (int)(y2 * 0.55f * scale);
    }

    // 1. 绘制外层 30 条空间几何晶棱
    for (int e = 0; e < NUM_ICO_EDGES; e++) {
        int u = ico_edges[e].u;
        int v = ico_edges[e].v;

        float avg_z = (sz[u] + sz[v]) * 0.5f;
        uint16_t col = (avg_z > 0.0f) ? rgb565(0, 240, 255) : rgb565(0, 60, 95);

        display_draw_line(fb, sx[u], sy[u], sx[v], sy[v], col);

        // 绘制内层晶棱
        uint16_t in_col = (avg_z > 0.0f) ? rgb565(255, 60, 160) : rgb565(80, 10, 50);
        display_draw_line(fb, sx_in[u], sy_in[u], sx_in[v], sy_in[v], in_col);

        // 内外棱线骨架连接
        if (e % 2 == 0) {
            display_draw_line(fb, sx[u], sy[u], sx_in[u], sy_in[u], rgb565(0, 90, 130));
        }
    }

    // 2. 绘制 12 个外层发光顶点
    for (int i = 0; i < NUM_ICO_VERTS; i++) {
        display_fill_circle(fb, sx[i], sy[i], 3, (sz[i] > 0.0f) ? COLOR_WHITE : rgb565(0, 140, 190));
    }

    // 3. 核心量子奇点光球
    display_fill_circle(fb, cx, cy, 5, COLOR_WHITE);
    display_draw_circle(fb, cx, cy, 9, rgb565(255, 230, 40));
}

Effect fx_icosahedron = {
    .id = "icosahedron",
    .title = "赛博二十面体水晶",
    .subtitle = "双重嵌套正多面体空间自旋复合晶体网格",
    .init = icosahedron_init,
    .update = icosahedron_update,
    .render = icosahedron_render,
    .cleanup = icosahedron_cleanup
};
