# icons —— 图标

放什么：菜单图标、状态图标、机种徽标。

约定：

- 命名 `<机种|页面>_<用途>.png`，如 `gba_badge.png`、`menu_settings.png`
- 机种徽标底色取 [JiaoZiPiMachine.h](../../src/utils/JiaoZiPiMachine.h) 的主题色，不另配
- 需要多倍图时用 `@2x` 后缀，并在代码里按倍数选择
