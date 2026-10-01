# resources —— 资源目录

运行时资源统一放这里，按类型分目录，由构建脚本打包（Switch 走 romfs）。

| 目录 | 内容 |
| --- | --- |
| [platforms/](platforms/README.md) | 各机种内置配置（机种名 / 默认核心 / 支持的后缀） |
| [fonts/](fonts/README.md) | 字体 |
| [icons/](icons/README.md) | 图标、机种徽标 |
| [images/](images/README.md) | 图片、背景、Logo |
| [sounds/](sounds/README.md) | 音效 |
| [lang/](lang/README.md) | 语言表 |
| [shaders/](shaders/README.md) | 滤镜 |
| [overlays/](overlays/README.md) | 遮罩 |
| [cheats/](cheats/README.md) | 金手指 |
| [themes/](themes/README.md) | 主题 |

约定：

- 文件名小写下划线，按机种或页面加前缀（如 `gba_badge.png`）
- 素材保留源文件与导出文件的一致性，不直接放大低分辨率原图
- 资源根路径由 `src/platform/` 提供，代码里不写死绝对路径
