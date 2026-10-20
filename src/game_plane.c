#include "game_plane.h"
#include "display.h"
#include "fast_math.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define PIN_BUZZER GPIO_NUM_6

#ifndef COLOR_GOLD
#define COLOR_GOLD rgb565(255, 215, 0)
#endif

#define MAX_STARS 32
#define MAX_P_BULLETS 20
#define MAX_E_BULLETS 12
#define MAX_ENEMIES 8
#define MAX_ITEMS 4
#define MAX_PARTICLES 36

typedef struct {
    float x, y, speed;
    uint16_t color;
} Star;

typedef struct {
    float x, y, vx, vy;
    bool active;
    uint16_t color;
} Bullet;

typedef enum {
    ENEMY_DRONE = 0,       // 快速轻型侦察机 (绿色，正弦蛇形机动)
    ENEMY_INTERCEPTOR,    // 中型截击机 (红色，直线俯冲+发射子弹)
    ENEMY_BOSS            // 重装旗舰 (金色大机体，横向巡航+散射激光)
} EnemyType;

typedef struct {
    float x, y, vx, vy;
    int hp, max_hp;
    EnemyType type;
    bool active;
    float anim_timer;
    float shoot_cd;
} Enemy;

typedef enum {
    ITEM_WEAPON = 0, // 'P' 武器火力升级
    ITEM_SHIELD,     // 'S' 护盾
    ITEM_LIFE        // 'H' 增加生命
} ItemType;

typedef struct {
    float x, y;
    ItemType type;
    bool active;
    float anim_timer;
} Item;

typedef struct {
    float x, y, vx, vy;
    float life, max_life;
    uint16_t color;
    bool active;
} Particle;

// 游戏全局上下文
static struct {
    // 玩家战机
    float px, py;
    float p_vx, p_vy;
    int lives;
    int score;
    int high_score;
    int weapon_level;
    bool shield;
    float invuln_timer;
    float fire_cd;
    bool is_gameover;

    // 输入与托管
    int input_x, input_y;
    bool input_fire;
    float last_human_input_time;
    bool is_autopilot;

    // 对象池
    Star stars[MAX_STARS];
    Bullet p_bullets[MAX_P_BULLETS];
    Bullet e_bullets[MAX_E_BULLETS];
    Enemy enemies[MAX_ENEMIES];
    Item items[MAX_ITEMS];
    Particle particles[MAX_PARTICLES];

    // 出怪与关卡波次
    float spawn_timer;
    int enemies_killed;
    bool boss_active;
    int boss_idx;

    // 非阻塞蜂鸣器声音
    int snd_frames_left;
    int snd_tone_mode; // 0: off, 1: pew, 2: boom, 3: item
} G;

// 声音触发 (纯帧计数，零阻塞)
static void trigger_sound(int mode, int frames) {
    G.snd_tone_mode = mode;
    G.snd_frames_left = frames;
    gpio_set_direction(PIN_BUZZER, GPIO_MODE_OUTPUT);
    gpio_set_level(PIN_BUZZER, 0); // 低电平发声
}

static void update_sound(void) {
    if (G.snd_frames_left > 0) {
        G.snd_frames_left--;
        if (G.snd_frames_left == 0) {
            // 恢复高阻输入态，彻底静音
            gpio_set_direction(PIN_BUZZER, GPIO_MODE_INPUT);
            G.snd_tone_mode = 0;
        }
    }
}

// 产生爆炸粒子
static void spawn_explosion(float x, float y, int count, uint16_t base_color) {
    for (int i = 0; i < count; i++) {
        for (int p = 0; p < MAX_PARTICLES; p++) {
            if (!G.particles[p].active) {
                G.particles[p].active = true;
                G.particles[p].x = x;
                G.particles[p].y = y;
                float angle = (float)(rand() % 360) * 3.14159f / 180.0f;
                float spd = (float)(20 + rand() % 80);
                G.particles[p].vx = cosf(angle) * spd;
                G.particles[p].vy = sinf(angle) * spd;
                G.particles[p].max_life = 0.2f + (float)(rand() % 25) / 100.0f;
                G.particles[p].life = G.particles[p].max_life;
                G.particles[p].color = (rand() % 2 == 0) ? base_color : COLOR_YELLOW;
                break;
            }
        }
    }
}

