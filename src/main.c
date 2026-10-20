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

static const char *TAG = "CLAWD_MOCHI";

#define PIN_BOOT_BUTTON GPIO_NUM_9
#define PIN_BUZZER      GPIO_NUM_6

// 39 款全屏硬核极客纯视觉特效阵列 (已剔除烟花与头像，增加10款全新硬核画面)
static Effect *effects[] = {
    &fx_donut,             // 1. 3D 光影旋转甜甜圈 (老样子开机首屏)
    &fx_synthwave,         // 2. 赛博朋克 80s 霓虹落日与透视网格
    &fx_matrix,            // 3. 黑客帝国数字流光雨
    &fx_plasma,            // 4. 迷幻复古彩虹等离子电浆
    &fx_spectrum,          // 5. 赛博霓虹音频频谱瀑布与镜面水波
    &fx_blackhole,         // 6. 引力透镜黑洞与相对论吸积盘
    &fx_galaxy,            // 7. 3D 银河旋涡与星云粒子潮
    &fx_fire,              // 8. 经典毁灭战士火焰
    &fx_tunnel,            // 9. 时空跃迁虫洞光速飞驰隧道
    &fx_stars,             // 10. 超空间星空与 3D 旋转超立方体
    &fx_dna,               // 11. 3D 炫彩旋转 DNA 双螺旋粒子链
    &fx_metaballs,         // 12. 霓虹流体熔岩双态融球
    &fx_radar,             // 13. 赛博全息战术雷达 HUD
    &fx_oscilloscope,      // 14. 赛博激光李萨如谐波示波器
    &fx_gameoflife,        // 15. 康威量子元胞自动机生命游戏
    &fx_aurora,            // 16. 极地深空极光与雪山倒影
    &fx_fractal,           // 17. 电光朱利亚全动态复变分形云
    &fx_reactor,           // 18. 赛博方舟反应堆 (高能聚变等离子电磁线圈)
    &fx_cyber_tunnel,      // 19. 赛博六角隧道 (高能几何光子跃迁通道)
    &fx_laser_grid,        // 20. 赛博激光阵列 (多维空间全息扫描交错光束)
    &fx_glitch,            // 21. 赛博故障裂缝 (数字信号撕裂与RGB色差故障)
    &fx_collider,          // 22. 赛博强子对撞机 (双向高能粒子流环形对撞)
    &fx_lightning,         // 23. 赛博特斯拉电弧 (高压分形放电能量分支网络)
    &fx_tesseract,         // 24. 赛博四维超正方体 (4D高维超立方体双平面旋转投影)
    &fx_compass,           // 25. 赛博全息罗盘 (战机HUD导航姿态仪与电子罗盘)
    &fx_sonar,             // 26. 赛博深空声纳 (极坐标脉冲回波与隐匿目标探测)
    &fx_gravity_grid,      // 27. 赛博引力时空曲率 (双黑洞相对论时空曲率漏斗拉扯)
    &fx_kaleidoscope,      // 28. 迷幻光影万花筒 (8重对称曼陀罗绚烂几何)
    &fx_sphere,            // 29. 赛博经纬三维球 (3D旋转线框经纬网格球)
    &fx_clockwork,         // 30. 赛博机械时钟齿轮 (行星齿轮差速啮合动力系统)
    &fx_mobius,            // 31. 赛博莫比乌斯环 (3D单侧拓扑曲面空间连续翻转)
    &fx_matrix_3d,         // 32. 赛博3D矩阵代码瀑布 (全景深数字流空间穿梭冲屏)
    &fx_pulsar,            // 33. 赛博脉冲星射流 (高速自转中子星磁层与高能相对论喷流)
    &fx_quantum,           // 34. 赛博量子波函数干涉 (多相位相干几率波干涉纹理云)
    &fx_lightspeed,        // 35. 赛博光轨穿梭 (高速网格光速粒子折射流光墙)
    &fx_icosahedron,       // 36. 赛博二十面体水晶 (黄金比例3D多面体空间自旋晶体)
    &fx_circular_spectrum, // 37. 赛博环形音频频谱仪 (360度圆周重低音电音律动辐射柱)
    &fx_hex_wave,          // 38. 赛博蜂巢矩阵波 (六角蜂窝弹性涟漪起伏动力场)
    &fx_cipher_lock        // 39. 赛博核心解密锁环 (三重同心差速密码盘对齐解锁)
};

