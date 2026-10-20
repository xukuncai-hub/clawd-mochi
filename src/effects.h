#ifndef EFFECTS_H
#define EFFECTS_H

#include "display.h"

typedef struct {
    const char *id;
    const char *title;
    const char *subtitle;
    void (*init)(void);
    void (*update)(float dt);
    void (*render)(uint16_t *fb);
    void (*cleanup)(void);
} Effect;

// 39 款豪华赛博朋克极客纯视觉特效 (无头像、无烟花、全屏硬核科技动态)
// 1. 经典原版 17 大高帧率特效 (老样子)
extern Effect fx_donut;             // 1. 3D 光影旋转甜甜圈 (开机首屏)
extern Effect fx_synthwave;         // 2. 赛博朋克 80s 霓虹落日与透视网格
extern Effect fx_matrix;            // 3. 黑客帝国数字流光雨
extern Effect fx_plasma;            // 4. 迷幻复古彩虹等离子电浆
extern Effect fx_spectrum;          // 5. 赛博霓虹音频频谱瀑布与镜面水波
extern Effect fx_blackhole;         // 6. 引力透镜黑洞与相对论吸积盘
extern Effect fx_galaxy;            // 7. 3D 银河旋涡与星云粒子潮
extern Effect fx_fire;              // 8. 经典毁灭战士火焰
extern Effect fx_tunnel;            // 9. 时空跃迁虫洞光速飞驰隧道
extern Effect fx_stars;             // 10. 超空间星空与 3D 旋转超立方体
extern Effect fx_dna;               // 11. 3D 炫彩旋转 DNA 双螺旋粒子链
extern Effect fx_metaballs;         // 12. 霓虹流体熔岩双态融球
extern Effect fx_radar;             // 13. 赛博全息战术雷达 HUD
extern Effect fx_oscilloscope;      // 14. 赛博激光李萨如谐波示波器
extern Effect fx_gameoflife;        // 15. 康威量子元胞自动机生命游戏
extern Effect fx_aurora;            // 16. 极地深空极光与雪山倒影
extern Effect fx_fractal;           // 17. 电光朱利亚全动态复变分形云

// 2. 第一批赛博空间与高维几何特效 (12 款)
extern Effect fx_reactor;           // 18. 赛博方舟反应堆 (高能聚变等离子电磁线圈)
extern Effect fx_cyber_tunnel;      // 19. 赛博六角隧道 (高能几何光子跃迁通道)
extern Effect fx_laser_grid;        // 20. 赛博激光阵列 (多维空间全息扫描交错光束)
extern Effect fx_glitch;            // 21. 赛博故障裂缝 (数字信号撕裂与RGB色差故障)
extern Effect fx_collider;          // 22. 赛博强子对撞机 (双向高能粒子流环形对撞)
extern Effect fx_lightning;         // 23. 赛博特斯拉电弧 (高压分形放电能量分支网络)
extern Effect fx_tesseract;         // 24. 赛博四维超正方体 (4D高维超立方体双平面旋转投影)
extern Effect fx_compass;           // 25. 赛博全息罗盘 (战机HUD导航姿态仪与电子罗盘)
extern Effect fx_sonar;             // 26. 赛博深空声纳 (极坐标脉冲回波与隐匿目标探测)
extern Effect fx_gravity_grid;      // 27. 赛博引力时空曲率 (双黑洞相对论时空曲率漏斗拉扯)
extern Effect fx_kaleidoscope;      // 28. 迷幻光影万花筒 (8重对称曼陀罗绚烂几何)
extern Effect fx_sphere;            // 29. 赛博经纬三维球 (3D旋转线框经纬网格球)

// 3. 全新第二批硬核赛博特效 (10 款)
extern Effect fx_clockwork;         // 30. 赛博机械时钟齿轮 (行星齿轮差速啮合动力系统)
extern Effect fx_mobius;            // 31. 赛博莫比乌斯环 (3D单侧拓扑曲面空间连续翻转)
extern Effect fx_matrix_3d;         // 32. 赛博3D矩阵代码瀑布 (全景深数字流空间穿梭冲屏)
extern Effect fx_pulsar;            // 33. 赛博脉冲星射流 (高速自转中子星磁层与高能相对论喷流)
extern Effect fx_quantum;           // 34. 赛博量子波函数干涉 (多相位相干几率波干涉纹理云)
extern Effect fx_lightspeed;        // 35. 赛博光轨穿梭 (高速网格光速粒子折射流光墙)
extern Effect fx_icosahedron;       // 36. 赛博二十面体水晶 (黄金比例3D多面体空间自旋晶体)
extern Effect fx_circular_spectrum; // 37. 赛博环形音频频谱仪 (360度圆周重低音电音律动辐射柱)
extern Effect fx_hex_wave;          // 38. 赛博蜂巢矩阵波 (六角蜂窝弹性涟漪起伏动力场)
extern Effect fx_cipher_lock;       // 39. 赛博核心解密锁环 (三重同心差速密码盘对齐解锁)

#endif // EFFECTS_H
