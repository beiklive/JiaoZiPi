#!/usr/bin/env python3
"""生成各机种的内置配置文件 resources/platforms/<machine>.json。

机种 key 与名称直接解析 src/utils/JiaoZiPiMachine.h（MachineKey / MachineName），
保证与代码单一来源；核心名与后缀列表在下面的表里维护。

用法：python3 tools/gen_platform_configs.py
"""

import json
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
HEADER = os.path.join(ROOT, "src", "utils", "JiaoZiPiMachine.h")
OUT_DIR = os.path.join(ROOT, "resources", "platforms")

# 通用归档后缀：前端解压后把里面的 ROM 交给核心（ARCADE 例外，见下）
ARCHIVES = [".zip", ".7z"]

# 机种 key -> (默认核心名, 该机种 ROM 后缀)
CORES = {
    "fc":     ("fceumm",          [".nes", ".fds", ".unf", ".unif"]),
    "sfc":    ("snes9x",          [".sfc", ".smc", ".swc", ".fig", ".bs"]),
    "gb":     ("gambatte",        [".gb"]),
    "gbc":    ("gambatte",        [".gbc", ".gb"]),
    "gba":    ("mgba",            [".gba", ".agb"]),
    "nds":    ("melonds",         [".nds", ".dsi"]),
    "3ds":    ("azahar",          [".3ds", ".cci", ".cxi", ".cia"]),
    "ngc":    ("dolphin",         [".iso", ".gcm", ".rvz", ".ciso", ".wia"]),
    "wii":    ("dolphin",         [".iso", ".wbfs", ".rvz", ".ciso", ".wia"]),
    "md":     ("genesis_plus_gx", [".md", ".gen", ".bin", ".smd"]),
    "ss":     ("yabasanshiro",    [".cue", ".bin", ".iso", ".chd", ".mds", ".ccd"]),
    "dc":     ("flycast",         [".gdi", ".cdi", ".cue", ".bin", ".chd", ".iso", ".mds"]),
    "ps1":    ("duckstation",     [".cue", ".bin", ".iso", ".img", ".chd", ".pbp", ".ccd"]),
    "psp":    ("ppsspp",          [".iso", ".cso", ".chd", ".pbp"]),
    # 街机的 zip/7z 本身就是 ROM 集，前端不要解压
    "arcade": ("fbneo",           [".zip", ".7z", ".chd"]),
}

NO_ARCHIVE_KEY = "arcade"


def parse_header():
    src = open(HEADER, encoding="utf-8").read()

    def body(name):
        match = re.search(r"constexpr const char\* %s\(Machine m\) \{(.*?)\n\}" % name, src, re.S)
        if not match:
            sys.exit("未在 %s 中找到 %s()" % (HEADER, name))
        return match.group(1)

    keys = re.findall(r'case Machine::(\w+):\s*return "([^"]*)";', body("MachineKey"))
    names = dict(re.findall(r'case Machine::(\w+):\s*return "([^"]*)";', body("MachineName")))
    return keys, names


def main():
    keys, names = parse_header()
    os.makedirs(OUT_DIR, exist_ok=True)

    written = []
    for enum_name, key in keys:
        if key not in CORES:
            sys.exit("机种 %s（key=%s）缺少核心/后缀配置，请补到 CORES 表" % (enum_name, key))
        core, extensions = CORES[key]
        all_extensions = list(extensions)
        if key != NO_ARCHIVE_KEY:
            all_extensions += [ext for ext in ARCHIVES if ext not in all_extensions]
        payload = {
            "machine": key,
            "name": names.get(enum_name, enum_name),
            "core": core,
            "extensions": all_extensions,
        }
        path = os.path.join(OUT_DIR, key + ".json")
        with open(path, "w", encoding="utf-8") as handle:
            json.dump(payload, handle, ensure_ascii=False, indent=2)
            handle.write("\n")
        written.append(key)

    extra = sorted(set(CORES) - set(written))
    if extra:
        sys.exit("CORES 表里有头文件中不存在的机种：%s" % ", ".join(extra))
    print("已生成 %d 个机种配置 -> resources/platforms/" % len(written))
    print("  " + " ".join(written))


if __name__ == "__main__":
    main()
