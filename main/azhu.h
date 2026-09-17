#pragma once
#include <stdint.h>
#include <stddef.h>

// EMBED_FILES "azhu_assets.bin" 生成的符号
extern const uint8_t azhu_assets_bin_start[] asm("_binary_azhu_assets_bin_start");
extern const uint8_t azhu_assets_bin_end[]   asm("_binary_azhu_assets_bin_end");

// 解析嵌入的素材包。必须在使用其它接口前调用一次。
void azhu_init(void);

int  azhu_num_actions(void);
int  azhu_frames_per(void);
int  azhu_w(void);
int  azhu_h(void);

// 对齐好的 256 槽 RGB565 调色板（主机小端存储）
const uint16_t *azhu_palette(void);

// 动作名（最多12字节，可能不带结尾0，勿直接 printf %s，用固定长度）
const char *azhu_action_name(int action);

// 返回第 action 个动作第 f 帧的 W*H 8bpp 索引数据指针（指向 flash 映射区）
const uint8_t *azhu_frame(int action, int f);