// 掉落道具
static void maybe_spawn_item(float x, float y) {
    if ((rand() % 100) < 22) { // 22% 概率掉落道具
        for (int i = 0; i < MAX_ITEMS; i++) {
            if (!G.items[i].active) {
                G.items[i].active = true;
                G.items[i].x = x;
                G.items[i].y = y;
                int r = rand() % 10;
                if (r < 5) G.items[i].type = ITEM_WEAPON;
                else if (r < 8) G.items[i].type = ITEM_SHIELD;
                else G.items[i].type = ITEM_LIFE;
                G.items[i].anim_timer = 0;
                break;
            }
        }
    }
}

// 生成敌机
static void spawn_enemy(void) {
    // 寻找空闲槽位
    int slot = -1;
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (!G.enemies[i].active) {
            slot = i;
            break;
        }
    }
    if (slot < 0) return;

    // 检查是否该出 Boss (每击杀 15 架敌机且当前无 Boss)
    if (!G.boss_active && G.enemies_killed > 0 && (G.enemies_killed % 15 == 0)) {
        G.enemies[slot].active = true;
        G.enemies[slot].type = ENEMY_BOSS;
        G.enemies[slot].x = 120.0f;
        G.enemies[slot].y = 45.0f;
        G.enemies[slot].vx = 50.0f;
        G.enemies[slot].vy = 0;
        G.enemies[slot].hp = 18;
        G.enemies[slot].max_hp = 18;
        G.enemies[slot].shoot_cd = 1.0f;
        G.boss_active = true;
        G.boss_idx = slot;
        trigger_sound(3, 4); // 警报声
        return;
    }

    // 普通随机生成
    EnemyType type = (rand() % 3 == 0) ? ENEMY_INTERCEPTOR : ENEMY_DRONE;
    G.enemies[slot].active = true;
    G.enemies[slot].type = type;
    G.enemies[slot].x = (float)(20 + rand() % 200);
    G.enemies[slot].y = -10.0f;
    G.enemies[slot].anim_timer = (float)(rand() % 100);

    if (type == ENEMY_DRONE) {
        G.enemies[slot].vx = (float)((rand() % 60) - 30);
        G.enemies[slot].vy = (float)(60 + rand() % 35);
        G.enemies[slot].hp = 1;
        G.enemies[slot].max_hp = 1;
        G.enemies[slot].shoot_cd = 999.0f; // 侦察机不发弹
    } else {
        G.enemies[slot].vx = (float)((rand() % 30) - 15);
        G.enemies[slot].vy = (float)(45 + rand() % 30);
        G.enemies[slot].hp = 2;
        G.enemies[slot].max_hp = 2;
        G.enemies[slot].shoot_cd = 1.2f + (float)(rand() % 10) / 10.0f;
    }
}

// 发射玩家子弹
static void player_fire(void) {
    if (G.fire_cd > 0) return;
    G.fire_cd = 0.14f;

    if (G.weapon_level == 1) {
        // 单发主激光
        for (int i = 0; i < MAX_P_BULLETS; i++) {
            if (!G.p_bullets[i].active) {
                G.p_bullets[i].active = true;
                G.p_bullets[i].x = G.px;
                G.p_bullets[i].y = G.py - 10.0f;
                G.p_bullets[i].vx = 0;
                G.p_bullets[i].vy = -280.0f;
                G.p_bullets[i].color = COLOR_CYAN;
                break;
            }
        }
    } else if (G.weapon_level == 2) {
        // 双翼平行激光
        for (int side = -1; side <= 1; side += 2) {
            for (int i = 0; i < MAX_P_BULLETS; i++) {
                if (!G.p_bullets[i].active) {
                    G.p_bullets[i].active = true;
                    G.p_bullets[i].x = G.px + (float)side * 6.0f;
                    G.p_bullets[i].y = G.py - 8.0f;
                    G.p_bullets[i].vx = 0;
                    G.p_bullets[i].vy = -280.0f;
                    G.p_bullets[i].color = COLOR_CYAN;
                    break;
                }
            }
        }
    } else {
        // 三路散射等离子炮
        float angles[3] = {-35.0f, 0.0f, 35.0f};
        for (int k = 0; k < 3; k++) {
            for (int i = 0; i < MAX_P_BULLETS; i++) {
                if (!G.p_bullets[i].active) {
                    G.p_bullets[i].active = true;
                    G.p_bullets[i].x = G.px + (k - 1) * 5.0f;
                    G.p_bullets[i].y = G.py - 8.0f;
                    G.p_bullets[i].vx = angles[k];
                    G.p_bullets[i].vy = -280.0f;
                    G.p_bullets[i].color = (k == 1) ? COLOR_WHITE : COLOR_YELLOW;
                    break;
                }
            }
        }
    }
    trigger_sound(1, 1); // 清脆射击音 Pew
}

