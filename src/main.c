#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_rom_sys.h"

#include "display.h"
#include "effects.h"
#include "fast_math.h"
#include "alarm_timer.h"
#include "wifi_app.h"
#include "game_plane.h"

static const char *TAG = "CLAWD_MOCHI";

#define PIN_BOOT_BUTTON GPIO_NUM_9
#define PIN_BUZZER      GPIO_NUM_6

// 18 款精选高帧率动态特效与街机游戏轮播阵列
static Effect *effects[] = {
    &fx_donut,        // 1. 3D 旋转甜甜圈
    &fx_synthwave,    // 2. 赛博朋克 80s 霓虹落日与透视网格
    &fx_matrix,       // 3. 黑客帝国数字流光雨
    &fx_plasma,       // 4. 迷幻复古彩虹等离子电浆
    &fx_spectrum,     // 5. 赛博霓虹音频频谱瀑布与镜面水波
    &fx_blackhole,    // 6. 引力透镜黑洞与相对论吸积盘
    &fx_galaxy,       // 7. 3D 银河旋涡与星云粒子潮
    &fx_fire,         // 8. 经典毁灭战士热量流体火焰
    &fx_tunnel,       // 9. 时空跃迁虫洞光速飞驰隧道
    &fx_stars,        // 10. 超空间星空与 3D 旋转超立方体
    &fx_dna,          // 11. 3D 炫彩旋转 DNA 双螺旋粒子链
    &fx_metaballs,    // 12. 霓虹流体熔岩双态融球
    &fx_radar,        // 13. 赛博全息战术雷达 HUD
    &fx_oscilloscope, // 14. 赛博激光李萨如谐波示波器
    &fx_gameoflife,   // 15. 康威量子元胞自动机生命游戏
    &fx_aurora,       // 16. 极地深空极光与雪山倒影
    &fx_fractal,      // 17. 电光朱利亚全动态复变分形云
    &fx_game_plane    // 18. 街机太空飞机大战 (支持手机无线手柄)
};

#define NUM_EFFECTS (int)(sizeof(effects) / sizeof(effects[0]))

#define ENABLE_BUZZER 1

static void buzzer_beep(int ms) {
#if ENABLE_BUZZER
    // Clawd Mochi 蜂鸣器为低电平触发模块：输出 LOW 发声，平时必须保持高阻 INPUT 才能静音
    gpio_set_direction(PIN_BUZZER, GPIO_MODE_OUTPUT);
    gpio_set_level(PIN_BUZZER, 0);
    vTaskDelay(pdMS_TO_TICKS(ms));
    gpio_set_direction(PIN_BUZZER, GPIO_MODE_INPUT); // 恢复高阻输入态，彻底静音
#else
    (void)ms;
#endif
}

