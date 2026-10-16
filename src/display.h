#ifndef DISPLAY_H
#define DISPLAY_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define DISP_W 240
#define DISP_H 240

// 引脚定义 (Clawd Mochi 硬件规格)
#define TFT_MOSI 10  // SDA
#define TFT_SCLK 8   // SCL
#define TFT_RST  2   // RES
#define TFT_DC   1   // DC
#define TFT_CS   4   // CS
#define TFT_BLK  3   // 背光

// 常用 RGB565 颜色 (预先按 SPI 大端序字节交换)
static inline uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
    uint16_t c = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
    return __builtin_bswap16(c);
}

#define COLOR_BLACK   0x0000
#define COLOR_WHITE   0xFFFF
#define COLOR_RED     rgb565(255, 0, 0)
#define COLOR_GREEN   rgb565(0, 255, 0)
#define COLOR_BLUE    rgb565(0, 0, 255)
#define COLOR_CYAN    rgb565(0, 255, 255)
#define COLOR_MAGENTA rgb565(255, 0, 255)
#define COLOR_YELLOW  rgb565(255, 255, 0)
#define COLOR_ORANGE  rgb565(255, 140, 0)

// 初始化 ST7789 屏幕及 SPI DMA
bool display_init(void);

// 设置屏幕朝向 (0: 0°, 1: 90° Clawd Mochi原生正向, 2: 180°, 3: 270°)
void display_set_rotation(uint8_t r);
uint8_t display_get_rotation(void);

// 开启/关闭背光
void display_set_backlight(bool on);

// 全屏 DMA 刷新帧缓存
void display_flush(const uint16_t *buffer);

// 基础绘图函数
void display_draw_pixel(uint16_t *buf, int x, int y, uint16_t color);
void display_draw_line(uint16_t *buf, int x0, int y0, int x1, int y1, uint16_t color);
void display_draw_char(uint16_t *buf, int x, int y, char c, uint16_t fg, uint16_t bg, bool transparent);
void display_draw_string(uint16_t *buf, int x, int y, const char *str, uint16_t fg, uint16_t bg, bool transparent);
void display_draw_box(uint16_t *buf, int x, int y, int w, int h, uint16_t color);
void display_fill_rect(uint16_t *buf, int x, int y, int w, int h, uint16_t color);

// 绘制特效切换顶部/底部 OSD 悬浮提示框
void display_draw_osd(uint16_t *buf, const char *title, const char *subtitle);

#endif // DISPLAY_H