// 发射敌机子弹
static void enemy_fire(float x, float y, float vx, float vy) {
    for (int i = 0; i < MAX_E_BULLETS; i++) {
        if (!G.e_bullets[i].active) {
            G.e_bullets[i].active = true;
            G.e_bullets[i].x = x;
            G.e_bullets[i].y = y;
            G.e_bullets[i].vx = vx;
            G.e_bullets[i].vy = vy;
            G.e_bullets[i].color = COLOR_ORANGE;
            break;
        }
    }
}

// 玩家被击中
static void player_hit(void) {
    if (G.invuln_timer > 0) return;

    if (G.shield) {
        G.shield = false; // 护盾抵消伤害
        G.invuln_timer = 1.0f;
        spawn_explosion(G.px, G.py, 12, COLOR_CYAN);
        trigger_sound(2, 2);
        return;
    }

    G.lives--;
    spawn_explosion(G.px, G.py, 24, COLOR_RED);
    trigger_sound(2, 4);

    if (G.lives <= 0) {
        G.is_gameover = true;
        trigger_sound(2, 6);
    } else {
        G.invuln_timer = 2.2f;
        G.weapon_level = 1; // 降回 1 级火力
    }
}

void game_plane_init(void) {
    memset(&G, 0, sizeof(G));
    G.px = 120.0f;
    G.py = 200.0f;
    G.lives = 3;
    G.score = 0;
    G.high_score = 3600;
    G.weapon_level = 1;
    G.shield = false;
    G.invuln_timer = 0;
    G.fire_cd = 0;
    G.is_gameover = false;
    G.is_autopilot = true; // 默认演示模式
    G.last_human_input_time = 0;
    G.spawn_timer = 0;

    // 初始化星空
    for (int i = 0; i < MAX_STARS; i++) {
        G.stars[i].x = (float)(rand() % DISP_W);
        G.stars[i].y = (float)(rand() % DISP_H);
        G.stars[i].speed = (float)(30 + rand() % 90);
        int b = 100 + rand() % 155;
        G.stars[i].color = rgb565(b / 3, b / 2, b);
    }
}

void game_plane_restart(void) {
    G.px = 120.0f;
    G.py = 200.0f;
    G.p_vx = 0;
    G.p_vy = 0;
    G.lives = 3;
    G.score = 0;
    G.weapon_level = 1;
    G.shield = false;
    G.invuln_timer = 1.5f;
    G.is_gameover = false;
    G.boss_active = false;
    G.enemies_killed = 0;

    for (int i = 0; i < MAX_ENEMIES; i++) G.enemies[i].active = false;
    for (int i = 0; i < MAX_P_BULLETS; i++) G.p_bullets[i].active = false;
    for (int i = 0; i < MAX_E_BULLETS; i++) G.e_bullets[i].active = false;
    for (int i = 0; i < MAX_ITEMS; i++) G.items[i].active = false;
}

void game_plane_set_input(int dir_x, int dir_y, bool fire) {
    G.input_x = dir_x;
    G.input_y = dir_y;
    G.input_fire = fire;
    G.last_human_input_time = (float)esp_timer_get_time() / 1000000.0f;
    G.is_autopilot = false; // 人类接管控制！
}

