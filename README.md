<!-- LOGO -->
<p align="center">
  <img src="pics/clawd_mochi_banner.png" alt="Clawd Mochi Logo" width="700"/>
</p>

# Clawd Mochi PRO 🦀⚡ · 39 特效纯视觉版

[![Framework: ESP-IDF](https://img.shields.io/badge/Framework-ESP--IDF-E7352C.svg?style=flat-square&logo=espressif)](https://docs.espressif.com/projects/esp-idf/en/latest/)
[![PlatformIO](https://img.shields.io/badge/PlatformIO-Compatible-orange.svg?style=flat-square&logo=platformio)](https://platformio.org/)
[![Hardware: ESP32-C3](https://img.shields.io/badge/Hardware-ESP32--C3-blue.svg?style=flat-square&logo=espressif)](https://www.espressif.com/en/products/socs/esp32-c3)
[![Display: ST7789 IPS](https://img.shields.io/badge/Display-ST7789%20240x240%20IPS-green.svg?style=flat-square)]()
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg?style=flat-square)](LICENSE)
[![3D Case: MakerWorld](https://img.shields.io/badge/3D%20Print-MakerWorld-informational.svg?style=flat-square&logo=bambulab)](https://makerworld.com/en/models/2559505-clawd-mochi-physical-claude-code-mascot#profileId-2820000)

**Clawd Mochi PRO** 是专为 **ESP32-C3 + ST7789 1.54 寸 240×240 IPS 彩屏** 打造的高帧率赛博桌面动态特效展示台。

当前主程序为 **纯视觉（Pure Visual）独立固件**：开机即进入 39 款实时数学渲染特效，采用 Fisher-Yates 洗牌播放列表无重复轮播，全程无弹窗、无 WiFi 依赖、无需连接手机，插电即用。所有画面由 RISC-V 单核在 240×240 全屏 RGB565 显存上实时计算，并通过 40 MHz 硬件 SPI + DMA 分块推屏。

> ⚠️ **声明**：本项目为独立开源极客项目，灵感源自 Anthropic 旗下 Claude Code 的可爱吉祥物 Clawd 蟹。“Claude” 与 “Clawd” 为 Anthropic 的注册商标。

---

<p align="center">
  <img src="pics/clawd_mochi_3_4.jpeg" alt="Assembled Clawd Mochi on a desk" width="620"/>
</p>

---

## 🌟 核心特性 (Features)

- 🚀 **ESP-IDF 原生极速驱动**：40 MHz 硬件 SPI + DMA 分块流式推屏（每块 60 行），115.2 KB 全屏 RGB565 帧缓冲，配合 RISC-V 定点数 / 查表快速数学库，尽可能压榨 ESP32-C3 的渲染性能。
- 🎨 **39 款全屏赛博数学动态特效**：涵盖 3D 甜甜圈、Synthwave 落日、数字雨、Doom 火焰、黑洞吸积盘、DNA 双螺旋、4D 超环面、托卡马克反应堆、莫比乌斯环、蜂巢波等硬核画面。
- 🔀 **无重复洗牌轮播**：Fisher-Yates 洗牌播放列表，保证一整轮内 39 款特效全部均匀出现且不连续重复；每轮结束后重新洗牌，并避免与上一轮末尾撞车。
- 🖥️ **纯视觉零弹窗**：不绘制 OSD 徽标、不弹出闹钟 / 倒计时界面，画面始终全屏；当前特效与帧率仅通过串口日志输出。
- 🕹️ **单键全功能交互**：板载 BOOT 按键支持单击切换、快速双击旋转屏幕、长按锁定 / 恢复自动轮播，配蜂鸣器轻提示音。
- 🔌 **完全离线独立运行**：不启动 WiFi、不依赖手机或电脑，上电即自动播放，适合作为工位桌面摆件长期运行。
- 📈 **串口性能遥测**：每 2 秒输出当前特效 ID、FPS、渲染耗时与推屏耗时，方便针对单款特效继续优化。
- 🖨️ **完美兼容原版 3D 打印外壳**：适配原版 Clawd Mochi 蟹型外壳，小巧玲珑，工位吸睛神器。

---

## 🎮 按键操作 (BOOT · GPIO 9)

| 操作 | 触发条件 | 功能 | 提示音 |
| :--- | :--- | :--- | :--- |
| **单击** | 按下 40 ms ~ 800 ms | 按洗牌序列切换到下一款特效 | 30 ms 轻滴 |
| **快速双击** | 两次单击间隔 < 400 ms | 屏幕旋转 90°（0° → 90° → 180° → 270° 循环） | 30 ms 轻滴 |
| **长按** | 按住 ≥ 800 ms | 锁定当前特效 / 恢复 12 秒自动轮播 | 50 ms 轻滴 |
| **开机** | 上电启动 | 固定从经典 3D 甜甜圈开始，随后进入洗牌序列 | 25 ms 轻滴 |

- 自动轮播默认开启，每 **12 秒**切换下一款特效。
- 默认屏幕方向为 Rotation 1（适配 Clawd Mochi 外壳安装方向）。
- 蜂鸣器为可选件，仅用于开机与按键反馈；不接也不影响播放。

---

## 🎨 39 款高帧率动态特效全览

### 一、经典 17 款

| 编号 | 特效名称 | 核心算法与视觉呈现 |
| :---: | :--- | :--- |
| **01** | **3D 全息旋转甜甜圈 (`fx_donut.c`)** | 三维环面流光经纬网格与光子轨道，双轴自旋投影。 |
| **02** | **赛博朋克霓虹落日 (`fx_synthwave.c`)** | 3D 动态合成波山脉地形曲面网格，搭配条纹落日与透视地平线。 |
| **03** | **黑客帝国数字流光雨 (`fx_matrix.c`)** | 全屏数字雨，头部高亮引路、尾迹衰减渐暗、字符随机突变。 |
| **04** | **迷幻复古彩虹等离子电浆 (`fx_plasma.c`)** | 多频正余弦干涉流场，平滑律动的彩虹色带。 |
| **05** | **赛博 3D 声波山脉曲面 (`fx_spectrum.c`)** | 全息音频频谱多层流光声波山脊网格。 |
| **06** | **引力透镜黑洞 (`fx_blackhole.c`)** | 爱因斯坦引力透镜时空网格弯曲与炽热旋转吸积盘。 |
| **07** | **3D 银河旋涡引力臂 (`fx_galaxy.c`)** | 对数螺线引力密度波、星系盘粒子潮与核心高能星团。 |
| **08** | **经典毁灭战士火焰 (`fx_fire.c`)** | 经典热量流体火焰扩散算法，多阶火焰调色板渐变。 |
| **09** | **赛博扭曲虫洞网格 (`fx_tunnel.c`)** | 非线性时空收拢螺旋经纬管道，高速纵深跃迁。 |
| **10** | **超空间星空与立方体 (`fx_stars.c`)** | 3D 深空粒子流光拖尾，中心自转线框立方体。 |
| **11** | **3D 全息立体双螺旋 DNA (`fx_dna.c`)** | 连续空间编织骨架带与碱基对光桥，景深透视自旋。 |
| **12** | **霓虹流体熔岩融球 (`fx_metaballs.c`)** | 实时势能场叠加融球，互相吸引、融合与分离的液态质感。 |
| **13** | **赛博全息战术雷达 (`fx_radar.c`)** | 三维透视穹顶网格、扫描光束与多目标高度态势。 |
| **14** | **赛博激光李萨如示波器 (`fx_oscilloscope.c`)** | 三维多谐波共振立体空间流光带。 |
| **15** | **康威元胞自动机生命游戏 (`fx_gameoflife.c`)** | 2D 经典元胞自动机，量子生死演化与代际色彩繁衍。 |
| **16** | **极地深空极光 (`fx_aurora.c`)** | 多波段欧若拉极光幕帘，搭配雪山与湖面倒影。 |
| **17** | **电光朱利亚分形云 (`fx_fractal.c`)** | Julia Set 复变动力学全动态分形，实时呼吸缩放。 |

### 二、赛博空间与高维几何（18–29）

| 编号 | 特效名称 | 核心算法与视觉呈现 |
| :---: | :--- | :--- |
| **18** | **赛博方舟反应堆 (`fx_reactor.c`)** | 托卡马克聚变超导磁体与螺旋等离子流。 |
| **19** | **赛博六角隧道 (`fx_cyber_tunnel.c`)** | 高能几何光子跃迁通道，六角网格纵深收束。 |
| **20** | **赛博激光阵列 (`fx_laser_grid.c`)** | 多维空间李萨如全息高能激光波动曲面。 |
| **21** | **赛博故障裂缝 (`fx_glitch.c`)** | 数字信号撕裂、扫描线错位与 RGB 色差故障。 |
| **22** | **赛博强子对撞机 (`fx_collider.c`)** | 3D 真空管道超导磁约束与高能碰撞激波。 |
| **23** | **赛博特斯拉电弧 (`fx_lightning.c`)** | 高压分形放电分支网络与法拉第防护网格。 |
| **24** | **赛博 4D 超环面曲率 (`fx_tesseract.c`)** | 4D 克利福德高维双自旋嵌套曲面投影。 |
| **25** | **赛博全息罗盘 HUD (`fx_compass.c`)** | 三维姿态俯仰阶梯网格与航空电子罗盘。 |
| **26** | **赛博深空声纳 (`fx_sonar.c`)** | 极坐标凹面脉冲网格与隐匿目标回波探测。 |
| **27** | **赛博引力时空曲率 (`fx_gravity_grid.c`)** | 双黑洞相对论时空曲率漏斗与网格拉扯。 |
| **28** | **迷幻光影万花筒 (`fx_kaleidoscope.c`)** | 12 重轴对称曼陀罗激光编织几何。 |
| **29** | **赛博经纬三维球 (`fx_sphere.c`)** | 3D 高密度全息自旋经纬线框与赤道环。 |

### 三、硬核机械与量子系列（30–39）

| 编号 | 特效名称 | 核心算法与视觉呈现 |
| :---: | :--- | :--- |
| **30** | **赛博机械时钟齿轮 (`fx_clockwork.c`)** | 陀飞轮四行星齿轮差速动力笼与精密刻度。 |
| **31** | **赛博莫比乌斯环 (`fx_mobius.c`)** | 3D 单侧拓扑曲面高密度流光网格连续翻转。 |
| **32** | **赛博 3D 矩阵代码瀑布 (`fx_matrix_3d.c`)** | 全景深数字流时空网格高速冲屏。 |
| **33** | **赛博脉冲星射流 (`fx_pulsar.c`)** | 高速自转中子星磁层力线与相对论喷流。 |
| **34** | **赛博量子波函数干涉 (`fx_quantum.c`)** | 多相位相干几率波高密度曲率网格。 |
| **35** | **赛博光轨穿梭 (`fx_lightspeed.c`)** | 全息 Tron 电子网格高速光子流光墙。 |
| **36** | **赛博二十面体水晶 (`fx_icosahedron.c`)** | 双重嵌套正多面体空间自旋复合晶体网格。 |
| **37** | **赛博环形音频频谱仪 (`fx_circular_spectrum.c`)** | 360° 声波共振蜘蛛网曲面与辐射频谱柱。 |
| **38** | **赛博蜂巢矩阵波 (`fx_hex_wave.c`)** | 六角蜂窝弹性张力曲面动力场。 |
| **39** | **赛博核心解密锁环 (`fx_cipher_lock.c`)** | 星盘级四重同心差速密码盘对齐解锁。 |

---

## 🔌 硬件接线表 (Pinout Matrix)

推荐硬件：**AirM2M CORE ESP32-C3** 或 **ESP32-C3 Super Mini** + **ST7789 1.54 寸 240×240 IPS 屏幕（SPI 接口，7/8 Pin）**。

| 屏幕引脚 (ST7789) | ESP32-C3 引脚 | 说明 |
| :--- | :--- | :--- |
| **SDA / MOSI** | **GPIO 10** | 硬件 SPI 高速数据线 |
| **SCL / SCK** | **GPIO 8** | 硬件 SPI 时钟线（40 MHz 高速通信） |
| **RES / RST** | **GPIO 2** | 硬件复位引脚 |
| **DC** | **GPIO 1** | 数据 / 命令切换引脚 |
| **CS** | **GPIO 4** | SPI 片选引脚 |
| **BLK / BL** | **GPIO 3** | 背光控制引脚（高电平点亮） |
| **VCC** | **3.3V** | ⚠️ **请务必连接 3.3V，切勿接 5V** |
| **GND** | **GND** | 电源共地 |
| **按键 (BOOT)** | **GPIO 9** | 板载 BOOT 按键（单击切换 / 双击旋转 / 长按锁定） |
| **蜂鸣器 (可选)** | **GPIO 6** | 低电平触发蜂鸣器，仅作开机与按键反馈 |

---

## 🖨️ 3D 打印外壳与硬件清单 (BOM)

### 3D 打印模型
仓库 `models/` 目录中已包含全套 3D 打印 STL 模型文件：
- `models/clawd_mochi/`：原版经典 Clawd Mochi 壳体与支架
- `models/clawd_3d/`：优化版 3D 打印壳体
- `models/clawd_3d_squished_eyes/`：挤压大眼趣味版

也可以在 MakerWorld 上一键发送至拓竹 Bambu 打印机：
👉 [MakerWorld 3D 模型直达链接](https://makerworld.com/en/models/2559505-clawd-mochi-physical-claude-code-mascot#profileId-2820000)

### 硬件清单与成本估算
| 配件 | 规格建议 | 参考价格 |
| :--- | :--- | :---: |
| ESP32-C3 核心板 | Super Mini 或 合宙 AirM2M CORE ESP32-C3 | ~¥12 ($1.8) |
| 1.54 寸 IPS 屏幕 | ST7789 驱动，240×240 分辨率，SPI 接口 | ~¥15 ($2.2) |
| 蜂鸣器模块 | 低电平触发有源蜂鸣器（可选） | ~¥2 ($0.3) |
| 杜邦线 / 飞线 | 8~10 cm 细硅胶线 | ~¥2 ($0.3) |
| 3D 打印耗材 | PLA / PETG 约 30g | ~¥3 ($0.5) |
| M2 螺丝 | M2×6mm 螺丝 2 颗（固定屏幕） | ~¥1 ($0.1) |
| **总计** | | **~¥35 ($5~7)** |

---

## 🚀 编译与烧录指南 (Build & Flash)

本工程基于标准的 **PlatformIO + ESP-IDF** 开发环境：

### 方式一：VS Code + PlatformIO 插件（推荐）
1. 在 VS Code 中安装 **PlatformIO IDE** 插件。
2. 点击 VS Code 菜单：`文件 (File) -> 打开文件夹 (Open Folder)`，选择本项目根目录。
3. 状态栏底部点击 **Build (编译)** 按钮（或快捷键 `Ctrl+Alt+B`）。
4. 将 ESP32-C3 通过 USB 数据线接入电脑，点击 **Upload (烧录)** 按钮即可一键烧录！

### 方式二：命令行编译与烧录
```bash
# 1. 克隆或进入项目根目录
git clone https://github.com/xukuncai-hub/clawd-mochi.git
cd clawd-mochi

# 2. 编译固件
pio run

# 3. 连接开发板后一键上传
pio run -t upload

# 4. 开启串口调试监视器 (115200 波特率，可查看 FPS / 渲染耗时)
pio device monitor
```

> 提示：仓库内 `sdkconfig.airm2m_core_esp32c3` 按 **2 MB Flash** 配置。若使用 4 MB 模组，构建时出现 Flash 容量提示属正常现象，可在 `menuconfig` 中按实际模组调整。

---

## 📂 仓库目录结构

```text
clawd-mochi/
├── CMakeLists.txt                 # ESP-IDF 顶级构建配置
├── platformio.ini                 # PlatformIO 工程环境配置文件
├── sdkconfig.airm2m_core_esp32c3  # ESP32-C3 核心板配置
├── sdkconfig.defaults             # 自定义默认配置（主任务栈等）
├── src/                           # 固件核心源码
│   ├── main.c                     # 主循环、按键交互与洗牌轮播调度
│   ├── display.c / display.h      # ST7789 DMA SPI 驱动、旋转与图形基元
│   ├── effects.h                  # 39 款动态特效接口定义
│   ├── fx_*.c                     # 39 个独立特效实现文件
│   ├── fast_math.c / fast_math.h  # 快速正余弦定点数数学库
│   ├── font.c / font.h            # 极客风格等宽点阵字体库
│   ├── web_assets.h               # 内嵌 Web 控制台页面资源（实验模块）
│   ├── wifi_app.c / wifi_app.h    # WiFi AP + Web 服务器（当前固件未启动）
│   ├── alarm_timer.c / alarm_timer.h # 闹钟与倒计时逻辑（当前固件未启动）
│   ├── game_plane.c / game_plane.h   # 街机飞机大战（当前固件未接入）
│   └── CMakeLists.txt             # 源码组件注册
├── models/                        # 3D 打印外壳模型 STL 文件
├── pics/                          # 项目实物图片与展示素材
├── legacy/                        # 历史归档
│   └── arduino/                   # 原版 Arduino 历史固件备份
├── LICENSE                        # MIT 开源协议
└── README.md                      # 项目说明文档
```

> 说明：仓库历史版本曾包含 WiFi 网页控制台、智能闹钟与飞机大战功能，相关源码仍保留在 `src/` 中；当前主程序为纯视觉固件，不会初始化这些模块，可自行按需接线启用。

---

## 📄 开源协议 (License)

本项目采用 [MIT License](LICENSE) 开源协议，欢迎自由 Fork、修改、制作以及二次创作！
