# third_party —— 第三方代码与模拟器核心

## 已引入的子模块

| 路径 | 上游 | 版本 | 许可证 | 用途 |
| --- | --- | --- | --- | --- |
| [spdlog/](spdlog/) | https://github.com/gabime/spdlog | v1.17.0 | MIT | 日志系统的文件 IO、轮转、控制台着色（header-only 使用） |
| [json/](json/) | https://github.com/nlohmann/json | v3.12.0 | MIT | 多语言系统的 JSON 语言表解析（可选） |

首次克隆后拉取：

```bash
git submodule update --init --recursive
```

两处使用都是 header-only：把对应 `include/` 加入头文件搜索路径即可，不需要编译第三方源码、也不影响链接。

## 模拟器核心

- 每个核心一个子目录，尽量保持上游原样，本地改动以 patch 或 fork 分支形式记录
- 记录上游地址、commit / tag、许可证文件
- 与前端代码隔离：上层只通过 `src/core/` 的接口访问核心
- 计划接入的核心清单见根 [README.md](../README.md)