void game_plane_get_status(int *score, int *high_score, int *lives, int *level, bool *is_gameover) {
    if (score) *score = G.score;
    if (high_score) *high_score = G.high_score;
    if (lives) *lives = G.lives;
    if (level) *level = G.weapon_level;
    if (is_gameover) *is_gameover = G.is_gameover;
}

void game_plane_update(float dt) {
    update_sound();

    if (dt > 0.05f) dt = 0.05f; // 防止掉帧步长过大导致穿透

    float now_s = (float)esp_timer_get_time() / 1000000.0f;
    if (now_s - G.last_human_input_time > 6.0f) {
        G.is_autopilot = true; // 超过 6 秒无人操作切入自动巡航演示
    }

    // 1. 更新星空背景
    for (int i = 0; i < MAX_STARS; i++) {
        G.stars[i].y += G.stars[i].speed * dt;
        if (G.stars[i].y >= DISP_H) {
            G.stars[i].y = 0;
            G.stars[i].x = (float)(rand() % DISP_W);
        }
    }

    if (G.is_gameover) {
        // 游戏结束状态仅更新粒子与星空
        for (int p = 0; p < MAX_PARTICLES; p++) {
            if (G.particles[p].active) {
                G.particles[p].x += G.particles[p].vx * dt;
                G.particles[p].y += G.particles[p].vy * dt;
                G.particles[p].life -= dt;
                if (G.particles[p].life <= 0) G.particles[p].active = false;
            }
        }
        return;
    }

    // 2. 玩家控制与自动驾驶
    if (G.invuln_timer > 0) G.invuln_timer -= dt;
    if (G.fire_cd > 0) G.fire_cd -= dt;

    if (G.is_autopilot) {
        // AI 自动驾驶：寻找最近敌机跟随并自动开火
        float target_x = 120.0f;
        float min_dist = 999.0f;
        for (int i = 0; i < MAX_ENEMIES; i++) {
            if (G.enemies[i].active) {
                float dist = fabsf(G.enemies[i].x - G.px);
                if (dist < min_dist) {
                    min_dist = dist;
                    target_x = G.enemies[i].x;
                }
            }
        }
        // 平滑追踪
        if (G.px < target_x - 4.0f) G.p_vx = 90.0f;
        else if (G.px > target_x + 4.0f) G.p_vx = -90.0f;
        else G.p_vx = 0;
        G.p_vy = 0;
        player_fire();
    } else {
        // 人类玩家输入
        G.p_vx = (float)G.input_x * 125.0f;
        G.p_vy = (float)G.input_y * 100.0f;
        if (G.input_fire) player_fire();
    }

    G.px += G.p_vx * dt;
    G.py += G.p_vy * dt;
    // 屏幕边缘约束
    if (G.px < 14.0f) G.px = 14.0f;
    if (G.px > DISP_W - 14.0f) G.px = DISP_W - 14.0f;
    if (G.py < 40.0f) G.py = 40.0f;
    if (G.py > DISP_H - 18.0f) G.py = DISP_H - 18.0f;

    // 3. 更新玩家子弹
    for (int i = 0; i < MAX_P_BULLETS; i++) {
        if (G.p_bullets[i].active) {
            G.p_bullets[i].x += G.p_bullets[i].vx * dt;
            G.p_bullets[i].y += G.p_bullets[i].vy * dt;
            if (G.p_bullets[i].y < 5.0f || G.p_bullets[i].x < 2 || G.p_bullets[i].x >= DISP_W - 2) {
                G.p_bullets[i].active = false;
            }
        }
    }

    // 4. 更新敌机生成
    G.spawn_timer += dt;
    float spawn_interval = G.boss_active ? 2.5f : 1.1f;
    if (G.spawn_timer >= spawn_interval) {
        G.spawn_timer = 0;
        spawn_enemy();
    }

    // 5. 更新敌机逻辑与碰撞
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (!G.enemies[i].active) continue;

        Enemy *e = &G.enemies[i];
        e->anim_timer += dt;

        if (e->type == ENEMY_DRONE) {
            e->x += e->vx * dt;
            e->y += e->vy * dt;
            e->vx = sinf(e->anim_timer * 4.0f) * 45.0f;
        } else if (e->type == ENEMY_INTERCEPTOR) {
            e->x += e->vx * dt;
            e->y += e->vy * dt;
            e->shoot_cd -= dt;
            if (e->shoot_cd <= 0) {
                e->shoot_cd = 1.4f;
                // 朝玩家方向发射敌弹
                float dx = G.px - e->x;
                float dy = G.py - e->y;
                float len = sqrtf(dx * dx + dy * dy);
                if (len > 0) {
                    enemy_fire(e->x, e->y + 6.0f, (dx / len) * 85.0f, (dy / len) * 85.0f);
                }
            }
        } else if (e->type == ENEMY_BOSS) {
            e->x += e->vx * dt;
            if (e->x <= 35.0f) { e->x = 35.0f; e->vx = 45.0f; }
            if (e->x >= DISP_W - 35.0f) { e->x = DISP_W - 35.0f; e->vx = -45.0f; }
            e->shoot_cd -= dt;
            if (e->shoot_cd <= 0) {
                e->shoot_cd = 1.2f;
                // 3 路散射敌弹
                enemy_fire(e->x - 12.0f, e->y + 10.0f, -30.0f, 95.0f);
                enemy_fire(e->x, e->y + 12.0f, 0, 105.0f);
                enemy_fire(e->x + 12.0f, e->y + 10.0f, 30.0f, 95.0f);
            }
        }

        // 超出屏幕下界
        if (e->y > DISP_H + 15.0f) {
            e->active = false;
            if (e->type == ENEMY_BOSS) G.boss_active = false;
            continue;
        }

        // 与玩家战机碰撞
        float dist_pe = hypotf(e->x - G.px, e->y - G.py);
        float hit_rad = (e->type == ENEMY_BOSS) ? 22.0f : 12.0f;
        if (dist_pe < hit_rad) {
            player_hit();
            if (e->type != ENEMY_BOSS) {
                e->active = false;
                spawn_explosion(e->x, e->y, 14, COLOR_ORANGE);
            }
        }

        // 与玩家子弹碰撞检测
        float b_hit_rad = (e->type == ENEMY_BOSS) ? 18.0f : 9.0f;
        for (int b = 0; b < MAX_P_BULLETS; b++) {
            if (!G.p_bullets[b].active) continue;
            if (hypotf(e->x - G.p_bullets[b].x, e->y - G.p_bullets[b].y) < b_hit_rad) {
                G.p_bullets[b].active = false;
                e->hp--;
                spawn_explosion(G.p_bullets[b].x, G.p_bullets[b].y, 4, COLOR_WHITE);

                if (e->hp <= 0) {
                    e->active = false;
                    G.enemies_killed++;
                    int pts = (e->type == ENEMY_BOSS) ? 1000 : (e->type == ENEMY_INTERCEPTOR ? 200 : 100);
                    G.score += pts;
                    if (G.score > G.high_score) G.high_score = G.score;

                    spawn_explosion(e->x, e->y, (e->type == ENEMY_BOSS) ? 32 : 16,
                                    (e->type == ENEMY_BOSS) ? COLOR_GOLD : COLOR_RED);
                    trigger_sound(2, (e->type == ENEMY_BOSS) ? 4 : 2); // 击破爆炸声
                    maybe_spawn_item(e->x, e->y);

                    if (e->type == ENEMY_BOSS) G.boss_active = false;
                    break;
                }
            }
        }
    }

    // 6. 更新敌机子弹
    for (int i = 0; i < MAX_E_BULLETS; i++) {
        if (!G.e_bullets[i].active) continue;
        G.e_bullets[i].x += G.e_bullets[i].vx * dt;
        G.e_bullets[i].y += G.e_bullets[i].vy * dt;

        if (G.e_bullets[i].y > DISP_H + 5 || G.e_bullets[i].x < 0 || G.e_bullets[i].x >= DISP_W) {
            G.e_bullets[i].active = false;
            continue;
        }

        // 击中玩家判定
        if (hypotf(G.e_bullets[i].x - G.px, G.e_bullets[i].y - G.py) < 8.0f) {
            G.e_bullets[i].active = false;
            player_hit();
        }
    }

    // 7. 更新道具掉落与拾取
    for (int i = 0; i < MAX_ITEMS; i++) {
        if (!G.items[i].active) continue;
        G.items[i].y += 38.0f * dt;
        G.items[i].anim_timer += dt;

        if (G.items[i].y > DISP_H + 10) {
            G.items[i].active = false;
            continue;
        }

        // 玩家拾取道具判定
        if (hypotf(G.items[i].x - G.px, G.items[i].y - G.py) < 14.0f) {
            G.items[i].active = false;
            trigger_sound(3, 2); // 拾取清脆声

            if (G.items[i].type == ITEM_WEAPON) {
                if (G.weapon_level < 3) G.weapon_level++;
                G.score += 300;
            } else if (G.items[i].type == ITEM_SHIELD) {
                G.shield = true;
                G.score += 200;
            } else if (G.items[i].type == ITEM_LIFE) {
                if (G.lives < 4) G.lives++;
                G.score += 500;
            }
        }
    }

    // 8. 更新粒子
    for (int p = 0; p < MAX_PARTICLES; p++) {
        if (G.particles[p].active) {
            G.particles[p].x += G.particles[p].vx * dt;
            G.particles[p].y += G.particles[p].vy * dt;
            G.particles[p].life -= dt;
            if (G.particles[p].life <= 0) G.particles[p].active = false;
        }
    }
}