#define NUM_EFFECTS (int)(sizeof(effects) / sizeof(effects[0]))

// 智能随机洗牌播放列表算法 (Fisher-Yates Shuffle)
// 保证 39 款特效全部随机轮播，绝不连续重复，每轮洗牌均等展示
static int shuffle_list[NUM_EFFECTS];
static int shuffle_ptr = 0;

static void init_shuffle_playlist(bool keep_first_donut) {
    for (int i = 0; i < NUM_EFFECTS; i++) {
        shuffle_list[i] = i;
    }

    int start_i = keep_first_donut ? 1 : 0;
    // Fisher-Yates 洗牌
    for (int i = NUM_EFFECTS - 1; i > start_i; i--) {
        int j = start_i + (fast_rand() % (i - start_i + 1));
        int tmp = shuffle_list[i];
        shuffle_list[i] = shuffle_list[j];
        shuffle_list[j] = tmp;
    }
    shuffle_ptr = 0;
}

static int get_next_shuffle_effect(void) {
    shuffle_ptr++;
    if (shuffle_ptr >= NUM_EFFECTS) {
        int last_effect = shuffle_list[NUM_EFFECTS - 1];
        // 重新完整洗牌
        init_shuffle_playlist(false);
        // 保证新一轮首个特效与上一轮最后特效不重复
        if (shuffle_list[0] == last_effect) {
            int swap_idx = 1 + (fast_rand() % (NUM_EFFECTS - 1));
            int tmp = shuffle_list[0];
            shuffle_list[0] = shuffle_list[swap_idx];
            shuffle_list[swap_idx] = tmp;
        }
    }
    return shuffle_list[shuffle_ptr];
}

#define ENABLE_BUZZER 1

static void buzzer_beep(int ms) {
#if ENABLE_BUZZER
    gpio_set_direction(PIN_BUZZER, GPIO_MODE_OUTPUT);
    gpio_set_level(PIN_BUZZER, 0);
    vTaskDelay(pdMS_TO_TICKS(ms));
    gpio_set_direction(PIN_BUZZER, GPIO_MODE_INPUT);
#else
    (void)ms;
#endif
}

