#ifndef GAME_PLANE_H
#define GAME_PLANE_H

#include "effects.h"
#include <stdint.h>
#include <stdbool.h>

// 导出飞机大战作为第 18 款特殊交互式动态特效
extern Effect fx_game_plane;

// 游戏全局控制接口
void game_plane_set_input(int dir_x, int dir_y, bool fire);
void game_plane_restart(void);
void game_plane_get_status(int *score, int *high_score, int *lives, int *level, bool *is_gameover);

#endif // GAME_PLANE_H