// 绘制战机精灵图 (矢量化高精绘制)
static void draw_player_ship(uint16_t *fb, int x, int y, bool shield, bool invuln) {
    if (invuln && ((esp_timer_get_time() / 100000LL) % 2 == 0)) {
        return; // 无敌闪烁
    }

    // 引擎尾焰粒子 (动态闪烁)
    int flame_len = 4 + (rand() % 5);
    for (int fl = 0; fl < flame_len; fl++) {
        display_draw_pixel(fb, x, y + 8 + fl, (fl < 3) ? COLOR_WHITE : COLOR_ORANGE);
        display_draw_pixel(fb, x - 1, y + 8 + fl, COLOR_RED);
        display_draw_pixel(fb, x + 1, y + 8 + fl, COLOR_RED);
    }

    // 机身核心 (白色+银色)
    display_fill_rect(fb, x - 2, y - 6, 5, 12, COLOR_WHITE);
    display_draw_pixel(fb, x, y - 7, COLOR_CYAN); // 尖端雷达
    display_draw_pixel(fb, x, y - 8, COLOR_WHITE);

    // 驾驶舱 (赛博荧光青)
    display_fill_rect(fb, x - 1, y - 3, 3, 4, COLOR_CYAN);

    // 双翼 (三角折角翼)
    display_draw_line(fb, x - 3, y - 1, x - 9, y + 5, COLOR_BLUE);
    display_draw_line(fb, x + 3, y - 1, x + 9, y + 5, COLOR_BLUE);
    display_draw_line(fb, x - 9, y + 5, x - 3, y + 6, COLOR_CYAN);
    display_draw_line(fb, x + 9, y + 5, x + 3, y + 6, COLOR_CYAN);

    // 翼尖航标灯 (红绿)
    display_draw_pixel(fb, x - 9, y + 4, COLOR_RED);
    display_draw_pixel(fb, x + 9, y + 4, COLOR_GREEN);

    // 护盾光环
    if (shield) {
        int r = 14;
        for (int deg = 0; deg < 360; deg += 30) {
            float rad = (float)deg * 3.14159f / 180.0f;
            int sx = x + (int)(cosf(rad) * r);
            int sy = y + (int)(sinf(rad) * r);
            display_draw_pixel(fb, sx, sy, COLOR_CYAN);
        }
    }
}

