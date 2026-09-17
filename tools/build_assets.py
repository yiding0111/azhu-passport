# -*- coding: utf-8 -*-
"""把 assets/actions/*.png（阿猪 16 个 2x2 四格动作图）打包成 main/azhu_assets.bin。

输出格式：全屏 240x320 · 8bpp 索引 · 全局 256 色 RGB565 调色板。
改素材、换画风或加动作后，重跑本脚本再提交 main/azhu_assets.bin 即可。

用法：  python tools/build_assets.py
依赖：  pip install pillow
"""
import os
import struct
from PIL import Image, ImageDraw

HERE = os.path.dirname(os.path.abspath(__file__))
BASE = os.path.join(HERE, "..", "assets", "actions")
OUT  = os.path.join(HERE, "..", "main", "azhu_assets.bin")

W, H = 240, 320
BODY = 200                 # 身体最大边（居中偏下站在屏上）
BG   = (253, 246, 243)     # 暖白背景（也是身体抗锯齿边缘预合成色，衔接无缝）
GY   = H - 40              # 脚底基线

# 动作顺序：第 0 个是开机默认；"sleep" 会被主循环用作无操作待机
ACTIONS = ["wave", "think", "sleep", "eat", "drink", "laugh", "celebrate", "dance",
           "run", "shy", "sad", "angry", "hot", "sneeze", "fart", "bbq"]


def frame_rgb(cell):
    bb = cell.getbbox()
    if bb:
        cell = cell.crop(bb)
    cw, ch = cell.size
    s = BODY / max(cw, ch)
    nw, nh = max(1, int(cw * s)), max(1, int(ch * s))
    cell = cell.resize((nw, nh), Image.LANCZOS)
    canvas = Image.new("RGB", (W, H), BG)
    d = ImageDraw.Draw(canvas)
    cx = W // 2
    d.ellipse([cx - 72, GY - 12, cx + 72, GY + 12], fill=(235, 224, 219))  # 脚下阴影
    px = (W - nw) // 2
    py = GY - nh + 18
    canvas.paste(cell, (px, py), cell)
    return canvas


def rgb565(r, g, b):
    return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)


def main():
    frames = []
    for name in ACTIONS:
        im = Image.open(os.path.join(BASE, name + ".png")).convert("RGBA")
        Wd, Hd = im.size
        hw, hh = Wd // 2, Hd // 2
        for b in [(0, 0, hw, hh), (hw, 0, Wd, hh), (0, hh, hw, Hd), (hw, hh, Wd, Hd)]:
            frames.append(frame_rgb(im.crop(b)))
    nf = len(frames)

    # 所有帧共用一个全局 256 色调色板（省空间、切帧不闪）
    tall = Image.new("RGB", (W, H * nf))
    for i, f in enumerate(frames):
        tall.paste(f, (0, H * i))
    palimg = tall.quantize(colors=256, method=0, dither=0)
    pal = (palimg.getpalette() + [0] * 768)[:768]

    idx = [f.quantize(palette=palimg, dither=0).tobytes() for f in frames]

    fpa = 4
    na = len(ACTIONS)
    buf = bytearray()
    buf += struct.pack("<4sBBBHHH", b"AZHU", 1, na, fpa, W, H, 256)
    for i in range(256):
        buf += struct.pack("<H", rgb565(pal[i * 3], pal[i * 3 + 1], pal[i * 3 + 2]))
    for ai, name in enumerate(ACTIONS):
        buf += (name.encode("ascii")[:12] + bytes(12))[:12] + struct.pack("<H", ai * fpa)
    for fr in idx:
        buf += fr

    with open(OUT, "wb") as fh:
        fh.write(buf)
    print("wrote %s : %d actions, %d frames, %.2f MB"
          % (OUT, na, nf, len(buf) / 1024 / 1024))


if __name__ == "__main__":
    main()
