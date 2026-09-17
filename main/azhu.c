#include "azhu.h"

// 素材包布局（小端，与 tools/build_assets.py 一致）：
//   off 0  : magic 'AZHU'          (4B)
//   off 4  : version               (1B)
//   off 5  : n_actions             (1B)
//   off 6  : frames_per_action     (1B)
//   off 7  : width                 (2B)
//   off 9  : height                (2B)
//   off 11 : palette_count         (2B)
//   off 13 : palette[pal_count]    (2B each, RGB565 小端)
//   then   : action_table[n]       每项 14B = name(12B) + start_frame(2B)
//   then   : frame_data            连续 (n*frames_per) 帧，每帧 W*H 字节

static const uint8_t *B;
static uint16_t s_pal[256];
static uint8_t  s_na, s_fpa;
static uint16_t s_w, s_h, s_palc;
static const uint8_t *s_table;
static const uint8_t *s_frames;

static inline uint16_t rd16(const uint8_t *p) { return (uint16_t)(p[0] | (p[1] << 8)); }

void azhu_init(void) {
    B = azhu_assets_bin_start;
    s_na   = B[5];
    s_fpa  = B[6];
    s_w    = rd16(B + 7);
    s_h    = rd16(B + 9);
    s_palc = rd16(B + 11);

    const uint8_t *pal = B + 13;
    for (int i = 0; i < 256; i++) {
        s_pal[i] = (i < s_palc) ? rd16(pal + i * 2) : 0;
    }
    s_table  = B + 13 + (size_t)s_palc * 2;
    s_frames = s_table + (size_t)s_na * 14;
}

int azhu_num_actions(void) { return s_na; }
int azhu_frames_per(void)  { return s_fpa; }
int azhu_w(void)           { return s_w; }
int azhu_h(void)           { return s_h; }
const uint16_t *azhu_palette(void) { return s_pal; }
const char *azhu_action_name(int action) { return (const char *)(s_table + (size_t)action * 14); }

const uint8_t *azhu_frame(int action, int f) {
    uint16_t start_frame = rd16(s_table + (size_t)action * 14 + 12);
    size_t gi = (size_t)start_frame + f;
    return s_frames + gi * (size_t)s_w * s_h;
}
