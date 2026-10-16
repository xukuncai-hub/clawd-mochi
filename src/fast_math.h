#ifndef FAST_MATH_H
#define FAST_MATH_H

#include <stdint.h>
#include <stdbool.h>
#include <math.h>

#define FAST_PI 3.14159265358979323846f

extern float g_sin_lut[256];

void fast_math_init(void);

// 极速查表正弦函数 (2~3 个时钟周期，替代软浮点 sinf)
static inline float fast_sin(float rad) {
    // 映射 rad 到 [0, 256)
    int idx = (int)(rad * (256.0f / (2.0f * FAST_PI))) & 255;
    return g_sin_lut[idx];
}

// 极速查表余弦函数
static inline float fast_cos(float rad) {
    int idx = (int)((rad + (FAST_PI * 0.5f)) * (256.0f / (2.0f * FAST_PI))) & 255;
    return g_sin_lut[idx];
}

// 极速 32 位 XorShift 伪随机数发生器 (单周期指令，彻底消除 libc rand() 锁与软开销)
static inline uint32_t fast_rand(void) {
    static uint32_t state = 2463534242UL;
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return state;
}

static inline int fast_rand_range(int min, int max) {
    if (max <= min) return min;
    return min + (int)(fast_rand() % (uint32_t)(max - min + 1));
}

static inline float fast_rand_float(void) {
    return (float)(fast_rand() & 0xFFFF) / 65535.0f;
}

#endif // FAST_MATH_H
