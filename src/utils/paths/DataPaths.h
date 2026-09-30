#pragma once

// 数据根目录管理（独立模块，不依赖宿主项目）
//
// 负责"数据根目录"的保存与拼接，以及目录创建 / 可写探测：
//   - 平台无关：默认根路径由平台层算好后 Set() 进来（见 docs/data_paths.md）
//   - 覆盖来源：--data-dir=<path> / --data-dir <path> 优先，其次环境变量 JIAOZIPI_DATA_DIR
//   - 无第三方依赖，C++17
//
// 典型用法：
//   std::string root = paths::ResolveDataRootOverride(argc, argv);
//   if (root.empty()) root = platform::DefaultDataRoot();   // 平台层提供
//   paths::SetDataRoot(root);
//   paths::EnsureDataDirectories();                          // 创建规范里的全部子目录
//   std::string config = paths::DataPath("config/general.json");
//
// 迁移到其他项目：复制本目录即可（不依赖 log/、i18n/、format/）。

#include <mutex>
#include <string>
#include <string_view>
#include <vector>

namespace paths {

// 数据根目录下的子目录名，与 docs/data_paths.md 一致
namespace sub {
constexpr const char* kConfig = "config";
constexpr const char* kSaves = "saves";
constexpr const char* kStates = "states";
constexpr const char* kLogs = "logs";
constexpr const char* kCache = "cache";
constexpr const char* kLang = "lang";
constexpr const char* kCheats = "cheats";
constexpr const char* kShaders = "shaders";
constexpr const char* kOverlays = "overlays";
constexpr const char* kThemes = "themes";
}  // namespace sub

inline constexpr const char* kSubDirectories[] = {
    sub::kConfig, sub::kSaves,  sub::kStates,   sub::kLogs,   sub::kCache,
    sub::kLang,   sub::kCheats, sub::kShaders,  sub::kOverlays, sub::kThemes,
};

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

    // root + "/" + name
    std::string Sub(std::string_view name) const;
    // 创建 root（name 非空时创建 root/name）
    bool Ensure(std::string_view name = std::string_view()) const;
    // 创建 root 与 kSubDirectories 里的全部子目录
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
bool EnsureDataDirectories();

// ---------------------------------------------------------------------------
// 覆盖解析
// ---------------------------------------------------------------------------
// 只读环境变量；未设置返回空串
std::string DataRootFromEnvironment(const char* environmentVariable = "JIAOZIPI_DATA_DIR");
// 命令行优先：支持 --data-dir=<path> 与 --data-dir <path>，都没有则回退环境变量
std::string ResolveDataRootOverride(int argc, char** argv,
                                    const char* environmentVariable = "JIAOZIPI_DATA_DIR");

}  // namespace paths
