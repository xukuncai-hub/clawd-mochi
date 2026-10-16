#ifndef WIFI_APP_H
#define WIFI_APP_H

#include <stdbool.h>

// 初始化 Wi-Fi 热点 (SoftAP: ClawdMochi | 192.168.4.1) 并启动 Web 服务器
void wifi_app_init(void);

// 外部获取/控制当前特效索引与轮播状态的钩子函数
typedef struct {
    int current_effect_idx;
    int num_effects;
    bool auto_cycle;
    bool request_next;
    bool request_prev;
    int request_set_idx;
    bool request_toggle_cycle;
    bool request_rotate;

    // 街机游戏无线手柄控制桥
    int game_input_x;          // -1: 左移, 0: 停止, 1: 右移
    int game_input_y;          // -1: 上移, 0: 停止, 1: 下移
    bool game_input_fire;      // 是否开火
    bool game_request_restart; // 是否请求重新开始
    bool is_game_active;       // 是否处于游戏手柄激活态
} WebControlBridge;

WebControlBridge* wifi_app_get_bridge(void);

#endif // WIFI_APP_H
