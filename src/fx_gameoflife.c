#include "effects.h"
#include "fast_math.h"
#include <string.h>
#include <stdlib.h>

#define LIFE_W 60
#define LIFE_H 60

static uint8_t grid_cur[LIFE_H][LIFE_W];
static uint8_t grid_next[LIFE_H][LIFE_W];

static int stagnation_count = 0;
static int last_live_count = 0;

static void inject_glider(int gx, int gy) {
    const int pattern[3][3] = {
        {0, 1, 0},
        {0, 0, 1},
        {1, 1, 1}
    };
    for (int y = 0; y < 3; y++) {
        for (int x = 0; x < 3; x++) {
            int px = (gx + x) % LIFE_W;
            int py = (gy + y) % LIFE_H;
            if (pattern[y][x]) grid_cur[py][px] = 1;
        }
    }
}

static void inject_pulsar(int cx, int cy) {
    const int offsets[] = { -6, -5, -4, -2, -1, 1, 2, 4, 5, 6 };
    for (int i = 0; i < 10; i++) {
        int d = offsets[i];
        if (abs(d) != 2 && abs(d) != 4) {
            int px = (cx + d + LIFE_W) % LIFE_W;
            int py1 = (cy - 3 + LIFE_H) % LIFE_H;
            int py2 = (cy + 3 + LIFE_H) % LIFE_H;
            grid_cur[py1][px] = 1;
            grid_cur[py2][px] = 1;

            int py = (cy + d + LIFE_H) % LIFE_H;
            int px1 = (cx - 3 + LIFE_W) % LIFE_W;
            int px2 = (cx + 3 + LIFE_W) % LIFE_W;
            grid_cur[py][px1] = 1;
            grid_cur[py][px2] = 1;
        }
    }
}

static void seed_random_universe(void) {
    memset(grid_cur, 0, sizeof(grid_cur));
    for (int y = 0; y < LIFE_H; y++) {
        for (int x = 0; x < LIFE_W; x++) {
            if ((fast_rand() % 100) < 18) {
                grid_cur[y][x] = 1;
            }
        }
    }
    inject_glider(4, 4);
    inject_glider(LIFE_W - 12, 6);
    inject_pulsar(LIFE_W / 2, LIFE_H / 2);
    stagnation_count = 0;
}

static void gameoflife_init(void) {
    seed_random_universe();
}

static void gameoflife_cleanup(void) {
}

static void gameoflife_update(float dt) {
    (void)dt;
    int live_count = 0;

    for (int y = 0; y < LIFE_H; y++) {
        int ym = (y == 0) ? (LIFE_H - 1) : (y - 1);
        int yp = (y == LIFE_H - 1) ? 0 : (y + 1);

        for (int x = 0; x < LIFE_W; x++) {
            int xm = (x == 0) ? (LIFE_W - 1) : (x - 1);
            int xp = (x == LIFE_W - 1) ? 0 : (x + 1);

            int neighbors = (grid_cur[ym][xm] > 0) + (grid_cur[ym][x] > 0) + (grid_cur[ym][xp] > 0) +
                            (grid_cur[y][xm]  > 0)                          + (grid_cur[y][xp]  > 0) +
                            (grid_cur[yp][xm] > 0) + (grid_cur[yp][x] > 0) + (grid_cur[yp][xp] > 0);

            uint8_t age = grid_cur[y][x];
            if (age > 0) {
                if (neighbors == 2 || neighbors == 3) {
                    grid_next[y][x] = (age < 250) ? (age + 1) : 250;
                    live_count++;
                } else {
                    grid_next[y][x] = 0;
                }
            } else {
                if (neighbors == 3) {
                    grid_next[y][x] = 1;
                    live_count++;
                } else {
                    grid_next[y][x] = 0;
                }
            }
        }
    }

    memcpy(grid_cur, grid_next, sizeof(grid_cur));

    if (abs(live_count - last_live_count) < 2 || live_count < 18) {
        stagnation_count++;
        if (stagnation_count > 40) {
            inject_glider(fast_rand() % (LIFE_W - 6), fast_rand() % (LIFE_H - 6));
            if (live_count < 15) {
                seed_random_universe();
            }
            stagnation_count = 0;
        }
    } else {
        stagnation_count = 0;
    }
    last_live_count = live_count;
}

static inline uint16_t get_cell_color(uint8_t age) {
    if (age == 1) {
        return rgb565(0, 240, 255); // 新生：耀眼青蓝
    } else if (age < 4) {
        return rgb565(0, 255, 120); // 幼年：荧光翠绿
    } else if (age < 9) {
        return rgb565(255, 230, 20);  // 青年：金黄高能
    } else if (age < 18) {
        return rgb565(255, 90, 20);   // 成熟：炽热橙红
    } else {
        return rgb565(240, 30, 180);  // 长者：赛博粉紫
    }
}

static void gameoflife_render(uint16_t *fb) {
    memset(fb, 0, DISP_W * DISP_H * sizeof(uint16_t));

    // 60x60 网格以 4x4 像素块高速填充到 240x240
    for (int y = 0; y < LIFE_H; y++) {
        int py = y * 4;
        for (int x = 0; x < LIFE_W; x++) {
            uint8_t age = grid_cur[y][x];
            if (age == 0) continue;

            uint16_t col = get_cell_color(age);
            int px = x * 4;
            uint32_t c32 = ((uint32_t)col << 16) | col;

            // 绘制 3x3 实体核，留 1 像素黑色缝隙，增加经典的黑客复古元胞晶格感
            *(uint32_t *)&fb[py * DISP_W + px] = c32;
            fb[py * DISP_W + px + 2] = col;

            *(uint32_t *)&fb[(py + 1) * DISP_W + px] = c32;
            fb[(py + 1) * DISP_W + px + 2] = col;

            *(uint32_t *)&fb[(py + 2) * DISP_W + px] = c32;
            fb[(py + 2) * DISP_W + px + 2] = col;

            if (age > 2) {
                fb[(py + 1) * DISP_W + px + 1] = COLOR_WHITE;
            }
        }
    }
}

Effect fx_gameoflife = {
    .id = "life",
    .title = "» CONWAY'S LIFE «",
    .subtitle = "康威量子元胞自动机生命游戏",
    .init = gameoflife_init,
    .update = gameoflife_update,
    .render = gameoflife_render,
    .cleanup = gameoflife_cleanup
};
