#include "display.h"
#include "font.h"

#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_log.h"

static const char *TAG = "ST7789";
static spi_device_handle_t spi;

static void lcd_cmd(uint8_t cmd) {
    gpio_set_level(TFT_DC, 0);
    spi_transaction_t t = {
        .length = 8,
        .tx_buffer = &cmd
    };
    spi_device_polling_transmit(spi, &t);
}

static void lcd_data(const uint8_t *data, int len) {
    if (len <= 0) return;
    gpio_set_level(TFT_DC, 1);
    spi_transaction_t t = {
        .length = (size_t)len * 8,
        .tx_buffer = data
    };
    spi_device_polling_transmit(spi, &t);
}

bool display_init(void) {
    ESP_LOGI(TAG, "Initializing ST7789 Display GPIOs & SPI...");

    // 1. 初始化控制引脚
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << TFT_DC) | (1ULL << TFT_RST) | (1ULL << TFT_BLK),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);

    // 默认关闭背光，硬件复位
    gpio_set_level(TFT_BLK, 0);
    gpio_set_level(TFT_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(20));
    gpio_set_level(TFT_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(100));

    // 2. 初始化 SPI 总线 (使用 SPI2_HOST，40MHz 极速传输并开启 DMA)
    spi_bus_config_t buscfg = {
        .miso_io_num = -1,
        .mosi_io_num = TFT_MOSI,
        .sclk_io_num = TFT_SCLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = DISP_W * 60 * 2 + 8 // 28808 字节，完全在 ESP32-C3 32KB 硬件上限内
    };

    esp_err_t ret = spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize SPI bus: %d", ret);
        return false;
    }

    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 40 * 1000 * 1000, // 40 MHz 硬件标准额定稳定时钟，杜绝半屏卡死与时序失步
        .mode = 0,                           // SPI Mode 0
        .spics_io_num = TFT_CS,
        .queue_size = 7,
        .flags = SPI_DEVICE_NO_DUMMY
    };

    ret = spi_bus_add_device(SPI2_HOST, &devcfg, &spi);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add SPI device: %d", ret);
        return false;
    }

    // 3. ST7789 寄存器初始化指令序列
    lcd_cmd(0x01); // 软件复位 (SWRESET)
    vTaskDelay(pdMS_TO_TICKS(150));

    lcd_cmd(0x11); // 退出休眠 (SLPOUT)
    vTaskDelay(pdMS_TO_TICKS(120));

    // 16位色彩格式 RGB565
    lcd_cmd(0x3A);
    const uint8_t colmod = 0x55;
    lcd_data(&colmod, 1);

    // 开启反色 (IPS 屏幕必须开启 INVON 以获得纯正色彩)
    lcd_cmd(0x21);

    // 设置默认屏幕朝向 (Clawd Mochi 原生 Rotation 1)
    display_set_rotation(1);

    lcd_cmd(0x13); // 正常显示模式
    vTaskDelay(pdMS_TO_TICKS(10));

    lcd_cmd(0x29); // 开启显示 (DISPON)
    vTaskDelay(pdMS_TO_TICKS(50));

    // 开启背光
    display_set_backlight(true);

    ESP_LOGI(TAG, "ST7789 initialized successfully.");
    return true;
}

static uint8_t g_rotation = 1; // 默认采用 Clawd Mochi 官方设计朝向 (Rotation 1)
static uint16_t g_xstart = 80;
static uint16_t g_ystart = 0;

void display_set_rotation(uint8_t r) {
    g_rotation = r % 4;
    uint8_t madctl = 0;
    switch (g_rotation) {
        case 0:
            madctl = 0x00; // 0 度
            g_xstart = 0;
            g_ystart = 0;
            break;
        case 1:
            // 90 度 (Clawd Mochi 原生方向: tft.setRotation(1))
            madctl = 0xA0; // MY=1, MV=1
            g_xstart = 80;
            g_ystart = 0;
            break;
        case 2:
            madctl = 0xC0; // 180 度 (MX=1, MY=1)
            g_xstart = 0;
            g_ystart = 80;
            break;
        case 3:
            madctl = 0x60; // 270 度 (MX=1, MV=1)
            g_xstart = 0;
            g_ystart = 0;
            break;
    }
    lcd_cmd(0x36);
    lcd_data(&madctl, 1);
}

uint8_t display_get_rotation(void) {
    return g_rotation;
}

void display_set_backlight(bool on) {
    gpio_set_level(TFT_BLK, on ? 1 : 0);
}

