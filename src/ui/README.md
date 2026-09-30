# ui —— 界面

放什么：

- 页面与导航：游戏库、设置、核心管理、机种选择
- 机种徽标与配色（取 [JiaoZiPiMachine.h](../utils/JiaoZiPiMachine.h)）、加载动画、列表标签
- 文案取 `utils/i18n/`，界面内日志走 `utils/log/` 的 `CallbackSink`

约定：

- 颜色只取自机种主题色规范，不在界面里散落硬编码色值
- 界面不直接调用核心，全部经 `core/` 与 `emulator/` 的接口
