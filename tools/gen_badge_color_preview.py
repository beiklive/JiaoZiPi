#!/usr/bin/env python3
"""从 src/utils/JiaoZiPiMachine.h 解析机种徽标颜色，生成配色预览图。

只依赖 Python 标准库（自写 PNG 编码器 + 5x7 点阵字体）。
用法：python3 tools/gen_badge_color_preview.py [输出路径]
默认输出：docs/badge_colors.png
"""

import os
import re
import struct
import sys
import zlib

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
HEADER = os.path.join(ROOT, "src", "utils", "JiaoZiPiMachine.h")
DEFAULT_OUT = os.path.join(ROOT, "docs", "badge_colors.png")
DEFAULT_HTML = os.path.join(ROOT, "docs", "badge_colors.html")

# 仅用于预览页显示的中文机种名（颜色与名称均以头文件为准）
ZH_NAMES = {
    "FC": "红白机", "SFC": "超级任天堂", "GB": "Game Boy", "GBC": "Game Boy Color",
    "GBA": "Game Boy Advance", "NDS": "Nintendo DS", "N3DS": "Nintendo 3DS",
    "NGC": "GameCube", "WII": "Wii", "MD": "Mega Drive", "SS": "世嘉土星",
    "DC": "Dreamcast", "PS1": "PlayStation", "PSP": "PlayStation Portable",
    "ARCADE": "街机",
}

# --- 5x7 点阵字体（仅包含本脚本用到的字符） --------------------------------
FONT = {
    " ": [".....", ".....", ".....", ".....", ".....", ".....", "....."],
    "#": [".#.#.", ".#.#.", "#####", ".#.#.", "#####", ".#.#.", ".#.#."],
    "0": [".###.", "#...#", "#..##", "#.#.#", "##..#", "#...#", ".###."],
    "1": ["..#..", ".##..", "..#..", "..#..", "..#..", "..#..", ".###."],
    "2": [".###.", "#...#", "....#", "...#.", "..#..", ".#...", "#####"],
    "3": ["#####", "...#.", "..#..", "...#.", "....#", "#...#", ".###."],
    "4": ["...#.", "..##.", ".#.#.", "#..#.", "#####", "...#.", "...#."],
    "5": ["#####", "#....", "####.", "....#", "....#", "#...#", ".###."],
    "6": ["..##.", ".#...", "#....", "####.", "#...#", "#...#", ".###."],
    "7": ["#####", "....#", "...#.", "..#..", ".#...", ".#...", ".#..."],
    "8": [".###.", "#...#", "#...#", ".###.", "#...#", "#...#", ".###."],
    "9": [".###.", "#...#", "#...#", ".####", "....#", "...#.", ".##.."],
    "A": [".###.", "#...#", "#...#", "#####", "#...#", "#...#", "#...#"],
    "B": ["####.", "#...#", "#...#", "####.", "#...#", "#...#", "####."],
    "C": [".###.", "#...#", "#....", "#....", "#....", "#...#", ".###."],
    "D": ["####.", "#...#", "#...#", "#...#", "#...#", "#...#", "####."],
    "E": ["#####", "#....", "#....", "####.", "#....", "#....", "#####"],
    "F": ["#####", "#....", "#....", "####.", "#....", "#....", "#...."],
    "G": [".###.", "#...#", "#....", "#..##", "#...#", "#...#", ".####"],
    "H": ["#...#", "#...#", "#...#", "#####", "#...#", "#...#", "#...#"],
    "I": ["#####", "..#..", "..#..", "..#..", "..#..", "..#..", "#####"],
    "L": ["#....", "#....", "#....", "#....", "#....", "#....", "#####"],
    "M": ["#...#", "##.##", "#.#.#", "#...#", "#...#", "#...#", "#...#"],
    "N": ["#...#", "##..#", "#.#.#", "#..##", "#...#", "#...#", "#...#"],
    "O": [".###.", "#...#", "#...#", "#...#", "#...#", "#...#", ".###."],
    "P": ["####.", "#...#", "#...#", "####.", "#....", "#....", "#...."],
    "R": ["####.", "#...#", "#...#", "####.", "#.#..", "#..#.", "#...#"],
    "S": [".####", "#....", "#....", ".###.", "....#", "....#", "####."],
    "W": ["#...#", "#...#", "#...#", "#.#.#", "#.#.#", "##.##", "#...#"],
}

CANVAS_BG = (0xFA, 0xF7, 0xF0)   # 奶油白基础底色
TITLE_COLOR = (0x2E, 0x2A, 0x26)
HEX_COLOR = (0x5A, 0x54, 0x4C)
KEY_COLOR = (0xA8, 0xA0, 0x94)
INK = (0x2B, 0x2B, 0x2B)
WHITE = (0xFF, 0xFF, 0xFF)


