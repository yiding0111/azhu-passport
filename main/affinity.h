#pragma once

// 好感度 0..100，存 NVS，断电不丢。
void affinity_init(void);

void affinity_on_interact(void);   // 按键互动：+1，内部 30 秒冷却，防狂按刷分
void affinity_on_roam(void);       // 检测到一次溜达：+2
void affinity_tick(void);          // 主循环里调，处理"久不理会"的衰减

int  affinity_get(void);           // 0..100
int  affinity_level(void);         // 0=陌生(0~30) 1=熟悉(31~70) 2=亲密(71~100)
