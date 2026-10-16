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
} WebControlBridge;

WebControlBridge* wifi_app_get_bridge(void);

#endif // WIFI_APP_H