// 绘制敌机精灵
static void draw_enemy(uint16_t *fb, Enemy *e) {
    int x = (int)e->x;
    int y = (int)e->y;

    if (e->type == ENEMY_DRONE) {
        // 绿色轻型飞碟
        display_fill_rect(fb, x - 4, y - 1, 9, 3, COLOR_GREEN);
        display_draw_pixel(fb, x, y - 3, COLOR_CYAN);
        display_draw_pixel(fb, x - 2, y - 2, COLOR_WHITE);
        display_draw_pixel(fb, x + 2, y - 2, COLOR_WHITE);
        display_draw_pixel(fb, x - 3, y + 2, COLOR_YELLOW);
        display_draw_pixel(fb, x + 3, y + 2, COLOR_YELLOW);
    } else if (e->type == ENEMY_INTERCEPTOR) {
        // 红色重型截击机
        display_fill_rect(fb, x - 2, y - 4, 5, 8, COLOR_RED);
        display_draw_line(fb, x - 2, y - 3, x - 7, y + 1, COLOR_ORANGE);
        display_draw_line(fb, x + 2, y - 3, x + 7, y + 1, COLOR_ORANGE);
        display_draw_pixel(fb, x, y + 4, COLOR_YELLOW); // 机炮
    } else if (e->type == ENEMY_BOSS) {
        // 重装巨舰
        display_fill_rect(fb, x - 16, y - 8, 33, 14, rgb565(180, 140, 20));
        display_fill_rect(fb, x - 12, y - 5, 25, 10, rgb565(240, 190, 40));
        display_fill_rect(fb, x - 4, y - 2, 9, 5, COLOR_RED); // 核心反应堆

        // 巨舰两翼重炮
        display_draw_line(fb, x - 14, y + 6, x - 14, y + 10, COLOR_WHITE);
        display_draw_line(fb, x + 14, y + 6, x + 14, y + 10, COLOR_WHITE);
    }
}