void app_main(void) {
    ESP_LOGI(TAG, "=========================================");
    ESP_LOGI(TAG, " Clawd Mochi - ESP32-C3 13-FX Ultra WiFi ");
    ESP_LOGI(TAG, "=========================================");

    // 1. 初始化硬件数学加速查表
    fast_math_init();

    // 2. 初始化智能闹钟与定时器引擎
    alarm_timer_init();

    // 3. 初始化按键 (BOOT 键 GPIO 9)
    gpio_config_t btn_conf = {
        .pin_bit_mask = (1ULL << PIN_BOOT_BUTTON),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&btn_conf);

    // 4. 蜂鸣器初始化：严格设为高阻输入模式 (Hi-Z)，避免低电平触发常响
    gpio_config_t bz_conf = {
        .pin_bit_mask = (1ULL << PIN_BUZZER),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&bz_conf);

    // 5. 初始化 ST7789 显示屏 (40MHz SPI DMA)
    if (!display_init()) {
        ESP_LOGE(TAG, "Failed to initialize display!");
        return;
    }

    // 6. 使用静态 4 字节对齐的 240x240 全屏 16-bit DMA 显存 (115.2 KB，编译期确定，永不内存不足)
    static uint16_t g_framebuffer[DISP_W * DISP_H] __attribute__((aligned(4)));
    uint16_t *framebuffer = g_framebuffer;

    // 7. 启动 Wi-Fi 热点 (ClawdMochi) 与移动端 Web 闹钟/定时器控制后台
    wifi_app_init();

    // 启动短提示音
    buzzer_beep(30);

    int current_idx = 0;
    effects[current_idx]->init();

    int64_t last_time = esp_timer_get_time();
    int64_t last_switch_time = last_time;

    // 开机首屏展示 Wi-Fi 热点连接提示 (4 秒)
    int64_t custom_osd_until = last_time + 4000000LL;
    char osd_custom_title[32] = "» WIFI: ClawdMochi-PRO «";
    char osd_custom_sub[32] = "免密直连 192.168.4.1";
    int64_t osd_until = 0;

    bool btn_last_state = true;
    int64_t btn_down_time = 0;
    int64_t last_click_time = 0;
    bool auto_cycle = true;

    WebControlBridge *bridge = wifi_app_get_bridge();
    AlarmTimerContext *alarm_ctx = alarm_timer_get_ctx();

    ESP_LOGI(TAG, "Entering animation render & alarm monitoring loop with %d effects...", NUM_EFFECTS);

    while (1) {
        int64_t now = esp_timer_get_time();
        float dt = (float)(now - last_time) / 1000000.0f;
        last_time = now;

        // 1. 更新闹钟、倒计时与蜂鸣器警报状态
        alarm_timer_update();

        // 2. 同步 Web 桥接状态
        bridge->current_effect_idx = current_idx;
        bridge->auto_cycle = auto_cycle;

        // 处理来自手机网页端的远程切换特效指令
        if (bridge->request_set_idx >= 0 && bridge->request_set_idx < NUM_EFFECTS) {
            if (effects[current_idx]->cleanup) effects[current_idx]->cleanup();
            current_idx = bridge->request_set_idx;
            effects[current_idx]->init();
            osd_until = now + 2500000LL;
            last_switch_time = now;
            bridge->request_set_idx = -1;
            ESP_LOGI(TAG, "Web Remote: Switched to [%d/%d] %s", current_idx + 1, NUM_EFFECTS, effects[current_idx]->title);
        }

        // 处理手机网页端切换自动轮播
        if (bridge->request_toggle_cycle) {
            auto_cycle = !auto_cycle;
            bridge->request_toggle_cycle = false;
            snprintf(osd_custom_title, sizeof(osd_custom_title), "» AUTO CYCLE: %s «", auto_cycle ? "ON" : "OFF");
            snprintf(osd_custom_sub, sizeof(osd_custom_sub), auto_cycle ? "已开启自动轮播" : "已锁定当前特效");
            custom_osd_until = now + 2000000LL;
        }

        // 处理手机网页端旋转屏幕
        if (bridge->request_rotate) {
            uint8_t next_rot = (display_get_rotation() + 1) % 4;
            display_set_rotation(next_rot);
            bridge->request_rotate = false;
            snprintf(osd_custom_title, sizeof(osd_custom_title), "» ROTATE: %d DEG «", next_rot * 90);
            snprintf(osd_custom_sub, sizeof(osd_custom_sub), "屏幕旋转方向: %d度", next_rot * 90);
            custom_osd_until = now + 2000000LL;
        }

        // 处理街机游戏模式输入同步
        if (current_idx == 17) {
            if (bridge->game_request_restart) {
                game_plane_restart();
                bridge->game_request_restart = false;
            }
            game_plane_set_input(bridge->game_input_x, bridge->game_input_y, bridge->game_input_fire);
        }

        // 3. 处理物理 BOOT 按键 (GPIO 9, 低电平触发)
        bool btn_pressed = (gpio_get_level(PIN_BOOT_BUTTON) == 0);
        if (btn_pressed && btn_last_state) {
            btn_down_time = now;
        } else if (!btn_pressed && !btn_last_state) {
            int64_t press_duration = now - btn_down_time;

            // 若当前正在响铃，按任意键立即关闭响铃
            if (alarm_ctx->is_ringing) {
                alarm_timer_dismiss_ring();
                buzzer_beep(30);
                ESP_LOGI(TAG, "Alarm dismissed via physical BOOT button.");
            } else if (current_idx == 17 && press_duration > 40000LL && press_duration < 800000LL) {
                // 处于游戏模式中：短按触发开火或重新开始
                int sc, hsc, liv, lvl; bool go;
                game_plane_get_status(&sc, &hsc, &liv, &lvl, &go);
                if (go) {
                    game_plane_restart();
                } else {
                    game_plane_set_input(0, 0, true);
                }
            } else if (press_duration > 40000LL && press_duration < 800000LL) {
                int64_t time_since_last_click = now - last_click_time;
                if (time_since_last_click < 400000LL) {
                    // 双击：旋转屏幕 90 度
                    uint8_t next_rot = (display_get_rotation() + 1) % 4;
                    display_set_rotation(next_rot);
                    buzzer_beep(30);
                    snprintf(osd_custom_title, sizeof(osd_custom_title), "» ROTATE: %d DEG «", next_rot * 90);
                    snprintf(osd_custom_sub, sizeof(osd_custom_sub), "屏幕旋转方向: %d度", next_rot * 90);
                    custom_osd_until = now + 2000000LL;
                    last_click_time = 0;
                    ESP_LOGI(TAG, "Double-click: Screen rotated to %d deg", next_rot * 90);
                } else {
                    last_click_time = now;
                    // 单击：立即切换下一个特效 (静音切换)
                    if (effects[current_idx]->cleanup) effects[current_idx]->cleanup();
                    current_idx = (current_idx + 1) % NUM_EFFECTS;
                    effects[current_idx]->init();
                    osd_until = now + 2500000LL;
                    last_switch_time = now;
                    ESP_LOGI(TAG, "Single-click: Switched to [%d/%d] %s", current_idx + 1, NUM_EFFECTS, effects[current_idx]->title);
                }
            } else if (press_duration >= 800000LL) {
                if (current_idx == 17) {
                    // 游戏模式时长按：退出游戏并切换回第一个特效
                    if (effects[current_idx]->cleanup) effects[current_idx]->cleanup();
                    current_idx = 0;
                    effects[current_idx]->init();
                    osd_until = now + 2500000LL;
                    last_switch_time = now;
                    ESP_LOGI(TAG, "Exited game mode via long-press.");
                } else {
                    // 常规特效模式时长按：切换自动轮播状态
                    auto_cycle = !auto_cycle;
                    buzzer_beep(50);
                    snprintf(osd_custom_title, sizeof(osd_custom_title), "» AUTO CYCLE: %s «", auto_cycle ? "ON" : "OFF");
                    snprintf(osd_custom_sub, sizeof(osd_custom_sub), auto_cycle ? "已开启自动轮播" : "已锁定当前特效");
                    custom_osd_until = now + 2000000LL;
                    ESP_LOGI(TAG, "Long-press: Auto-cycle %s", auto_cycle ? "ON" : "OFF");
                }
            }
        }
        btn_last_state = !btn_pressed;

        // 4. 自动循环轮播 (默认每 12 秒轮播下一个特效；当玩游戏或开启游戏手柄时不自动切走)
        if (auto_cycle && current_idx != 17 && !bridge->is_game_active && (now - last_switch_time >= 12000000LL)) {
            if (effects[current_idx]->cleanup) effects[current_idx]->cleanup();
            current_idx = (current_idx + 1) % NUM_EFFECTS;
            effects[current_idx]->init();
            osd_until = now + 2500000LL;
            last_switch_time = now;
            ESP_LOGI(TAG, "Auto cycled to: [%d/%d] %s", current_idx + 1, NUM_EFFECTS, effects[current_idx]->title);
        }

        // 5. 计算并更新特效帧
        int64_t t_render_start = esp_timer_get_time();
        effects[current_idx]->update(dt);
        effects[current_idx]->render(framebuffer);
        int64_t t_render_end = esp_timer_get_time();

        // 6. 实时时间与倒计时角标 (同步后在屏幕左下角实时显示 CST-8 时钟)
        if (alarm_ctx->time_synced) {
            time_t now_t = time(NULL);
            struct tm cur_tm;
            localtime_r(&now_t, &cur_tm);
            char clock_badge[16];
            snprintf(clock_badge, sizeof(clock_badge), "%02d:%02d:%02d", cur_tm.tm_hour, cur_tm.tm_min, cur_tm.tm_sec);
            display_draw_string(framebuffer, 6, DISP_H - 14, clock_badge, COLOR_CYAN, COLOR_BLACK, false);
        }

        // 倒计时定时器微型角标 (当后台倒计时运行时在屏幕右下角静默提示)
        if (alarm_ctx->timer_state == TIMER_STATE_RUNNING || alarm_ctx->timer_state == TIMER_STATE_PAUSED) {
            char timer_badge[32];
            int rem = (alarm_ctx->timer_remaining_sec > 0) ? alarm_ctx->timer_remaining_sec : 0;
            int m = (rem / 60) % 100;
            int s = rem % 60;
            snprintf(timer_badge, sizeof(timer_badge), "[TMR %02d:%02d]", m, s);
            display_draw_string(framebuffer, DISP_W - 84, DISP_H - 14, timer_badge, COLOR_YELLOW, COLOR_BLACK, false);
        }

        // 7. 特效切换 / 响铃报警 OSD 悬浮层
        if (alarm_ctx->is_ringing) {
            // 响铃时高优先级全屏闪烁报警层 (红白交替)
            bool flash = ((now / 300000LL) % 2 == 0);
            uint16_t border_col = flash ? COLOR_RED : COLOR_YELLOW;
            display_draw_osd(framebuffer, alarm_ctx->ring_title, "按 BOOT 键或网页端关闭");
            // 屏幕边框高亮红圈警报
            display_draw_box(framebuffer, 1, 1, DISP_W - 2, DISP_H - 2, border_col);
            display_draw_box(framebuffer, 2, 2, DISP_W - 4, DISP_H - 4, border_col);
        } else if (now < custom_osd_until) {
            display_draw_osd(framebuffer, osd_custom_title, osd_custom_sub);
        } else if (now < osd_until) {
            display_draw_osd(framebuffer, effects[current_idx]->title, effects[current_idx]->subtitle);
        }

        // 8. 40MHz SPI DMA 刷新到 ST7789 屏幕 (约 23ms)
        int64_t t_flush_start = esp_timer_get_time();
        display_flush(framebuffer);
        int64_t t_flush_end = esp_timer_get_time();

        // 性能与帧率统计
        static int frame_counter = 0;
        static int64_t fps_timer = 0;
        static int64_t acc_render_us = 0;
        static int64_t acc_flush_us = 0;
        frame_counter++;
        acc_render_us += (t_render_end - t_render_start);
        acc_flush_us += (t_flush_end - t_flush_start);

        if (now - fps_timer >= 2000000LL) {
            float fps = (float)frame_counter * 1000000.0f / (float)(now - fps_timer);
            float avg_render_ms = (float)acc_render_us / (float)frame_counter / 1000.0f;
            float avg_flush_ms = (float)acc_flush_us / (float)frame_counter / 1000.0f;
            float avg_total_ms = 1000.0f / fps;
            ESP_LOGI("PERF", "[%s] FPS: %.1f | Total: %.2f ms | Render: %.2f ms | Flush: %.2f ms | Idle/Delay: %.2f ms",
                     effects[current_idx]->id, fps, avg_total_ms, avg_render_ms, avg_flush_ms,
                     avg_total_ms - avg_render_ms - avg_flush_ms);
            frame_counter = 0;
            acc_render_us = 0;
            acc_flush_us = 0;
            fps_timer = now;
        }

        // 9. 智能动态让步调度：在保持看门狗与高优先级 WiFi 畅通的前提下释放最高帧率
        static int yield_counter = 0;
        if (++yield_counter >= 4) {
            yield_counter = 0;
            vTaskDelay(1); // 每 4 帧让步 1 个 Tick 喂狗并提供低优先级任务执行窗口
        } else {
            taskYIELD(); // 其余帧零延迟让步，彻底释放 60 FPS 流畅潜能
        }
    }
}