#include "effects.h"
#include "fast_math.h"
#include <string.h>
#include <math.h>

#define Q_GRID_COLS 18
#define Q_GRID_ROWS 18
#define NUM_Q_SOURCES 3

typedef struct {
    float x, y;
    float freq;
    float speed;
} QSource;

static float anim_time = 0.0f;
static QSource sources[NUM_Q_SOURCES];

static void quantum_init(void) {
    anim_time = 0.0f;
    sources[0] = (QSource){ 80.0f, 120.0f, 0.08f, 3.2f };
    sources[1] = (QSource){ 160.0f, 120.0f, 0.08f, -2.8f };
    sources[2] = (QSource){ 120.0f, 60.0f, 0.11f, 4.0f };
}

static void quantum_cleanup(void) {
}

static void quantum_update(float dt) {
    anim_time += dt;
    sources[0].x = 120.0f + 55.0f * cosf(anim_time * 1.3f);
    sources[0].y = 120.0f + 45.0f * sinf(anim_time * 1.3f);

    sources[1].x = 120.0f + 55.0f * cosf(anim_time * 1.3f + FAST_PI);
    sources[1].y = 120.0f + 45.0f * sinf(anim_time * 1.3f + FAST_PI);

    sources[2].x = 120.0f + 35.0f * sinf(anim_time * 2.2f);
    sources[2].y = 120.0f + 50.0f * cosf(anim_time * 1.7f);
}

static void quantum_render(uint16_t *fb) {
    memset(fb, 0, DISP_W * DISP_H * sizeof(uint16_t));

    static int px[Q_GRID_ROWS][Q_GRID_COLS];
    static int py[Q_GRID_ROWS][Q_GRID_COLS];

    float col_step = (float)(DISP_W - 16) / (float)(Q_GRID_COLS - 1);
    float row_step = (float)(DISP_H - 16) / (float)(Q_GRID_ROWS - 1);

    for (int r = 0; r < Q_GRID_ROWS; r++) {
        for (int c = 0; c < Q_GRID_COLS; c++) {
            float ox = 8.0f + (float)c * col_step;
            float oy = 8.0f + (float)r * row_step;

            float wave_total = 0.0f;
            float grad_x = 0.0f;
            float grad_y = 0.0f;

            for (int s = 0; s < NUM_Q_SOURCES; s++) {
                float dx = ox - sources[s].x;
                float dy = oy - sources[s].y;
                float dist = sqrtf(dx * dx + dy * dy) + 0.1f;
                float phase = dist * sources[s].freq - anim_time * sources[s].speed;

                float amp = sinf(phase);
                wave_total += amp;

                // 梯度位移向量 (沿波阵面传播方向拉伸)
                float cos_p = cosf(phase);
                grad_x += (dx / dist) * cos_p * 4.5f;
                grad_y += (dy / dist) * cos_p * 4.5f;
            }

            // 振幅驱动网格顶点空间位移
            px[r][c] = (int)(ox + grad_x);
            py[r][c] = (int)(oy + grad_y);
        }
    }

    // 绘制横向量子干涉波纹线
    for (int r = 0; r < Q_GRID_ROWS; r++) {
        uint8_t hue = (uint8_t)((int)(anim_time * 30.0f + r * 9) & 0xFF);
        uint16_t col = hsv2rgb565(hue, 230, 220);
        for (int c = 0; c < Q_GRID_COLS - 1; c++) {
            display_draw_line(fb, px[r][c], py[r][c], px[r][c + 1], py[r][c + 1], col);
        }
    }

    // 绘制纵向量子干涉波纹线
    for (int c = 0; c < Q_GRID_COLS; c++) {
        uint8_t hue = (uint8_t)((int)(anim_time * 30.0f + c * 9 + 110) & 0xFF);
        uint16_t col = hsv2rgb565(hue, 230, 220);
        for (int r = 0; r < Q_GRID_ROWS - 1; r++) {
            display_draw_line(fb, px[r][c], py[r][c], px[r + 1][c], py[r + 1][c], col);
        }
    }

    // 绘制 3 个量子相干源核 (高能发光)
    for (int s = 0; s < NUM_Q_SOURCES; s++) {
        display_fill_circle(fb, (int)sources[s].x, (int)sources[s].y, 5, rgb565(0, 240, 255));
        display_fill_circle(fb, (int)sources[s].x, (int)sources[s].y, 2, COLOR_WHITE);
        display_draw_circle(fb, (int)sources[s].x, (int)sources[s].y, 9, rgb565(255, 60, 180));
    }
}

Effect fx_quantum = {
    .id = "quantum",
    .title = "赛博量子波函数干涉",
    .subtitle = "多相位相干几率波高密度曲率网格",
    .init = quantum_init,
    .update = quantum_update,
    .render = quantum_render,
    .cleanup = quantum_cleanup
};