void app_main(void) {
    ESP_LOGI(TAG, "=========================================");
    ESP_LOGI(TAG, " Clawd Mochi - Pure High-FPS Visual Art  ");
    ESP_LOGI(TAG, " 39 Effects | Shuffle Playlist | Standalone ");
    ESP_LOGI(TAG, "=========================================");

    // 1. 初始化硬件数学加速查表
    fast_math_init();

    // 2. 初始化按键 (BOOT 键 GPIO 9)
    gpio_config_t btn_conf = {
        .pin_bit_mask = (1ULL << PIN_BOOT_BUTTON),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&btn_conf);

    // 3. 蜂鸣器初始化：设为高阻输入模式 (Hi-Z)，彻底静音
    gpio_config_t bz_conf = {
        .pin_bit_mask = (1ULL << PIN_BUZZER),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&bz_conf);

    // 4. 初始化 ST7789 显示屏 (40MHz SPI DMA)
    if (!display_init()) {
        ESP_LOGE(TAG, "Failed to initialize display!");
        return;
    }

    // 5. 静态 4 字节对齐的 240x240 全屏 16-bit DMA 显存 (115.2 KB)
    static uint16_t g_framebuffer[DISP_W * DISP_H] __attribute__((aligned(4)));
    uint16_t *framebuffer = g_framebuffer;

    // 6. 初始化 Fisher-Yates 随机播放洗牌列表 (首屏保留经典老样子 3D 甜甜圈)
    init_shuffle_playlist(true);

    // 开机轻柔单音提示 (25ms)，开机无弹窗直接呈现 3D 甜甜圈
    buzzer_beep(25);

    int current_idx = shuffle_list[shuffle_ptr];
    effects[current_idx]->init();

    int64_t last_time = esp_timer_get_time();
    int64_t last_switch_time = last_time;

    bool btn_last_state = true;
    int64_t btn_down_time = 0;
    int64_t last_click_time = 0;
    bool auto_cycle = true; // 默认开启 12 秒自动无感随机轮播

    ESP_LOGI(TAG, "Started 39-effect visual engine with non-repeating shuffle (Zero Popups)...");

    while (1) {
        int64_t now = esp_timer_get_time();
        float dt = (float)(now - last_time) / 1000000.0f;
        last_time = now;

        // 1. 处理物理 BOOT 按键 (GPIO 9, 低电平触发)
        bool btn_pressed = (gpio_get_level(PIN_BOOT_BUTTON) == 0);
        if (btn_pressed && btn_last_state) {
            btn_down_time = now;
        } else if (!btn_pressed && !btn_last_state) {
            int64_t press_duration = now - btn_down_time;

            if (press_duration > 40000LL && press_duration < 800000LL) {
                int64_t time_since_last_click = now - last_click_time;
                if (time_since_last_click < 400000LL) {
                    // 双击：旋转屏幕 90 度 (微声反馈，无弹窗)
                    uint8_t next_rot = (display_get_rotation() + 1) % 4;
                    display_set_rotation(next_rot);
                    buzzer_beep(30);
                    last_click_time = 0;
                    ESP_LOGI(TAG, "Double-click: Screen rotated to %d deg", next_rot * 90);
                } else {
                    last_click_time = now;
                    // 单击：随机洗牌切换下一个特效 (静音切换，无弹窗)
                    if (effects[current_idx]->cleanup) effects[current_idx]->cleanup();
                    current_idx = get_next_shuffle_effect();
                    effects[current_idx]->init();
                    last_switch_time = now;
                    ESP_LOGI(TAG, "Single-click [Shuffle %d/%d]: %s", shuffle_ptr + 1, NUM_EFFECTS, effects[current_idx]->title);
                }
            } else if (press_duration >= 800000LL) {
                // 长按：切换自动轮播状态 (轻微声反馈，无弹窗)
                auto_cycle = !auto_cycle;
                buzzer_beep(50);
                ESP_LOGI(TAG, "Long-press: Auto-cycle %s", auto_cycle ? "ON" : "OFF");
            }
        }
        btn_last_state = !btn_pressed;

        // 2. 自动循环轮播 (默认每 12 秒按随机洗牌序列轮播下一个画面，纯净无弹窗)
        if (auto_cycle && (now - last_switch_time >= 12000000LL)) {
            if (effects[current_idx]->cleanup) effects[current_idx]->cleanup();
            current_idx = get_next_shuffle_effect();
            effects[current_idx]->init();
            last_switch_time = now;
            ESP_LOGI(TAG, "Auto cycled [Shuffle %d/%d]: %s", shuffle_ptr + 1, NUM_EFFECTS, effects[current_idx]->title);
        }

        // 3. 计算并更新特效帧 (全屏纯净渲染，0 弹窗)
        int64_t t_render_start = esp_timer_get_time();
        effects[current_idx]->update(dt);
        effects[current_idx]->render(framebuffer);
        int64_t t_render_end = esp_timer_get_time();

        // 4. 40MHz SPI DMA 硬件级极速推屏刷新到 ST7789 屏幕
        int64_t t_flush_start = esp_timer_get_time();
        display_flush(framebuffer);
        int64_t t_flush_end = esp_timer_get_time();

        // 性能与帧率统计日志 (通过串口输出，完全不影响屏幕纯净)
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
            ESP_LOGI("PERF", "[%s] FPS: %.1f | Render: %.2f ms | Flush: %.2f ms",
                     effects[current_idx]->id, fps, avg_render_ms, avg_flush_ms);
            frame_counter = 0;
            acc_render_us = 0;
            acc_flush_us = 0;
            fps_timer = now;
        }

        // 让步 2ms 喂看门狗
        vTaskDelay(pdMS_TO_TICKS(2));
    }
}