def luminance(color):
    """相对亮度 0~1000，与头文件 RelativeLuminance 保持一致。"""

    def ch(c):
        return c * c * 1000 // (255 * 255)

    return (299 * ch(color[0]) + 587 * ch(color[1]) + 114 * ch(color[2])) // 1000


def text_color_on(bg):
    """与头文件 MachineBadgeTextColor 保持一致：浅色底用深色字，深色底用白字。"""
    bg_l, ink_l, white_l = luminance(bg), luminance(INK), luminance(WHITE)
    return INK if (bg_l + 50) / (ink_l + 50) >= (white_l + 50) / (bg_l + 50) else WHITE


def text_width(text, scale):
    return len(text) * 6 * scale - scale


def draw_text(buf, w, h, x, y, text, color, scale=2):
    for ch in text.upper():
        glyph = FONT.get(ch, FONT[" "])
        for ry, row in enumerate(glyph):
            for rx, px in enumerate(row):
                if px != "#":
                    continue
                for dy in range(scale):
                    for dx in range(scale):
                        put_px(buf, w, h, x + rx * scale + dx, y + ry * scale + dy, color)
        x += 6 * scale


def put_px(buf, w, h, x, y, color):
    if 0 <= x < w and 0 <= y < h:
        off = (y * w + x) * 3
        buf[off:off + 3] = bytes(color)


def fill_round_rect(buf, w, h, x, y, rw, rh, color, radius=6, border=None):
    for yy in range(y, y + rh):
        for xx in range(x, x + rw):
            dx = min(xx - x, x + rw - 1 - xx)
            dy = min(yy - y, y + rh - 1 - yy)
            if dx < radius and dy < radius:
                if (radius - dx) ** 2 + (radius - dy) ** 2 > radius ** 2:
                    continue
            if border and (dx == 0 or dy == 0):
                put_px(buf, w, h, xx, yy, border)
            else:
                put_px(buf, w, h, xx, yy, color)


def write_png(path, w, h, buf):
    raw = b"".join(b"\x00" + buf[y * w * 3:(y + 1) * w * 3] for y in range(h))

    def chunk(tag, data):
        return (struct.pack(">I", len(data)) + tag + data +
                struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF))

    png = (b"\x89PNG\r\n\x1a\n" +
           chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0)) +
           chunk(b"IDAT", zlib.compress(raw, 9)) +
           chunk(b"IEND", b""))
    with open(path, "wb") as f:
        f.write(png)


def parse_header():
    src = open(HEADER, encoding="utf-8").read()

    def section(name):
        m = re.search(r"constexpr[^\n]*\b%s\([^)]*\)\s*\{(.*?)\n\}" % name, src, re.S)
        return m.group(1) if m else ""

    colors = dict(re.findall(r"case Machine::(\w+):\s*return Rgba\(0x([0-9A-Fa-f]{6})\)",
                             section("MachineBadgeColor")))
    labels = dict(re.findall(r'case Machine::(\w+):\s*return "([^"]*)"', section("MachineLabel")))
    names = dict(re.findall(r'case Machine::(\w+):\s*return "([^"]*)"', section("MachineName")))
    order = re.findall(r"case Machine::(\w+)\s*=", src) or list(colors)
    order = [k for k in order if k in colors and labels.get(k) not in (None, "?")]
    return order, colors, labels, names


def mix(hex_str, target, p):
    """hex_str 向 target(FF/00) 混合 p 比例，用于预览页的高光/描边示意。"""
    src = tuple(int(hex_str[i:i + 2], 16) for i in (0, 2, 4))
    tgt = 255 if target == "FF" else 0
    return "#%02X%02X%02X" % tuple(round(s + (tgt - s) * p) for s in src)


