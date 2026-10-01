# third_party —— 第三方代码与模拟器核心

## 已引入的子模块

| 路径 | 上游 | 版本 | 许可证 | 用途 |
| --- | --- | --- | --- | --- |
| [spdlog/](spdlog/) | https://github.com/gabime/spdlog | v1.17.0 | MIT | 日志系统的文件 IO、轮转、控制台着色（header-only 使用） |
| [json/](json/) | https://github.com/nlohmann/json | v3.12.0 | MIT | 多语言系统读取单个 `language.json`，以及机种配置文件（header-only 使用） |
| [miniz/](miniz/) | https://github.com/richgel999/miniz | 3.1.2 | MIT | zip 解压（zip64、流式，纯 C，编译 `miniz.c` 即可） |
| [libarchive/](libarchive/) | https://github.com/libarchive/libarchive | v3.8.9 | BSD-2-Clause | 7z 解压；同时支持 zip / rar / tar 等 |

首次克隆后拉取：

```bash
git submodule update --init --recursive
```

## 解压方案

| 格式 | 库 | 说明 |
| --- | --- | --- |
| `.zip` | miniz | 轻量、无额外依赖，常见路径走它 |
| `.7z` | libarchive | 读 7z 需要 liblzma（LZMA/LZMA2）；zip 的 deflate 走 zlib |

- 两个库在 zip 上有重叠：默认 **zip → miniz、7z → libarchive**；若更想只维护一套 API，可以只留 libarchive（体积更大，但要加 zlib + liblzma 两个构建依赖）。
- 前端解压策略：`resources/platforms/<机种>.json` 的 `extensions` 里列出 `.zip` / `.7z` 表示「解压后把 ROM 交给核心」；**`arcade` 例外**——街机 zip/7z 就是 ROM 集本身，必须原样传给核心。
- `.chd`、`.cso`、`.rvz` 不是归档，是压缩镜像格式，交给核心处理，不要解压。
- Switch / Android / iOS 上这两个库的构建与依赖（zlib、liblzma 是否可用）需要在目标平台实测后再写进构建脚本。

## 模拟器核心

- 每个核心一个子目录，尽量保持上游原样，本地改动以 patch 或 fork 分支形式记录
- 记录上游地址、commit / tag、许可证文件
- 与前端代码隔离：上层只通过 `src/core/` 的接口访问核心
- 计划接入的核心清单见根 [README.md](../README.md)
