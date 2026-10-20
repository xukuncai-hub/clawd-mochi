<!-- LOGO -->
<p align="center">
  <img src="pics/clawd_mochi_banner.png" alt="Clawd Mochi Logo" width="700"/>
</p>

# Clawd Mochi PRO 🦀⚡

[![Framework: ESP-IDF](https://img.shields.io/badge/Framework-ESP--IDF-E7352C.svg?style=flat-square&logo=espressif)](https://docs.espressif.com/projects/esp-idf/en/latest/)
[![PlatformIO](https://img.shields.io/badge/PlatformIO-Compatible-orange.svg?style=flat-square&logo=platformio)](https://platformio.org/)
[![Hardware: ESP32-C3](https://img.shields.io/badge/Hardware-ESP32--C3-blue.svg?style=flat-square)](https://www.espressif.com/en/products/socs/esp32-c3)
[![Display: ST7789 IPS](https://img.shields.io/badge/Display-ST7789%20240x240%20IPS-green.svg?style=flat-square)]()
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg?style=flat-square)](LICENSE)
[![3D Case: MakerWorld](https://img.shields.io/badge/3D%20Print-MakerWorld-informational.svg?style=flat-square&logo=bambulab)](https://makerworld.com/en/models/2559505-clawd-mochi-physical-claude-code-mascot#profileId-2820000)

**Clawd Mochi PRO** 是专为 **ESP32-C3 + ST7789 1.54寸 240×240 高清 IPS 彩屏** 打造的高性能赛博极客动态特效展示台与智能桌面伴侣。

本项目使用 **ESP-IDF 原生驱动与高性能实时数学渲染引擎**，重构了原版低帧率静态表情包方案，升级为 **17 款极致丝滑的数学动态特效**，并内置 **独立 WiFi AP 网页控制器、一键时钟同步、智能定时闹钟与专注番茄钟**，兼顾高颜值极客桌面美学与实用工位工具属性！

> ⚠️ **声明**：本项目为独立开源极客项目，灵感源自 Anthropic 旗下 Claude Code 的可爱吉祥物 Clawd 蟹。“Claude” 与 “Clawd” 为 Anthropic 的注册商标。

---

<p align="center">
  <img src="pics/clawd_mochi_3_4.jpeg" alt="Assembled Clawd Mochi on a desk" width="460"/>
  &nbsp;
  <img src="pics/clawd_mochi_claude_code.jpeg" alt="Claude Code view" width="460"/>
</p>

---

## 🌟 核心特性 (Features)

- 🚀 **ESP-IDF 原生极速驱动**：采用硬件高速 SPI（40MHz）双缓冲 DMA 异步推屏，算法针对 RISC-V 架构进行定点数/快速正余弦数学优化，保持满帧极致流畅。
- 🎨 **17 款豪华精选赛博数学动态特效**：涵盖 3D 光影甜甜圈、黑客帝国代码雨、Doom 流体火焰、复古 Synthwave 网格落日、引力透镜黑洞等。
- 📱 **独立免 App 手机 Web 控制台**：ESP32 自动发射独立 WiFi AP（`ClawdMochi-PRO`，密码 `clawd1234`，无需外网），手机连接即刻进入极客暗黑风控制面板与街机飞机大战手柄。
- 🕒 **一键手机时间秒同步**：进入网页瞬间自动读取手机高精度时间并同步至 ESP32 硬件 RTC。
- ⏰ **智能定时闹钟 & 专注倒计时（番茄钟）**：支持网页自由设定闹铃时间与专注倒计时（1/3/5/10/25分钟及自定义），到期触发蜂鸣器警报并在屏幕弹出醒目倒计时提示。
- 🕹️ **双重交互（按键 + Web）**：可通过板载 BOOT 按键或手机网页随时切换特效、锁定轮播或一键解除闹铃。
- 🖨️ **完美兼容原版 3D 打印外壳**：完全适配原版 Clawd Mochi 蟹型 3D 打印外壳，小巧玲珑，工位吸睛神器。

---

## 🎨 17 大高帧率动态特效全览

| 编号 | 特效名称 | 核心算法与视觉呈现 |
| :---: | :--- | :--- |
| **01** | **3D 光影旋转甜甜圈 (`fx_donut.c`)** | 3D 环面三维空间双轴旋转投影渲染，实时法向量光照着色与 16-bit 赛博彩虹渐变。 |
| **02** | **赛博朋克 80s 霓虹落日与透视网格 (`fx_synthwave.c`)** | 经典 Synthwave 复古未来主义，透视地面线条无限滚动推进，搭配百叶窗条纹渐变巨日与地平线辉光。 |
| **03** | **黑客帝国数字流光雨 (`fx_matrix.c`)** | 30 列全屏动态字符流光飞瀑，头部炽白荧光流光引路，尾迹深绿衰减渐暗，字符实时突变扰动。 |
| **04** | **迷幻复古彩虹等离子电浆 (`fx_plasma.c`)** | 经典老派 Demoscene 正余弦复合波动，多频色彩干涉流体平滑律动。 |
| **05** | **赛博霓虹音频频谱瀑布与镜面水波 (`fx_spectrum.c`)** | 20 段动感音频频谱跳动柱形图，带有峰值衰减悬浮点与底部真实水面倒影波纹。 |
| **06** | **引力透镜黑洞与相对论吸积盘 (`fx_blackhole.c`)** | 相对论事件视界光线弯曲、核心绝对黑暗奇异点与极速旋转的多层炽热光子吸积盘。 |
| **07** | **3D 旋涡银河与星云粒子潮 (`fx_galaxy.c`)** | 双旋臂对数螺旋星系旋转演化，核心高能稠密星团与边缘漫游星尘粒子流。 |
| **08** | **经典毁灭战士地狱火焰 (`fx_fire.c`)** | 经典 PSX DOOM 向上热对流粒子扩散算法，37 阶炭黑 $\to$ 炽红 $\to$ 烈焰金 $\to$ 炽白火焰调色板。 |
| **09** | **时空跃迁虫洞光速飞驰隧道 (`fx_tunnel.c`)** | 光速曲率飞驰的几何纹理隧道，多层高频缩放与空间极坐标深度扭曲。 |
| **10** | **超空间星空与 3D 旋转超立方体 (`fx_stars.c`)** | 3D 深空粒子流光加速穿越（带运动模糊速度拖尾），屏幕中心自转的 3D 线框超立方体与发光八面体核心。 |
| **11** | **3D 炫彩旋转 DNA 双螺旋粒子链 (`fx_dna.c`)** | 3D 空间自转的双螺旋碱基对微粒链，带近大远小景深透视与彩虹色彩渐变。 |
| **12** | **霓虹流体熔岩双态融球 (`fx_metaballs.c`)** | 实时势能场叠加融球流体动画，互相吸引、融合与分离的液态水银球质感。 |
| **13** | **赛博全息战术雷达 HUD (`fx_radar.c`)** | 360 度动态扫描光束、极坐标同心环刻度、随机脉冲闪烁目标点与战术十字瞄准线。 |
| **14** | **赛博激光李萨如谐波示波器 (`fx_oscilloscope.c`)** | 复合多频简谐振动李萨如图形，霓虹辉光轨迹实时三维旋转扭折。 |
| **15** | **康威量子元胞自动机生命游戏 (`fx_gameoflife.c`)** | 2D 经典元胞自动机，量子生死演化规则，生命代际色彩繁衍与自愈机制。 |
| **16** | **极地深空极光与雪山倒影 (`fx_aurora.c`)** | 柔和多波段波动的夜空欧若拉极光幕帘，搭配下方静谧山峦与湖面倒影。 |
| **17** | **电光朱利亚全动态复变分形云 (`fx_fractal.c`)** | 经典 Julia Set 复变动力学全动态分形迭代，实时呼吸缩放与色彩周期流转。 |
| **18** | **街机太空飞机大战 (`game_plane.c`)** | 经典复古 8-Bit 像素弹幕空战！支持手机无线手柄低延迟触控（D-Pad 走位、按住连发、火力升级、吸收护盾、战机血量、重型 Boss 战），无人操作时自动切入 AI 巡航演示（Attract Mode）。 |

---

## 🕹️ 手机端无线手柄与飞机大战玩法

只需在手机连接 WiFi（`ClawdMochi`）打开网页，点击 **「🕹️ 街机飞机大战 · 展开手柄」**：
- **手机秒变无线手柄**：
  - **左侧 D-Pad 十字方向键**：原生 `touchstart` 零延迟触控响应，按住持续平滑走位，松手刹车。
  - **右侧 FIRE 按钮**：按住自动高频连发激光脉冲！电脑端同时支持键盘 `WASD` / 方向键走位与空格键开火。
- **丰富的游戏机制**：
  - **道具掉落**：击毁敌机概率掉落 **[P] 火力强化**（单发 $\to$ 双发 $\to$ 三路等离子散弹）、**[S] 能量护盾**（抵消一次致命伤害）、**[H] 生命补给**。
  - **Boss 旗舰战**：每击杀 15 架敌机触发警报并降临重装巡洋舰 Boss，顶部显示专用血条，挑战你的极限走位！
  - **非阻塞 8-Bit 街机音效**：板载蜂鸣器硬件级微秒脉冲，射击（Pew Pew）、爆炸（Boom）、拾取与死亡各具专属音效。
  - **机身 BOOT 按键联动**：游戏中短按 BOOT 键开火（死亡时一键复活），长按 1.2 秒一键退出游戏切回特效展示。

---

## 📱 手机 Web 控制台与操作交互

### 1. 连接控制台
1. 开机后，ESP32-C3 自动开启 WiFi 局域网热点：
   - **WiFi SSID**：`ClawdMochi`
   - **WiFi 密码**：无（开放热点，开箱即连）
2. 手机连接该 WiFi 后，在浏览器中打开：
   👉 **`http://192.168.4.1`**
3. 即可进入内嵌的赛博暗黑风 Web 客户端：
   - **街机手柄面板**：一键呼出触摸十字键与连发开火按钮，实时同步战机生命与分数。
   - **时钟面板**：一键同步当前手机高精度时分秒到 ESP32。
   - **闹钟设置**：开启/关闭定时闹钟，设置小时/分钟。
   - **倒计时定时器**：快捷选择 1m、3m、5m、10m、25m（番茄工作法）或自定义时长。
   - **特效网格面板**：点击对应特效按钮立即切换，支持一键切换 12s 自动轮播、旋转屏幕 180°。
   - **响铃消除**：响铃中一键关闭声光警报。

### 2. 机身物理按键交互 (BOOT 键 · GPIO 9)
- **常规模式**：
  - 短按：立即切换到下一个特效，伴随清脆滴声提示；若正处于闹钟/倒计时响铃状态，短按立即解除响铃。
  - 长按（> 1.2 秒）：锁定当前特效 / 恢复自动轮播，伴随长滴声提示。
- **游戏模式中**：
  - 短按：发射激光子弹（Game Over 时短按一键重新开始）。
  - 长按（> 1.2 秒）：退出游戏模式，切回常规动态特效。
- **切换 OSD 徽标**：每次切换特效时，屏幕上方浮现半透明赛博 HUD 徽标，展示当前特效编号与中英文名（持续 2.5 秒后自动淡出）。

---

## 🔌 硬件接线表 (Pinout Matrix)

推荐硬件：**AirM2M CORE ESP32-C3** 或 **ESP32-C3 Super Mini** + **ST7789 1.54寸 240×240 IPS 屏幕（SPI 接口，7/8 Pin）**。

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
| **按键 (BOOT)** | **GPIO 9** | 板载 BOOT 按键（短按切换，长按锁定） |
| **蜂鸣器 (可选)** | **GPIO 6** | 低电平触发蜂鸣器 / 有源蜂鸣器 |

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
| 1.54寸 IPS 屏幕 | ST7789 驱动，240×240 分辨率，SPI 接口 | ~¥15 ($2.2) |
| 蜂鸣器模块 | 低电平触发有源蜂鸣器（或无源） | ~¥2 ($0.3) |
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

# 4. 开启串口调试监视器 (115200 波特率)
pio device monitor
```

---

## 📂 仓库目录结构

```text
clawd-mochi/
├── CMakeLists.txt                 # ESP-IDF 顶级构建配置
├── platformio.ini                 # PlatformIO 工程环境配置文件
├── sdkconfig.airm2m_core_esp32c3  # ESP32-C3 核心板配置
├── src/                           # 固件核心源码
│   ├── main.c                     # 主程序入口、按键轮询与特效调度
│   ├── display.c / display.h      # ST7789 高性能 DMA SPI 驱动与图形基元
│   ├── wifi_app.c / wifi_app.h    # 独立 WiFi AP Web 服务器与控制台网页
│   ├── alarm_timer.c / alarm_timer.h # 智能闹钟与番茄倒计时逻辑
│   ├── effects.h                  # 17 款动态特效接口定义
│   ├── fx_*.c                     # 17 个独立特效实现文件
│   ├── fast_math.c / fast_math.h  # 快速正余弦定点数数学库
│   ├── font.c / font.h            # 极客风格等宽点阵字体库
│   └── CMakeLists.txt             # 源码组件注册
├── models/                        # 3D 打印外壳模型 STL 文件
├── pics/                          # 项目实物图片与展示素材
├── legacy/                        # 历史归档
│   └── arduino/                   # 原版 Arduino 历史固件备份
├── LICENSE                        # MIT 开源协议
└── README.md                      # 项目说明文档
```

---

## 📄 开源协议 (License)

本项目采用 [MIT License](LICENSE) 开源协议，欢迎自由 Fork、修改、制作以及二次创作！
