#include "alarm_timer.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_log.h"
#include <sys/time.h>
#include <string.h>

static const char *TAG = "ALARM_TIMER";

#define PIN_BUZZER GPIO_NUM_6

static AlarmTimerContext g_ctx;
static int64_t last_sec_tick = 0;
static int ring_step = 0;

static void buzzer_set_sound(bool on) {
    if (on) {
        gpio_set_direction(PIN_BUZZER, GPIO_MODE_OUTPUT);
        gpio_set_level(PIN_BUZZER, 0); // 低电平触发蜂鸣器发声
    } else {
        gpio_set_direction(PIN_BUZZER, GPIO_MODE_INPUT); // 恢复高阻输入彻底静音
    }
}

void alarm_timer_init(void) {
    memset(&g_ctx, 0, sizeof(g_ctx));
    g_ctx.alarm_enabled = false;
    g_ctx.alarm_hour = 8;
    g_ctx.alarm_minute = 0;
    g_ctx.last_alarm_day = -1;

    g_ctx.timer_state = TIMER_STATE_IDLE;
    g_ctx.timer_total_sec = 0;
    g_ctx.timer_remaining_sec = 0;

    g_ctx.is_ringing = false;
    g_ctx.ring_counter = 0;
    g_ctx.time_synced = false;

    // 初始蜂鸣器静音
    buzzer_set_sound(false);
    last_sec_tick = esp_timer_get_time();

    // 关键修复：配置中国标准时间东八区时区 (CST-8)，确保 POSIX 时间函数与手机时间 100% 同步
    setenv("TZ", "CST-8", 1);
    tzset();
}

AlarmTimerContext* alarm_timer_get_ctx(void) {
    return &g_ctx;
}

void alarm_timer_set_datetime(int year, int mon, int day, int hour, int min, int sec) {
    struct tm tm_info = {0};
    tm_info.tm_year = year - 1900;
    tm_info.tm_mon = mon - 1;
    tm_info.tm_mday = day;
    tm_info.tm_hour = hour;
    tm_info.tm_min = min;
    tm_info.tm_sec = sec;
    tm_info.tm_isdst = 0;

    time_t ts = mktime(&tm_info);
    if (ts > 0) {
        struct timeval tv = { .tv_sec = ts, .tv_usec = 0 };
        settimeofday(&tv, NULL);
        g_ctx.time_synced = true;
        ESP_LOGI(TAG, "Exact DateTime synced: %04d-%02d-%02d %02d:%02d:%02d",
                 year, mon, day, hour, min, sec);
    }
}

void alarm_timer_set_timestamp(time_t ts) {
    struct timeval tv = {
        .tv_sec = ts,
        .tv_usec = 0
    };
    settimeofday(&tv, NULL);
    g_ctx.time_synced = true;

    time_t now = time(NULL);
    struct tm tm_info;
    localtime_r(&now, &tm_info);
    ESP_LOGI(TAG, "Timestamp synced: %02d:%02d:%02d (CST-8)", tm_info.tm_hour, tm_info.tm_min, tm_info.tm_sec);
}

void alarm_timer_set_time(int hour, int min, int sec) {
    time_t now = time(NULL);
    struct tm tm_info;
    localtime_r(&now, &tm_info);

    tm_info.tm_hour = hour;
    tm_info.tm_min = min;
    tm_info.tm_sec = sec;

    time_t new_ts = mktime(&tm_info);
    alarm_timer_set_timestamp(new_ts);
}

void alarm_timer_set_alarm(bool enabled, int hour, int min) {
    g_ctx.alarm_enabled = enabled;
    g_ctx.alarm_hour = hour % 24;
    g_ctx.alarm_minute = min % 60;
    ESP_LOGI(TAG, "Alarm set: %s [%02d:%02d]", enabled ? "ENABLED" : "DISABLED", g_ctx.alarm_hour, g_ctx.alarm_minute);
}