PAGE = """<!doctype html>
<html lang="zh-CN">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>JiaoZiPi 机种主题色规范</title>
<style>
  :root { --base: #FAF7F0; --ink: #2E2A26; --muted: #8C857A; --line: #E8E1D5; }
  * { box-sizing: border-box; }
  body { margin: 0; padding: 40px 32px 64px; background: var(--base); color: var(--ink);
         font: 14px/1.6 ui-monospace, SFMono-Regular, Menlo, Consolas, monospace; }
  h1 { margin: 0 0 6px; font-size: 22px; letter-spacing: .06em; }
  .sub { margin: 0 0 32px; color: var(--muted); font-size: 12px; }
  .grid { display: grid; gap: 18px; grid-template-columns: repeat(auto-fill, minmax(260px, 1fr)); }
  .card { border: 1px solid var(--line); border-radius: 14px; background: #FFFDF9; padding: 16px; }
  .head { display: flex; align-items: center; gap: 12px; }
  .badge { width: 84px; height: 44px; border-radius: 10px; background: var(--c); color: var(--on);
           display: flex; align-items: center; justify-content: center; font-weight: 700;
           font-size: 15px; letter-spacing: .08em; border: 1px solid var(--shade); }
  .meta { min-width: 0; }
  .name { font-weight: 700; }
  .en { color: var(--muted); font-size: 12px; }
  .hex { margin: 12px 0 14px; color: var(--shade); font-size: 13px; }
  .samples { display: flex; flex-wrap: wrap; gap: 8px; align-items: center; }
  .tag { border-radius: 999px; padding: 3px 12px; font-size: 12px;
         background: var(--tint); color: var(--shade); border: 1px solid transparent; }
  .tag.selected { border-color: var(--c); color: var(--ink); box-shadow: 0 0 0 3px var(--tint); }
  .swatch { width: 100%; height: 26px; border-radius: 6px; background: var(--c); margin-top: 12px; }
  footer { margin-top: 36px; color: var(--muted); font-size: 12px; }
  code { color: var(--ink); }
</style>
</head>
<body>
  <h1>JiaoZiPi 机种主题色规范</h1>
  <p class="sub">__COUNT__ 个机种 · 浅色低饱和印象色 · 由 tools/gen_badge_color_preview.py 生成自 src/utils/JiaoZiPiMachine.h</p>
  <div class="grid">
__CARDS__
  </div>
  <footer>白字徽标：__WHITE__ · 其余为深色字 __INK__（由 MachineBadgeTextColor() 按对比度自动选择）</footer>
</body>
</html>
"""

CARD = """    <article class="card" style="--c:{hex};--on:{on};--shade:{shade};--tint:{tint}">
      <div class="head">
        <div class="badge">{label}</div>
        <div class="meta">
          <div class="name">{zh}</div>
          <div class="en">{en}</div>
        </div>
      </div>
      <div class="hex">{hex}</div>
      <div class="samples">
        <span class="tag">游戏列表</span>
        <span class="tag selected">选中</span>
      </div>
      <div class="swatch"></div>
    </article>
"""


def emit_html(order, colors, labels, names, path):
    cards, white = [], []
    for key in order:
        hex_str = colors[key].upper()
        rgb = tuple(int(hex_str[i:i + 2], 16) for i in (0, 2, 4))
        on = "#2B2B2B" if text_color_on(rgb) == INK else "#FFFFFF"
        if on == "#FFFFFF":
            white.append(key)
        cards.append(CARD.format(hex="#" + hex_str, on=on, shade=mix(hex_str, "00", .35),
                                 tint=mix(hex_str, "FF", .78), label=labels[key],
                                 zh=ZH_NAMES.get(key, key),
                                 en=names.get(key, "")))
    html = (PAGE.replace("__CARDS__", "".join(cards))
                .replace("__COUNT__", str(len(order)))
                .replace("__WHITE__", "、".join(white))
                .replace("__INK__", "#2B2B2B"))
    with open(path, "w", encoding="utf-8") as f:
        f.write(html)


def main():
    out = sys.argv[1] if len(sys.argv) > 1 else DEFAULT_OUT
    order, colors, labels, names = parse_header()
    if not order:
        sys.exit("未从 %s 解析到机种颜色" % HEADER)

    scale = 3
    row_h = 64
    pad = 28
    badge_w, badge_h = 132, 40
    w = 560
    h = pad + 44 + len(order) * row_h + pad
    buf = bytearray(bytes(CANVAS_BG) * (w * h))

    draw_text(buf, w, h, pad, pad, "MACHINE BADGE COLORS", TITLE_COLOR, scale)

    y = pad + 44
    for key in order:
        rgb = tuple(int(colors[key][i:i + 2], 16) for i in (0, 2, 4))
        border = tuple(max(0, c * 3 // 4) for c in rgb)
        fill_round_rect(buf, w, h, pad, y, badge_w, badge_h, rgb, border=border)
        label = labels[key]
        tx = pad + (badge_w - text_width(label, 2)) // 2
        ty = y + (badge_h - 7 * 2) // 2
        draw_text(buf, w, h, tx, ty, label, text_color_on(rgb), 2)

        draw_text(buf, w, h, pad + badge_w + 24, y + 6, "#" + colors[key].upper(), HEX_COLOR, 2)
        draw_text(buf, w, h, pad + badge_w + 200, y + 6, key, KEY_COLOR, 2)
        y += row_h

    os.makedirs(os.path.dirname(out), exist_ok=True)
    write_png(out, w, h, buf)
    emit_html(order, colors, labels, names, DEFAULT_HTML)
    print("%s  (%d 机种, %dx%d)" % (out, len(order), w, h))
    print(DEFAULT_HTML)


if __name__ == "__main__":
    main()
