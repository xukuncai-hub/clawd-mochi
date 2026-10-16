#include "effects.h"
#include "fast_math.h"
#include <string.h>
#include <stdlib.h>

#define NUM_BARS 16
#define BASE_Y 145
#define MAX_BAR_H 100
#define NUM_BEAT_SPARKS 30

typedef struct {
    float height;
    float peak_h;
    float peak_decay;
    float target;
} EqBar;

typedef struct {
    float x, y;
    float vx, vy;
    int life;
    uint16_t color;
} Spark;

static EqBar eq_bars[NUM_BARS];
static Spark sparks[NUM_BEAT_SPARKS];
static float beat_phase = 0.0f;
static float beat_timer = 0.0f;

static uint16_t spec_lut[MAX_BAR_H + 1];

static void spectrum_init(void) {
    for (int i = 0; i < NUM_BARS; i++) {
        eq_bars[i].height = 10.0f;
        eq_bars[i].peak_h = 15.0f;
        eq_bars[i].peak_decay = 0.0f;
        eq_bars[i].target = 20.0f;
    }
    for (int i = 0; i < NUM_BEAT_SPARKS; i++) {
        sparks[i].life = 0;
    }
    beat_phase = 0.0f;
    beat_timer = 0.0f;

    // 预计算 100 阶垂直色彩渐变查找表 (消除每像素软浮点除法与条件分支)
    for (int dy = 0; dy <= MAX_BAR_H; dy++) {
        float f = (float)dy / (float)MAX_BAR_H;
        if (f < 0.45f) {
            uint8_t g = (uint8_t)(160 + 95 * (f / 0.45f));
            spec_lut[dy] = rgb565(0, g, 240);
        } else if (f < 0.75f) {
            float t = (f - 0.45f) / 0.30f;
            spec_lut[dy] = rgb565((uint8_t)(255 * t), 240, 20);
        } else {
            float t = (f - 0.75f) / 0.25f;
            spec_lut[dy] = rgb565(255, (uint8_t)(180 * (1.0f - t)), (uint8_t)(240 * t));
        }
    }
}

static void spectrum_cleanup(void) {
}

static void spawn_spark(float x, float y, uint16_t col) {
    for (int i = 0; i < NUM_BEAT_SPARKS; i++) {
        if (sparks[i].life <= 0) {
            sparks[i].x = x;
            sparks[i].y = y;
            sparks[i].vx = ((float)(fast_rand() % 100) - 50.0f) * 0.04f;
            sparks[i].vy = -((float)(fast_rand() % 100) * 0.05f) - 1.2f;
            sparks[i].life = 25 + (fast_rand() % 20);
            sparks[i].color = col;
            break;
        }
    }
}

static void spectrum_update(float dt) {
    (void)dt;
    beat_phase += 0.08f;
    if (beat_phase > 2.0f * FAST_PI) beat_phase -= 2.0f * FAST_PI;

    beat_timer += 0.12f;
    bool bass_kick = (fast_sin(beat_timer * 1.8f) > 0.88f);

    for (int i = 0; i < NUM_BARS; i++) {
        // 多八度频谱节奏模拟
        float freq_mod = fast_sin(beat_phase * 2.2f + (float)i * 0.45f);
        float rhythm = (freq_mod > 0.0f ? freq_mod : 0.0f);
        float h_target = 15.0f + rhythm * (float)(MAX_BAR_H - 20);

        if (i < 4 && bass_kick) {
            h_target = 80.0f + (float)(fast_rand() % 20);
            if (fast_rand() % 4 == 0) {
                spawn_spark(15 + i * 14 + 5, BASE_Y - h_target, COLOR_WHITE);
            }
        }

        // 快速上升，物理平滑重力衰减
        if (h_target > eq_bars[i].height) {
            eq_bars[i].height += (h_target - eq_bars[i].height) * 0.65f;
        } else {
            eq_bars[i].height -= 2.8f;
            if (eq_bars[i].height < 4.0f) eq_bars[i].height = 4.0f;
        }

        // 峰值悬停顶帽
        if (eq_bars[i].height >= eq_bars[i].peak_h) {
            eq_bars[i].peak_h = eq_bars[i].height;
            eq_bars[i].peak_decay = 0.0f;
        } else {
            eq_bars[i].peak_decay += 0.18f;
            eq_bars[i].peak_h -= eq_bars[i].peak_decay;
            if (eq_bars[i].peak_h < eq_bars[i].height) {
                eq_bars[i].peak_h = eq_bars[i].height;
            }
        }
    }

    // 更新爆裂粒子火花
    for (int i = 0; i < NUM_BEAT_SPARKS; i++) {
        if (sparks[i].life > 0) {
            sparks[i].x += sparks[i].vx;
            sparks[i].y += sparks[i].vy;
            sparks[i].vy += 0.08f; // 重力加速度
            sparks[i].life--;
        }
    }
}

