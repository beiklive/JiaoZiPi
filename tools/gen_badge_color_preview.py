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

CANVAS_BG = (0x1E, 0x1E, 0x1E)
TITLE_COLOR = (0xF2, 0xF2, 0xF2)
HEX_COLOR = (0xE0, 0xE0, 0xE0)
KEY_COLOR = (0x8A, 0x8A, 0x8A)


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


def fill_round_rect(buf, w, h, x, y, rw, rh, color, radius=6):
    for yy in range(y, y + rh):
        for xx in range(x, x + rw):
            dx = min(xx - x, x + rw - 1 - xx)
            dy = min(yy - y, y + rh - 1 - yy)
            if dx < radius and dy < radius:
                if (radius - dx) ** 2 + (radius - dy) ** 2 > radius ** 2:
                    continue
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
    order = re.findall(r"case Machine::(\w+)\s*=", src) or list(colors)
    order = [k for k in order if k in colors and labels.get(k) not in (None, "?")]
    return order, colors, labels


def main():
    out = sys.argv[1] if len(sys.argv) > 1 else DEFAULT_OUT
    order, colors, labels = parse_header()
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
        r, g, b = (int(colors[key][i:i + 2], 16) for i in (0, 2, 4))
        fill_round_rect(buf, w, h, pad, y, badge_w, badge_h, (r, g, b))
        label = labels[key]
        tx = pad + (badge_w - text_width(label, 2)) // 2
        ty = y + (badge_h - 7 * 2) // 2
        draw_text(buf, w, h, tx, ty, label, (0xFF, 0xFF, 0xFF), 2)

        draw_text(buf, w, h, pad + badge_w + 24, y + 6, "#" + colors[key].upper(), HEX_COLOR, 2)
        draw_text(buf, w, h, pad + badge_w + 200, y + 6, key, KEY_COLOR, 2)
        y += row_h

    os.makedirs(os.path.dirname(out), exist_ok=True)
    write_png(out, w, h, buf)
    print("%s  (%d 机种, %dx%d)" % (out, len(order), w, h))


if __name__ == "__main__":
    main()