void alarm_timer_start(int seconds) {
    if (seconds <= 0) return;
    g_ctx.timer_total_sec = seconds;
    g_ctx.timer_remaining_sec = seconds;
    g_ctx.timer_state = TIMER_STATE_RUNNING;
    ESP_LOGI(TAG, "Timer started: %d seconds", seconds);
}

void alarm_timer_pause(void) {
    if (g_ctx.timer_state == TIMER_STATE_RUNNING) {
        g_ctx.timer_state = TIMER_STATE_PAUSED;
        ESP_LOGI(TAG, "Timer paused at %d seconds", g_ctx.timer_remaining_sec);
    }
}

void alarm_timer_resume(void) {
    if (g_ctx.timer_state == TIMER_STATE_PAUSED) {
        g_ctx.timer_state = TIMER_STATE_RUNNING;
        ESP_LOGI(TAG, "Timer resumed at %d seconds", g_ctx.timer_remaining_sec);
    }
}

void alarm_timer_stop(void) {
    g_ctx.timer_state = TIMER_STATE_IDLE;
    g_ctx.timer_remaining_sec = 0;
    if (g_ctx.is_ringing) {
        alarm_timer_dismiss_ring();
    }
    ESP_LOGI(TAG, "Timer stopped");
}

void alarm_timer_dismiss_ring(void) {
    g_ctx.is_ringing = false;
    g_ctx.ring_counter = 0;
    ring_step = 0;
    buzzer_set_sound(false);
    if (g_ctx.timer_state == TIMER_STATE_RINGING) {
        g_ctx.timer_state = TIMER_STATE_IDLE;
    }
    ESP_LOGI(TAG, "Ring dismissed by user.");
}

void alarm_timer_update(void) {
    int64_t now_us = esp_timer_get_time();

    // 1. 每秒逻辑判定 (时间、闹钟、倒计时)
    if (now_us - last_sec_tick >= 1000000LL) {
        last_sec_tick = now_us;

        // 倒计时更新
        if (g_ctx.timer_state == TIMER_STATE_RUNNING) {
            g_ctx.timer_remaining_sec--;
            if (g_ctx.timer_remaining_sec <= 0) {
                g_ctx.timer_state = TIMER_STATE_RINGING;
                g_ctx.is_ringing = true;
                strncpy(g_ctx.ring_title, "» 00:00 TIMER DONE! «", sizeof(g_ctx.ring_title));
                ESP_LOGI(TAG, "Timer completed! Ringing alarm...");
            }
        }

        // 闹钟检查 (若已同步真实时间)
        if (g_ctx.time_synced && g_ctx.alarm_enabled) {
            time_t now_t = time(NULL);
            struct tm cur_tm;
            localtime_r(&now_t, &cur_tm);

            if (cur_tm.tm_hour == g_ctx.alarm_hour &&
                cur_tm.tm_min == g_ctx.alarm_minute &&
                cur_tm.tm_sec == 0) {
                if (g_ctx.last_alarm_day != cur_tm.tm_yday) {
                    g_ctx.last_alarm_day = cur_tm.tm_yday;
                    g_ctx.is_ringing = true;
                    snprintf(g_ctx.ring_title, sizeof(g_ctx.ring_title), "» %02d:%02d ALARM RING! «", g_ctx.alarm_hour, g_ctx.alarm_minute);
                    ESP_LOGI(TAG, "Alarm triggered! Ringing...");
                }
            }
        }
    }

    // 2. 响铃音律节奏生成 (非阻塞微秒节奏)
    if (g_ctx.is_ringing) {
        ring_step = (ring_step + 1) % 30; // 约 30 帧循环 (约 1 秒周期)

        // 节奏模式：双滴鸣叫 [滴(80ms) 停(80ms) 滴(80ms) 静音长停]
        if (ring_step == 0 || ring_step == 1 || ring_step == 4 || ring_step == 5) {
            buzzer_set_sound(true);
        } else {
            buzzer_set_sound(false);
        }

        g_ctx.ring_counter++;
        // 自动超时保护 (连续响铃 90 秒后自动停止)
        if (g_ctx.ring_counter > 30 * 90) {
            alarm_timer_dismiss_ring();
        }
    }
}
