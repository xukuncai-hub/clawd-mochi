#include "effects.h"
#include "fast_math.h"
#include <string.h>
#include <math.h>

#define BH_GRID_COLS 16
#define BH_GRID_ROWS 16
#define NUM_DISK_STREAMS 36

static float hole_rot = 0.0f;
static float anim_time = 0.0f;

static void blackhole_init(void) {
    hole_rot = 0.0f;
    anim_time = 0.0f;
}

static void blackhole_cleanup(void) {
}

static void blackhole_update(float dt) {
    anim_time += dt;
    hole_rot += dt * 1.6f;
}

static void blackhole_render(uint16_t *fb) {
    memset(fb, 0, DISP_W * DISP_H * sizeof(uint16_t));
    int cx = DISP_W / 2;
    int cy = DISP_H / 2;

    static int px[BH_GRID_ROWS][BH_GRID_COLS];
    static int py[BH_GRID_ROWS][BH_GRID_COLS];

    float col_step = (float)(DISP_W - 16) / (float)(BH_GRID_COLS - 1);
    float row_step = (float)(DISP_H - 16) / (float)(BH_GRID_ROWS - 1);

    // 1. 爱因斯坦引力透镜偏折时空网格 (Spacetime Grid Under Lensing)
    for (int r = 0; r < BH_GRID_ROWS; r++) {
        for (int c = 0; c < BH_GRID_COLS; c++) {
            float ox = 8.0f + (float)c * col_step;
            float oy = 8.0f + (float)r * row_step;

            float dx = ox - (float)cx;
            float dy = oy - (float)cy;
            float dist = sqrtf(dx * dx + dy * dy) + 1.0f;

            // 施瓦西引力透镜弯曲公式
            float lens_warp = 480.0f / (dist * 0.8f + 18.0f);
            // 伴随引力吸积涡旋旋转偏折
            float angle = atan2f(dy, dx) + (18.0f / (dist + 12.0f));
            float new_dist = dist - lens_warp;
            if (new_dist < 15.0f) new_dist = 15.0f;

            px[r][c] = cx + (int)(new_dist * cosf(angle));
            py[r][c] = cy + (int)(new_dist * sinf(angle));
        }
    }

    // 绘制横向透镜弯曲网格线
    for (int r = 0; r < BH_GRID_ROWS; r++) {
        uint16_t col = rgb565(0, 70, (uint8_t)(90 + r * 6));
        for (int c = 0; c < BH_GRID_COLS - 1; c++) {
            display_draw_line(fb, px[r][c], py[r][c], px[r][c + 1], py[r][c + 1], col);
        }
    }

    // 绘制纵向透镜弯曲网格线
    for (int c = 0; c < BH_GRID_COLS; c++) {
        uint16_t col = rgb565(0, 70, (uint8_t)(90 + c * 6));
        for (int r = 0; r < BH_GRID_ROWS - 1; r++) {
            display_draw_line(fb, px[r][c], py[r][c], px[r + 1][c], py[r + 1][c], col);
        }
    }

    // 2. 相对论超高速开普勒吸积盘 (Accretion Disk)
    for (int i = 0; i < NUM_DISK_STREAMS; i++) {
        float rad = 32.0f + (float)i * 1.9f;
        float speed = 3.5f / sqrtf(rad * 0.05f);
        float a = hole_rot * speed + (float)i * 0.4f;

        // 倾斜椭圆吸积盘投影
        int ax1 = cx + (int)(rad * cosf(a));
        int ay1 = cy + (int)(rad * 0.42f * sinf(a) - 4.0f);
        int ax2 = cx + (int)((rad + 6.0f) * cosf(a + 0.35f));
        int ay2 = cy + (int)((rad + 6.0f) * 0.42f * sinf(a + 0.35f) - 4.0f);

        // 多普勒频移：左侧朝向观察者高速运动偏蓝白发亮，右侧远离偏暗红
        uint16_t disk_col;
        if (cosf(a) < 0.0f) {
            disk_col = (cosf(a) < -0.6f) ? COLOR_WHITE : rgb565(120, 220, 255);
        } else {
            disk_col = rgb565(255, 90, 20);
        }

        display_draw_line(fb, ax1, ay1, ax2, ay2, disk_col);
    }

    // 3. 爱因斯坦环 (Einstein Ring 辉光)
    display_draw_circle(fb, cx, cy, 32, rgb565(255, 230, 140));
    display_draw_circle(fb, cx, cy, 33, COLOR_WHITE);
    display_draw_circle(fb, cx, cy, 34, rgb565(0, 200, 255));

    // 4. 事件视界 (Event Horizon 绝对纯黑与内边缘吸积光弧)
    display_fill_circle(fb, cx, cy, 26, COLOR_BLACK);
    display_draw_circle(fb, cx, cy, 26, rgb565(255, 60, 20));
}

Effect fx_blackhole = {
    .id = "blackhole",
    .title = "引力透镜黑洞",
    .subtitle = "爱因斯坦引力透镜时空网格弯曲与吸积盘",
    .init = blackhole_init,
    .update = blackhole_update,
    .render = blackhole_render,
    .cleanup = blackhole_cleanup
};