static void spectrum_render(uint16_t *fb) {
    memset(fb, 0, DISP_W * DISP_H * sizeof(uint16_t));

    const int bar_w = 11;
    const int gap = 3;
    const int start_x = 10;

    // 1. 绘制基准地平线
    for (int x = 4; x < DISP_W - 4; x++) {
        fb[BASE_Y * DISP_W + x] = rgb565(60, 80, 140);
    }

    // 2. 绘制 16 根实体频谱柱 (使用查找表高速填充)
    for (int i = 0; i < NUM_BARS; i++) {
        int bx = start_x + i * (bar_w + gap);
        int h = (int)eq_bars[i].height;
        if (h > MAX_BAR_H) h = MAX_BAR_H;

        for (int dy = 0; dy < h; dy++) {
            int py = BASE_Y - 1 - dy;
            if (py < 0) continue;
            uint16_t col = spec_lut[dy];

            // 每隔 3 像素留 1 像素黑色网格缝隙，增强 LED 屏条块质感
            if (dy % 4 == 0) col = COLOR_BLACK;

            for (int dx = 0; dx < bar_w; dx++) {
                fb[py * DISP_W + (bx + dx)] = col;
            }
        }

        // 绘制悬浮峰值顶帽 (白/亮青)
        int peak_y = BASE_Y - 1 - (int)eq_bars[i].peak_h;
        if (peak_y >= 0 && peak_y < DISP_H) {
            for (int dx = 0; dx < bar_w; dx++) {
                fb[peak_y * DISP_W + (bx + dx)] = COLOR_WHITE;
                if (peak_y - 1 >= 0) fb[(peak_y - 1) * DISP_W + (bx + dx)] = rgb565(120, 240, 255);
            }
        }

        // 3. 绘制地面镜面水波倒影 (带深度透视渐暗衰减，纯定点整型)
        int refl_h = h / 2;
        if (refl_h > 0) {
            for (int dy = 0; dy < refl_h; dy++) {
                int ry = BASE_Y + 1 + dy * 2;
                if (ry >= DISP_H) break;

                int lut_idx = dy * 2;
                if (lut_idx > MAX_BAR_H) lut_idx = MAX_BAR_H;
                uint16_t c = spec_lut[lut_idx];

                // 快速定点数透视深度衰减
                int fade_factor = ((refl_h - dy) * 115) / refl_h;
                uint8_t r = (uint8_t)((((c >> 11) & 0x1F) * 8 * fade_factor) >> 8);
                uint8_t g = (uint8_t)((((c >> 5) & 0x3F) * 4 * fade_factor) >> 8);
                uint8_t b = (uint8_t)(((c & 0x1F) * 8 * fade_factor) >> 8);
                uint16_t refl_col = rgb565(r, g, b);

                for (int dx = 0; dx < bar_w; dx++) {
                    fb[ry * DISP_W + (bx + dx)] = refl_col;
                }
            }
        }
    }

    // 4. 绘制飞射跳动的火花粒子
    for (int i = 0; i < NUM_BEAT_SPARKS; i++) {
        if (sparks[i].life > 0) {
            int sx = (int)sparks[i].x;
            int sy = (int)sparks[i].y;
            if (sx >= 0 && sx < DISP_W && sy >= 0 && sy < DISP_H) {
                fb[sy * DISP_W + sx] = sparks[i].color;
            }
        }
    }
}

Effect fx_spectrum = {
    .id = "spectrum",
    .title = "» CYBER EQUALIZER «",
    .subtitle = "赛博霓虹音频频谱瀑布与镜面倒影",
    .init = spectrum_init,
    .update = spectrum_update,
    .render = spectrum_render,
    .cleanup = spectrum_cleanup
};
