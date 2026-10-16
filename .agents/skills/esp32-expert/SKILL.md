---
name: esp32-expert
description: Deep expertise in ESP32 embedded systems, ESP-IDF 5/6, PlatformIO, FreeRTOS kernel, high-FPS display pipelines (ST7789/SPI DMA), and hardware peripherals. Use when architecting, building, profiling, or debugging ESP32/ESP32-C3/ESP32-S3 firmware.
---

# ESP32 Embedded Systems & ESP-IDF Development Skill

This skill provides comprehensive architectural guidelines, coding patterns, and debugging workflows for high-performance ESP32 development under ESP-IDF and PlatformIO.

---

## 1. Hardware Architecture & Memory Hierarchy

### Memory Layout
- **IRAM (Instruction RAM)**: High-speed execution memory (~300-400KB depending on chip). Place critical code (interrupt handlers, high-frequency math, inner loops) in IRAM using `IRAM_ATTR`.
- **DRAM (Data RAM)**: General heap and stack. Always monitor remaining DRAM using `esp_get_free_heap_size()` and `heap_caps_get_free_size(MALLOC_CAP_8BIT)`.
- **Flash (SPI/QPI)**: Mapped via MMU cache. Accessing Flash functions from an ISR causes cache-miss crashes (`Guru Meditation Error: Core 0 panic'ed (Cache disabled but cached memory region accessed)`).

### Memory Safety Rulebook
1. **ISR Safety**: Any function called from an interrupt handler or timer ISR MUST have `IRAM_ATTR` and must NOT allocate heap memory or execute blocking calls.
2. **Buffer Alignment for DMA**: All buffers passed to SPI DMA (`spi_device_polling_transmit` / `spi_device_queue_trans`) MUST be allocated in DMA-capable memory with 32-bit alignment:
   ```c
   uint16_t *buf = heap_caps_malloc(size, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
   ```

---

## 2. High-FPS Display Pipelines (ST7789 / SPI Master DMA)

### Optimization Guidelines for 60+ FPS
1. **SPI Frequency**: Clock ST7789 at 40MHz or 80MHz (`clock_speed_hz = 40000000`).
2. **DMA Chunking & Double Buffering**:
   - Transmit in lines or bands (e.g. 240x20 or 240x40 lines) to balance RAM usage and DMA throughput.
   - While DMA is flushing band $N$ asynchronously (`spi_device_queue_trans`), render band $N+1$ in CPU.
3. **Color Space**: ST7789 uses big-endian RGB565 (`(R << 11) | (G << 5) | B`, byte-swapped). Pre-compute endianness swap in lookup tables or during pixel packing.

---

## 3. FreeRTOS Kernel & Task Architecture

### Task Priorities & Core Affinity
- **Core 0 (System & Wi-Fi)**: Wi-Fi driver task runs at priority 23, LWIP TCPIP task at priority 18.
- **Core 1 / CPU (Rendering / App)**: Run rendering and game loop at priority 5.
- **Task Stack Sizing**:
  - Minimum stack for simple tasks: 2048 bytes.
  - Tasks calling `printf`/`ESP_LOGI` or sockets: minimum 4096 bytes.
  - WebServer / HTTP handlers: minimum 8192 bytes.
- **Watchdog Prevention**: Always yield or sleep (`vTaskDelay(pdMS_TO_TICKS(1))` or `taskYIELD()`) in infinite loops to feed the FreeRTOS Task Watchdog Timer (TWDT).

---

## 4. Hardware Audio & PWM (LEDC Buzzer)

- Use LEDC (LED Controller) for non-blocking square-wave sound effects.
- Set timer frequency dynamically via `ledc_set_freq()` and toggle volume via `ledc_set_duty()`.
- Use a dedicated sound queue or non-blocking timer to step through multi-tone sound effects without blocking the graphics rendering task.

---

## 5. Diagnostic Runbook

### Guru Meditation / Crash Diagnosis
1. Decode PC and Backtrace:
   ```bash
   riscv32-esp-elf-addr2line -pfiaC -e .pio/build/airm2m_core_esp32c3/firmware.elf 0x4038...
   ```
2. Check stack high-water mark:
   ```c
   UBaseType_t hwm = uxTaskGetStackHighWaterMark(NULL);
   ESP_LOGI("PERF", "Task free stack: %d words", hwm);
   ```
3. Check heap fragmentation:
   ```c
   multi_heap_info_t info;
   heap_caps_get_info(&info, MALLOC_CAP_INTERNAL);
   ESP_LOGI("HEAP", "Free: %u, Largest block: %u", info.total_free_bytes, info.largest_free_block);
   ```
