# platform —— 平台适配层

放什么：

- 每个目标平台一个实现（如 `switch/`、`desktop/`、`android/`）
- 只负责算出各平台的**默认数据根路径**（可执行文件目录 / Documents / `/sdcard` / `sdmc:`）与只读资源根，路径的保存、拼接、创建、覆盖解析由 [src/utils/paths](../utils/paths/README.md) 负责，规范见 [docs/data_paths.md](../../docs/data_paths.md)
- 窗口与渲染后端、文件系统挂载、输入源、时钟、休眠与退出

约定：

- 上层不出现 `#ifdef` 平台分支，差异全部收敛到本目录
- 新增平台 = 新增一个实现，不改上层逻辑
- 资源根路径与数据根路径都由这里提供，代码里不写死绝对路径（除规范中约定的固定前缀）
