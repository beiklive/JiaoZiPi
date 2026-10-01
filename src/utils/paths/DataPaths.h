#pragma once

// 数据根目录管理（独立模块，不依赖宿主项目）
//
// 目录布局（规范见 docs/data_paths.md）：
//   <root>/config/{frontend.json, platforms/, cores/, games/}
//   <root>/data/{saves/, states/, nand/}
//   <root>/system/{bios/, database/}
//   <root>/media/{themes/, shaders/<后端>/, overlays/, thumbnails/, fonts/, icons/}
//   <root>/cores/
//   <root>/cache/
//   <root>/playlists/            游戏列表（playlists/<机种或分类>.json）
//
// 注意命名：数据目录里的 platforms 指**机种**（gba / ps1 / 3ds ...），
// 与 src/platform/ 的宿主平台（Switch / Windows / Android ...）不是一回事。
//
// 策略：
//   - 首次启动只创建 config/，其余目录按需懒创建（根目录保持干净）
//   - 覆盖来源：--data-dir=<path> / --data-dir <path> 优先，其次 JIAOZIPI_DATA_DIR
//   - 拼接用的机种/游戏/核心名会先经 NormalizeKey() 规范化（小写 + 非法字符换 _）
//   - 无第三方依赖，C++17
//
// 典型用法：
//   std::string root = paths::ResolveDataRootOverride(argc, argv);
//   if (root.empty()) root = platform::DefaultDataRoot();   // 平台层提供默认根
//   paths::SetDataRoot(root);
//   paths::EnsureDataDirectories();                          // 建 root + config
//   std::string save = paths::SaveFilePath("gba", gameId);
//
// 迁移到其他项目：复制本目录即可（不依赖 log/、i18n/、format/）。

#include <mutex>
#include <string>
#include <string_view>

namespace paths {

// 顶层目录
namespace sub {
constexpr const char* kConfig = "config";
constexpr const char* kData = "data";
constexpr const char* kSystem = "system";
constexpr const char* kMedia = "media";
constexpr const char* kCores = "cores";
constexpr const char* kCache = "cache";
constexpr const char* kPlaylists = "playlists";  // 游戏列表（类似 RetroArch playlists/）

// 二级目录
constexpr const char* kPlatforms = "platforms";  // config/platforms —— 机种全局配置
constexpr const char* kGames = "games";          // config/games/<机种>/ —— 每游戏配置
constexpr const char* kSaves = "saves";          // data/saves/<机种>/
constexpr const char* kStates = "states";        // data/states/<机种>/
constexpr const char* kNand = "nand";            // data/nand/<机种>/ —— 虚拟 NAND
constexpr const char* kBios = "bios";            // system/bios/<机种>/
constexpr const char* kDatabase = "database";    // system/database/
constexpr const char* kThemes = "themes";        // media/themes/
constexpr const char* kShaders = "shaders";      // media/shaders/<后端>/
constexpr const char* kOverlays = "overlays";    // media/overlays/
constexpr const char* kThumbnails = "thumbnails";// media/thumbnails/<机种>/
constexpr const char* kFonts = "fonts";
constexpr const char* kIcons = "icons";
constexpr const char* kFrontendConfig = "frontend.json";
}  // namespace sub

// 着色器后端目录名
namespace shader {
constexpr const char* kVulkan = "vulkan";
constexpr const char* kOpenGl = "opengl";
}  // namespace shader

// 应用目录名：<可执行文件目录>/JiaoZiPi、sdmc:/JiaoZiPi
constexpr const char* kAppDirectoryName = "JiaoZiPi";

// 首次启动创建的目录（最小集）
inline constexpr const char* kStartupDirectories[] = {sub::kConfig};
// 全部顶层目录（需要预创建时用 EnsureAllDirectories()）
inline constexpr const char* kRootDirectories[] = {sub::kConfig, sub::kData,     sub::kSystem,
                                                  sub::kMedia,  sub::kCores,    sub::kCache,
                                                  sub::kPlaylists};

// ---------------------------------------------------------------------------
// 纯字符串处理（不做文件系统访问）
// ---------------------------------------------------------------------------
// 统一为正斜杠、折叠重复斜杠、去掉末尾斜杠；保留 "sdmc:" / "C:" / UNC 前缀
std::string Normalize(std::string_view path);
// 拼接父子路径；child 为绝对路径时直接返回 child
std::string Join(std::string_view base, std::string_view child);
std::string ParentDirectory(std::string_view path);
std::string FileName(std::string_view path);
// 绝对路径：以 '/' 开头，或 "C:/"、"sdmc:/" 这类带设备/盘符前缀
bool IsAbsolute(std::string_view path);

// 机种名 / 游戏 ID / 核心名 → 安全的目录与文件名片段：
// ASCII 转小写，[a-z0-9_.-] 之外的字符换成 '_'，压缩连续 '_'，去掉首部 '.' 与首尾 '_' / '.'
// 结果为空时返回 "unknown"
std::string NormalizeKey(std::string_view text);

// ---------------------------------------------------------------------------
// 文件系统（POSIX stat/mkdir，Windows _stat/_mkdir，无第三方依赖）
// ---------------------------------------------------------------------------
bool DirectoryExists(std::string_view path);
bool FileExists(std::string_view path);
// 递归创建目录；已存在视为成功
bool MakeDirectories(std::string_view path);
// 在目录里建一个探针文件再删掉，用于判断目录是否真的可写
bool IsWritableDirectory(std::string_view path, std::string_view probeName = ".jzp_write_test");

// ---------------------------------------------------------------------------
// 数据根目录
// ---------------------------------------------------------------------------
class DataRoot {
public:
    static DataRoot& Default();