void game_plane_render(uint16_t *fb) {
    // 1. 深空背景清屏
    display_fill_rect(fb, 0, 0, DISP_W, DISP_H, rgb565(2, 4, 12));

    // 2. 渲染星空
    for (int i = 0; i < MAX_STARS; i++) {
        display_draw_pixel(fb, (int)G.stars[i].x, (int)G.stars[i].y, G.stars[i].color);
    }

    // 3. 渲染粒子
    for (int p = 0; p < MAX_PARTICLES; p++) {
        if (G.particles[p].active) {
            display_draw_pixel(fb, (int)G.particles[p].x, (int)G.particles[p].y, G.particles[p].color);
        }
    }

    // 4. 渲染道具
    for (int i = 0; i < MAX_ITEMS; i++) {
        if (!G.items[i].active) continue;
        int ix = (int)G.items[i].x;
        int iy = (int)G.items[i].y;
        uint16_t col = (G.items[i].type == ITEM_WEAPON) ? COLOR_GOLD :
                       ((G.items[i].type == ITEM_SHIELD) ? COLOR_CYAN : COLOR_GREEN);
        char ch = (G.items[i].type == ITEM_WEAPON) ? 'P' :
                  ((G.items[i].type == ITEM_SHIELD) ? 'S' : 'H');

        display_draw_box(fb, ix - 5, iy - 5, 11, 11, col);
        display_draw_char(fb, ix - 3, iy - 3, ch, col, COLOR_BLACK, true);
    }

    // 5. 渲染敌机
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (G.enemies[i].active) {
            draw_enemy(fb, &G.enemies[i]);
        }
    }

    // 6. 渲染敌机子弹
    for (int i = 0; i < MAX_E_BULLETS; i++) {
        if (G.e_bullets[i].active) {
            int bx = (int)G.e_bullets[i].x;
            int by = (int)G.e_bullets[i].y;
            display_draw_pixel(fb, bx, by, COLOR_WHITE);
            display_draw_pixel(fb, bx - 1, by, COLOR_RED);
            display_draw_pixel(fb, bx + 1, by, COLOR_RED);
            display_draw_pixel(fb, bx, by - 1, COLOR_ORANGE);
            display_draw_pixel(fb, bx, by + 1, COLOR_ORANGE);
        }
    }

    // 7. 渲染玩家子弹 (高光激光束)
    for (int i = 0; i < MAX_P_BULLETS; i++) {
        if (G.p_bullets[i].active) {
            int bx = (int)G.p_bullets[i].x;
            int by = (int)G.p_bullets[i].y;
            display_draw_line(fb, bx, by - 4, bx, by + 2, G.p_bullets[i].color);
            display_draw_pixel(fb, bx, by - 4, COLOR_WHITE);
        }
    }

    // 8. 渲染玩家战机
    if (!G.is_gameover) {
        draw_player_ship(fb, (int)G.px, (int)G.py, G.shield, (G.invuln_timer > 0));
    }

    // 9. 顶部 HUD 看板
    // 生命值图标
    for (int h = 0; h < 4; h++) {
        uint16_t c = (h < G.lives) ? COLOR_RED : rgb565(60, 60, 60);
        display_fill_rect(fb, 8 + h * 9, 8, 6, 6, c);
    }

    // 得分
    char score_str[24];
    snprintf(score_str, sizeof(score_str), "SC:%05d", G.score);
    display_draw_string(fb, 52, 7, score_str, COLOR_WHITE, COLOR_BLACK, true);

    // 火力等级
    char lvl_str[8];
    snprintf(lvl_str, sizeof(lvl_str), "LV%d", G.weapon_level);
    display_draw_string(fb, 126, 7, lvl_str, COLOR_YELLOW, COLOR_BLACK, true);

    // 自动驾驶状态微标
    if (G.is_autopilot && !G.is_gameover) {
        display_draw_string(fb, DISP_W - 74, 7, "[AUTO]", COLOR_CYAN, COLOR_BLACK, true);
    } else if (!G.is_gameover) {
        display_draw_string(fb, DISP_W - 74, 7, "[HUMAN]", COLOR_GREEN, COLOR_BLACK, true);
    }

    // Boss 血条
    if (G.boss_active && G.enemies[G.boss_idx].active) {
        Enemy *boss = &G.enemies[G.boss_idx];
        display_draw_box(fb, 40, 20, 160, 5, COLOR_GOLD);
        int bar_w = (boss->hp * 158) / boss->max_hp;
        if (bar_w > 0) {
            display_fill_rect(fb, 41, 21, bar_w, 3, COLOR_RED);
        }
    }

    // 10. Game Over 界面
    if (G.is_gameover) {
        display_fill_rect(fb, 24, 70, DISP_W - 48, 100, rgb565(12, 16, 28));
        display_draw_box(fb, 24, 70, DISP_W - 48, 100, COLOR_RED);
        display_draw_box(fb, 26, 72, DISP_W - 52, 96, COLOR_YELLOW);

        display_draw_string(fb, 64, 84, "GAME OVER", COLOR_RED, COLOR_BLACK, true);

        char final_sc[32];
        snprintf(final_sc, sizeof(final_sc), "SCORE: %05d", G.score);
        display_draw_string(fb, 60, 104, final_sc, COLOR_WHITE, COLOR_BLACK, true);

        char hi_sc[32];
        snprintf(hi_sc, sizeof(hi_sc), "BEST : %05d", G.high_score);
        display_draw_string(fb, 60, 120, hi_sc, COLOR_GOLD, COLOR_BLACK, true);

        display_draw_string(fb, 36, 142, "PRESS FIRE TO RESTART", COLOR_CYAN, COLOR_BLACK, true);
    }
}

void game_plane_cleanup(void) {
    gpio_set_direction(PIN_BUZZER, GPIO_MODE_INPUT);
}

// 封装为 Effect 对象
Effect fx_game_plane = {
    .id = "game_plane",
    .title = "18. 街机太空飞机大战",
    .subtitle = "手机无线手柄 / 自动巡航演示",
    .init = game_plane_init,
    .update = game_plane_update,
    .render = game_plane_render,
    .cleanup = game_plane_cleanup
};