void display_flush(const uint16_t *buffer) {
    if (!buffer) return;

    // 设置列地址窗口 (带旋转偏移量)
    lcd_cmd(0x2A);
    uint16_t x0 = g_xstart;
    uint16_t x1 = g_xstart + DISP_W - 1;
    uint8_t ca[4] = {(uint8_t)(x0 >> 8), (uint8_t)(x0 & 0xFF), (uint8_t)(x1 >> 8), (uint8_t)(x1 & 0xFF)};
    lcd_data(ca, 4);

    // 设置行地址窗口 (带旋转偏移量)
    lcd_cmd(0x2B);
    uint16_t y0 = g_ystart;
    uint16_t y1 = g_ystart + DISP_H - 1;
    uint8_t ra[4] = {(uint8_t)(y0 >> 8), (uint8_t)(y0 & 0xFF), (uint8_t)(y1 >> 8), (uint8_t)(y1 & 0xFF)};
    lcd_data(ra, 4);

    // 写显存指令 (RAMWR)
    lcd_cmd(0x2C);

    // 使用 DMA 分块流式高速推送 (每次 60 行，共 4 块，28.8KB 完美契合 ESP32-C3 硬件 SPI 32KB 限制)
    gpio_set_level(TFT_DC, 1);
    const int lines_per_chunk = 60;
    const int chunk_bytes = DISP_W * lines_per_chunk * 2;
    const uint8_t *raw_buf = (const uint8_t *)buffer;

    for (int y = 0; y < DISP_H; y += lines_per_chunk) {
        spi_transaction_t t = {
            .length = (size_t)chunk_bytes * 8,
            .tx_buffer = raw_buf + (size_t)y * DISP_W * 2
        };
        spi_device_polling_transmit(spi, &t);
    }
}

void display_draw_pixel(uint16_t *buf, int x, int y, uint16_t color) {
    if (x >= 0 && x < DISP_W && y >= 0 && y < DISP_H) {
        buf[y * DISP_W + x] = color;
    }
}

void display_draw_line(uint16_t *buf, int x0, int y0, int x1, int y1, uint16_t color) {
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy, e2;

    while (1) {
        display_draw_pixel(buf, x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

void display_draw_char(uint16_t *buf, int x, int y, char c, uint16_t fg, uint16_t bg, bool transparent) {
    if (c < 32 || c > 126) c = ' ';
    const uint8_t *glyph = font8x8[c - 32];

    for (int row = 0; row < 8; row++) {
        uint8_t line = glyph[row];
        for (int col = 0; col < 8; col++) {
            if (line & (0x80 >> col)) {
                display_draw_pixel(buf, x + col, y + row, fg);
            } else if (!transparent) {
                display_draw_pixel(buf, x + col, y + row, bg);
            }
        }
    }
}

void display_draw_string(uint16_t *buf, int x, int y, const char *str, uint16_t fg, uint16_t bg, bool transparent) {
    while (*str) {
        display_draw_char(buf, x, y, *str, fg, bg, transparent);
        x += 8;
        str++;
    }
}

void display_fill_rect(uint16_t *buf, int x, int y, int w, int h, uint16_t color) {
    for (int j = y; j < y + h; j++) {
        if (j < 0 || j >= DISP_H) continue;
        for (int i = x; i < x + w; i++) {
            if (i >= 0 && i < DISP_W) {
                buf[j * DISP_W + i] = color;
            }
        }
    }
}

void display_draw_box(uint16_t *buf, int x, int y, int w, int h, uint16_t color) {
    display_draw_line(buf, x, y, x + w - 1, y, color);
    display_draw_line(buf, x, y + h - 1, x + w - 1, y + h - 1, color);
    display_draw_line(buf, x, y, x, y + h - 1, color);
    display_draw_line(buf, x + w - 1, y, x + w - 1, y + h - 1, color);
}

void display_draw_osd(uint16_t *buf, const char *title, const char *subtitle) {
    // 顶部科技感深空曜石灰悬浮徽标
    int bw = 216, bh = 34;
    int bx = (DISP_W - bw) / 2;
    int by = 8;

    uint16_t bg_dark = rgb565(8, 12, 24);
    uint16_t border_col = rgb565(0, 220, 255);
    uint16_t corner_accent = COLOR_WHITE;

    display_fill_rect(buf, bx, by, bw, bh, bg_dark);
    display_draw_box(buf, bx, by, bw, bh, border_col);

    // 四角赛博机甲包角装饰
    display_draw_line(buf, bx, by, bx + 5, by, corner_accent);
    display_draw_line(buf, bx, by, bx, by + 5, corner_accent);
    display_draw_line(buf, bx + bw - 6, by, bx + bw - 1, by, corner_accent);
    display_draw_line(buf, bx + bw - 1, by, bx + bw - 1, by + 5, corner_accent);
    display_draw_line(buf, bx, by + bh - 1, bx + 5, by + bh - 1, corner_accent);
    display_draw_line(buf, bx, by + bh - 6, bx, by + bh - 1, corner_accent);
    display_draw_line(buf, bx + bw - 6, by + bh - 1, bx + bw - 1, by + bh - 1, corner_accent);
    display_draw_line(buf, bx + bw - 1, by + bh - 6, bx + bw - 1, by + bh - 1, corner_accent);

    int title_len = (int)strlen(title);
    int title_x = (DISP_W - title_len * 8) / 2;
    display_draw_string(buf, title_x, by + 6, title, border_col, bg_dark, true);

    if (subtitle) {
        int sub_len = (int)strlen(subtitle);
        int sub_x = (DISP_W - sub_len * 8) / 2;
        display_draw_string(buf, sub_x, by + 18, subtitle, COLOR_WHITE, bg_dark, true);
    }
}
