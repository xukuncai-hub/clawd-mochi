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

// 17 大豪华精选高帧率动态特效阵列
extern Effect fx_donut;         // 1. 3D 光影旋转甜甜圈
extern Effect fx_synthwave;     // 2. 赛博朋克 80s 霓虹落日与透视网格
extern Effect fx_matrix;        // 3. 黑客帝国数字流光雨
extern Effect fx_plasma;        // 4. 迷幻复古彩虹等离子电浆
extern Effect fx_spectrum;      // 5. 赛博霓虹音频频谱瀑布与镜面水波
extern Effect fx_blackhole;     // 6. 引力透镜黑洞与相对论吸积盘
extern Effect fx_galaxy;        // 7. 3D 银河旋涡与星云粒子潮
extern Effect fx_fire;          // 8. 经典毁灭战士火焰
extern Effect fx_tunnel;        // 9. 时空跃迁虫洞光速飞驰隧道
extern Effect fx_stars;         // 10. 超空间星空与 3D 旋转超立方体
extern Effect fx_dna;           // 11. 3D 炫彩旋转 DNA 双螺旋粒子链
extern Effect fx_metaballs;     // 12. 霓虹流体熔岩双态融球
extern Effect fx_radar;         // 13. 赛博全息战术雷达 HUD
extern Effect fx_oscilloscope;  // 14. 赛博激光李萨如谐波示波器
extern Effect fx_gameoflife;    // 15. 康威量子元胞自动机生命游戏
extern Effect fx_aurora;        // 16. 极地深空极光与雪山倒影
extern Effect fx_fractal;       // 17. 电光朱利亚全动态复变分形云
extern Effect fx_game_plane;    // 18. 街机太空飞机大战 (支持手机无线手柄)

#endif // EFFECTS_H
