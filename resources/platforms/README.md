# platforms —— 各机种内置配置

一个机种一个文件，文件名即 [`MachineKey()`](../../src/utils/JiaoZiPiMachine.h)（`fc` `sfc` `gb` `gbc` `gba` `nds` `3ds` `ngc` `wii` `md` `ss` `dc` `ps1` `psp` `arcade`）。

```json
{
  "machine": "gba",
  "name": "Game Boy Advance",
  "core": "mgba",
  "extensions": [".gba", ".agb", ".zip", ".7z"]
}
```

| 字段 | 含义 |
| --- | --- |
| `machine` | 机种 key，与 `MachineKey()` 一致，同时也是文件名 |
| `name` | 机种全名，与 `MachineName()` 一致 |
| `core` | 该机种的默认核心名 |
| `extensions` | 可被该机种识别的 ROM 后缀，小写、点开头；含通用归档 `.zip` / `.7z` |

## 约定

- **`.zip` / `.7z` 是归档**：前端先解压，再把里面的 ROM 交给核心。**唯一例外是 `arcade`**——街机的 zip/7z 本身就是 ROM 集，不能解压，要原样传给核心。
- `.chd`、`.cso`、`.rvz` 这类是**压缩镜像格式**，不是归档，直接交给核心，不要解压。
- 这里的配置是**只读内置值**；用户在 `config/platforms/<key>.json` 里的同名配置会覆盖它（路径见 [docs/data_paths.md](../../docs/data_paths.md)）。
- `core` 只是默认核心名；切换到别的核心实现时改这里一处即可。

## 生成方式

`machine` 与 `name` 解析自 [JiaoZiPiMachine.h](../../src/utils/JiaoZiPiMachine.h)，核心名与后缀表在脚本里维护：

```bash
python3 tools/gen_platform_configs.py
```

新增机种时：先在头文件加 `Machine` 枚举值 + `MachineKey()` + `MachineName()`，再到 [gen_platform_configs.py](../../tools/gen_platform_configs.py) 的 `CORES` 表补一行，重新运行脚本。缺项会直接报错，不会静默生成半成品。

冒烟测试会校验 15 个文件与头文件一致、后缀合法、且都列出了 `.zip` 与 `.7z`。
