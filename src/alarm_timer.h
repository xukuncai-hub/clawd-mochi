#ifndef ALARM_TIMER_H
#define ALARM_TIMER_H

#include <stdint.h>
#include <stdbool.h>
#include <time.h>

typedef enum {
    TIMER_STATE_IDLE = 0,
    TIMER_STATE_RUNNING,
    TIMER_STATE_PAUSED,
    TIMER_STATE_RINGING
} TimerState;

typedef struct {
    // 闹钟设置
    bool alarm_enabled;
    int alarm_hour;
    int alarm_minute;
    int last_alarm_day; // 避免同分钟内重复触发

    // 倒计时定时器
    TimerState timer_state;
    int timer_total_sec;
    int timer_remaining_sec;

    // 响铃警报状态
    bool is_ringing;
    char ring_title[32]; // "ALARM WAKE UP!" 或 "TIMER FINISHED!"
    int ring_counter;

    // 系统时间同步标志
    bool time_synced;
} AlarmTimerContext;

void alarm_timer_init(void);
AlarmTimerContext* alarm_timer_get_ctx(void);

// 时间与时区设置
void alarm_timer_set_time(int hour, int min, int sec);
void alarm_timer_set_datetime(int year, int mon, int day, int hour, int min, int sec);
void alarm_timer_set_timestamp(time_t ts);

// 闹钟控制
void alarm_timer_set_alarm(bool enabled, int hour, int min);

// 定时器控制
void alarm_timer_start(int seconds);
void alarm_timer_pause(void);
void alarm_timer_resume(void);
void alarm_timer_stop(void);

// 关闭响铃警报 (按 BOOT 键或网页点击)
void alarm_timer_dismiss_ring(void);

// 主循环定时调用
void alarm_timer_update(void);

#endif // ALARM_TIMER_H