    // 设置根目录（内部会 Normalize）；传空字符串等价于 Clear()
    void Set(std::string_view path);
    void Clear();
    bool IsSet() const;
    std::string Get() const;

    // root + "/" + name（name 为空时返回 root）
    std::string Sub(std::string_view name) const;
    // 创建 root（name 非空时创建 root/name）
    bool Ensure(std::string_view name = std::string_view()) const;
    // 创建 root 与初始启动目录（config/）
    bool EnsureStartupLayout() const;
    // 创建 root 与 kRootDirectories 的全部顶层目录
    bool EnsureLayout() const;

    std::string LastError() const;

private:
    DataRoot() = default;

    mutable std::mutex mutex_;
    std::string root_;
    mutable std::string lastError_;
};

// 便捷接口（走 DataRoot::Default()）
void SetDataRoot(std::string_view path);
std::string DataRootPath();
std::string DataPath(std::string_view relative);
bool EnsureDataDirectories();  // 初始启动目录：root + config
bool EnsureAllDirectories();   // 全部顶层目录

// ---------------------------------------------------------------------------
// 常用路径（未设置根目录时返回相对路径，不抛错）
// ---------------------------------------------------------------------------
std::string FrontendConfigPath();                                              // config/frontend.json
std::string PlatformConfigPath(std::string_view platform);                     // config/platforms/<platform>.json
std::string CoreConfigPath(std::string_view core);                             // config/cores/<core>.json
std::string GameConfigPath(std::string_view platform, std::string_view gameId);// config/games/<platform>/<gameId>.json
std::string SaveFilePath(std::string_view platform, std::string_view gameId,
                         std::string_view extension = ".sav");                 // data/saves/<platform>/<gameId>.sav
std::string StateFilePath(std::string_view platform, std::string_view gameId, int slot = 0,
                          std::string_view extension = ".st");                 // data/states/<platform>/<gameId>.st0
std::string NandDirectory(std::string_view platform);                          // data/nand/<platform>
std::string BiosDirectory(std::string_view platform);                          // system/bios/<platform>
std::string DatabaseDirectory();                                               // system/database
std::string ThemeDirectory(std::string_view theme = std::string_view());       // media/themes[/<theme>]
std::string ShaderDirectory(std::string_view backend = shader::kVulkan);       // media/shaders/<backend>
std::string OverlayDirectory();                                                // media/overlays
std::string ThumbnailDirectory(std::string_view platform);                     // media/thumbnails/<platform>
std::string CoreFilePath(std::string_view core, std::string_view extension = std::string_view());
std::string PlaylistDirectory();                                               // playlists
std::string PlaylistPath(std::string_view name);                               // playlists/<name>.json
std::string CacheDirectory(std::string_view name = std::string_view());        // cache[/<name>]

// ---------------------------------------------------------------------------
// 可执行文件位置（Windows / macOS / Linux / Switch）
// ---------------------------------------------------------------------------
// 当前进程可执行文件的绝对路径；取不到时返回空串。
// Switch 没有 /proc/self/exe，必须传 argv（argv[0] 由 hbmenu 给出，形如 "sdmc:/switch/xxx.nro"）；
// 其它平台传入时作为兜底。
std::string ExecutablePath(int argc = 0, char** argv = nullptr);
// 可执行文件所在目录
std::string ExecutableDirectory(int argc = 0, char** argv = nullptr);
// 规范里的默认数据根：
//   Windows / macOS / Linux → <可执行文件目录>/JiaoZiPi
//   Switch                  → sdmc:/JiaoZiPi
//   iOS / Android 等无法从可执行文件推导的平台 → 返回空串，由平台层提供
std::string DefaultDataRoot(int argc = 0, char** argv = nullptr);
// 默认只读资源根：
//   Windows / macOS / Linux → <可执行文件目录>/resources
//   Switch                  → romfs:/
//   其它平台 → 空串，由平台层提供
std::string DefaultResourceRoot(int argc = 0, char** argv = nullptr);

// ---------------------------------------------------------------------------
// 覆盖解析
// ---------------------------------------------------------------------------
// 只读环境变量；未设置返回空串
std::string DataRootFromEnvironment(const char* environmentVariable = "JIAOZIPI_DATA_DIR");
// 命令行优先：支持 --data-dir=<path> 与 --data-dir <path>，都没有则回退环境变量
std::string ResolveDataRootOverride(int argc, char** argv,
                                    const char* environmentVariable = "JIAOZIPI_DATA_DIR");

}  // namespace paths
