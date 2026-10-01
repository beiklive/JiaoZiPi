#include "DataPaths.h"

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <utility>

#if defined(_WIN32)
#include <direct.h>
#include <sys/stat.h>
#include <sys/types.h>
#else
#include <sys/stat.h>
#include <sys/types.h>
#endif

namespace paths {
namespace {

bool MakeSingleDirectory(const std::string& path) {
    if (path.empty()) {
        return false;
    }
#if defined(_WIN32)
    if (_mkdir(path.c_str()) == 0) {
        return true;
    }
#else
    if (mkdir(path.c_str(), 0755) == 0) {
        return true;
    }
#endif
    return errno == EEXIST;
}

bool StatIsDirectory(const std::string& path) {
#if defined(_WIN32)
    struct _stat info {};
    if (_stat(path.c_str(), &info) != 0) {
        return false;
    }
    return (info.st_mode & _S_IFDIR) != 0;
#else
    struct stat info {};
    if (stat(path.c_str(), &info) != 0) {
        return false;
    }
    return S_ISDIR(info.st_mode) != 0;
#endif
}

bool StatIsRegularFile(const std::string& path) {
#if defined(_WIN32)
    struct _stat info {};
    if (_stat(path.c_str(), &info) != 0) {
        return false;
    }
    return (info.st_mode & _S_IFREG) != 0;
#else
    struct stat info {};
    if (stat(path.c_str(), &info) != 0) {
        return false;
    }
    return S_ISREG(info.st_mode) != 0;
#endif
}

// "C:" / "sdmc:" / "romfs:" 这类前缀（含 ':' 且后面紧跟斜杠）
bool IsDevicePrefix(std::string_view text) {
    if (text.empty() || text.front() == '/') {
        return false;
    }
    const std::size_t colon = text.find(':');
    if (colon == std::string_view::npos || colon == 0) {
        return false;
    }
    return colon + 1 >= text.size() || text[colon + 1] == '/';
}

}  // namespace

std::string Normalize(std::string_view path) {
    if (path.empty()) {
        return std::string();
    }

    const bool keptLeadingDoubleSlash = path.size() >= 2 && path[0] == '/' && path[1] == '/';

    std::string collapsed;
    collapsed.reserve(path.size());
    for (const char raw : path) {
        const char ch = raw == '\\' ? '/' : raw;
        if (ch == '/' && !collapsed.empty() && collapsed.back() == '/') {
            continue;
        }
        collapsed.push_back(ch);
    }
    if (keptLeadingDoubleSlash && collapsed.size() >= 2 && collapsed[0] == '/' && collapsed[1] != '/') {
        collapsed.insert(collapsed.begin(), '/');  // 还原 UNC 前缀 "//server/share"
    }
    while (collapsed.size() > 1 && collapsed.back() == '/') {
        collapsed.pop_back();
    }
    return collapsed;
}

bool IsAbsolute(std::string_view path) {
    if (path.empty()) {
        return false;
    }
    if (path.front() == '/') {
        return true;
    }
    return IsDevicePrefix(path);
}

std::string Join(std::string_view base, std::string_view child) {
    if (child.empty()) {
        return Normalize(base);
    }
    if (base.empty() || IsAbsolute(child)) {
        return Normalize(child);
    }
    std::string out = Normalize(base);
    if (!out.empty() && out.back() != '/' && out.back() != ':') {
        out.push_back('/');
    }
    out.append(child.data(), child.size());
    return Normalize(out);
}

std::string ParentDirectory(std::string_view path) {
    const std::string normalized = Normalize(path);
    const std::size_t slash = normalized.find_last_of('/');
    if (slash == std::string::npos) {
        return std::string();
    }
    if (slash == 0) {
        return std::string("/");
    }
    std::string parent = normalized.substr(0, slash);
    if (!parent.empty() && parent.back() == ':') {
        parent.push_back('/');  // "sdmc:" → "sdmc:/"
    }
    return parent;
}

std::string FileName(std::string_view path) {
    const std::string normalized = Normalize(path);
    const std::size_t slash = normalized.find_last_of('/');
    return slash == std::string::npos ? normalized : normalized.substr(slash + 1);
}

std::string NormalizeKey(std::string_view text) {
    std::string out;
    out.reserve(text.size());
    for (const char raw : text) {
        char ch = raw;
        if (ch >= 'A' && ch <= 'Z') {
            ch = static_cast<char>(ch - 'A' + 'a');
        }
        const bool keep = (ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9') || ch == '_' ||
                          ch == '-' || ch == '.';
        if (!keep) {
            ch = '_';
        }
        if (ch == '_' && !out.empty() && out.back() == '_') {
            continue;  // 压缩连续下划线
        }
        out.push_back(ch);
    }
    while (!out.empty() && out.front() == '.') {
        out.erase(out.begin());  // 防止隐藏文件与 ".." 上跳
    }
    while (!out.empty() && (out.back() == '_' || out.back() == '.')) {
        out.pop_back();
    }
    return out.empty() ? std::string("unknown") : out;
}

bool DirectoryExists(std::string_view path) {
    const std::string normalized = Normalize(path);
    return !normalized.empty() && StatIsDirectory(normalized);
}

bool FileExists(std::string_view path) {
    const std::string normalized = Normalize(path);
    return !normalized.empty() && StatIsRegularFile(normalized);
}

bool MakeDirectories(std::string_view path) {
    const std::string normalized = Normalize(path);
    if (normalized.empty()) {
        return false;
    }
    if (StatIsDirectory(normalized)) {
        return true;
    }

    // 逐级创建中间目录，最后再创建目标本身
    for (std::size_t i = 1; i < normalized.size(); ++i) {
        if (normalized[i] != '/') {
            continue;
        }
        const std::string prefix = normalized.substr(0, i);
        if (IsDevicePrefix(prefix)) {
            continue;  // "sdmc:" / "C:" 是设备或盘符，不是要创建的目录
        }
        MakeSingleDirectory(prefix);
    }
    MakeSingleDirectory(normalized);
    return StatIsDirectory(normalized);
}

bool IsWritableDirectory(std::string_view path, std::string_view probeName) {
    const std::string directory = Normalize(path);
    if (directory.empty() || !StatIsDirectory(directory)) {
        return false;
    }
    const std::string probe = Join(directory, probeName);
    std::FILE* file = std::fopen(probe.c_str(), "wb");
    if (file == nullptr) {
        return false;
    }
    std::fputc('\n', file);
    std::fclose(file);
    std::remove(probe.c_str());
    return true;
}

// ---------------------------------------------------------------------------
// DataRoot
// ---------------------------------------------------------------------------
DataRoot& DataRoot::Default() {
    static DataRoot instance;
    return instance;
}

void DataRoot::Set(std::string_view path) {
    const std::string normalized = Normalize(path);
    std::lock_guard<std::mutex> lock(mutex_);
    root_ = normalized;
    lastError_.clear();
    if (root_.empty()) {
        lastError_ = "数据根目录为空";
    }
}

void DataRoot::Clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    root_.clear();
    lastError_.clear();
}

bool DataRoot::IsSet() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return !root_.empty();
}

std::string DataRoot::Get() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return root_;
}

std::string DataRoot::Sub(std::string_view name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return Join(root_, name);
}

bool DataRoot::Ensure(std::string_view name) const {
    std::string target;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (root_.empty()) {
            lastError_ = "数据根目录未设置";
            return false;
        }
        target = name.empty() ? root_ : Join(root_, name);
    }

    if (MakeDirectories(target)) {
        return true;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    lastError_ = "无法创建目录：" + target;
    return false;
}

bool DataRoot::EnsureStartupLayout() const {
    for (const char* name : kStartupDirectories) {
        if (!Ensure(name)) {
            return false;
        }
    }
    return Ensure();
}

bool DataRoot::EnsureLayout() const {
    for (const char* name : kRootDirectories) {
        if (!Ensure(name)) {
            return false;
        }
    }
    return Ensure();
}

std::string DataRoot::LastError() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return lastError_;
}

void SetDataRoot(std::string_view path) { DataRoot::Default().Set(path); }

std::string DataRootPath() { return DataRoot::Default().Get(); }

std::string DataPath(std::string_view relative) { return DataRoot::Default().Sub(relative); }

bool EnsureDataDirectories() { return DataRoot::Default().EnsureStartupLayout(); }

bool EnsureAllDirectories() { return DataRoot::Default().EnsureLayout(); }

// ---------------------------------------------------------------------------
// 常用路径
// ---------------------------------------------------------------------------
std::string FrontendConfigPath() {
    return Join(DataPath(sub::kConfig), sub::kFrontendConfig);
}

std::string PlatformConfigPath(std::string_view platform) {
    return Join(Join(DataPath(sub::kConfig), sub::kPlatforms),
                NormalizeKey(platform) + ".json");
}

std::string CoreConfigPath(std::string_view core) {
    return Join(Join(DataPath(sub::kConfig), sub::kCores), NormalizeKey(core) + ".json");
}

std::string GameConfigPath(std::string_view platform, std::string_view gameId) {
    return Join(Join(Join(DataPath(sub::kConfig), sub::kGames), NormalizeKey(platform)),
                NormalizeKey(gameId) + ".json");
}

std::string SaveFilePath(std::string_view platform, std::string_view gameId,
                         std::string_view extension) {
    return Join(Join(Join(DataPath(sub::kData), sub::kSaves), NormalizeKey(platform)),
                NormalizeKey(gameId) + std::string(extension));
}

std::string StateFilePath(std::string_view platform, std::string_view gameId, int slot,
                          std::string_view extension) {
    return Join(Join(Join(DataPath(sub::kData), sub::kStates), NormalizeKey(platform)),
                NormalizeKey(gameId) + std::string(extension) + std::to_string(slot));
}

std::string NandDirectory(std::string_view platform) {
    return Join(Join(DataPath(sub::kData), sub::kNand), NormalizeKey(platform));
}

std::string BiosDirectory(std::string_view platform) {
    return Join(Join(DataPath(sub::kSystem), sub::kBios), NormalizeKey(platform));
}

std::string DatabaseDirectory() { return Join(DataPath(sub::kSystem), sub::kDatabase); }

std::string ThemeDirectory(std::string_view theme) {
    const std::string base = Join(DataPath(sub::kMedia), sub::kThemes);
    return theme.empty() ? base : Join(base, NormalizeKey(theme));
}

std::string ShaderDirectory(std::string_view backend) {
    const std::string base = Join(DataPath(sub::kMedia), sub::kShaders);
    return backend.empty() ? base : Join(base, NormalizeKey(backend));
}

std::string OverlayDirectory() { return Join(DataPath(sub::kMedia), sub::kOverlays); }

std::string ThumbnailDirectory(std::string_view platform) {
    return Join(Join(DataPath(sub::kMedia), sub::kThumbnails), NormalizeKey(platform));
}

std::string CoreFilePath(std::string_view core, std::string_view extension) {
    return Join(DataPath(sub::kCores), NormalizeKey(core) + std::string(extension));
}

std::string CacheDirectory(std::string_view name) {
    const std::string base = DataPath(sub::kCache);
    return name.empty() ? base : Join(base, NormalizeKey(name));
}

std::string DataRootFromEnvironment(const char* environmentVariable) {
    if (environmentVariable == nullptr || environmentVariable[0] == '\0') {
        return std::string();
    }
    const char* value = std::getenv(environmentVariable);
    if (value == nullptr || value[0] == '\0') {
        return std::string();
    }
    return Normalize(value);
}

std::string ResolveDataRootOverride(int argc, char** argv, const char* environmentVariable) {
    constexpr std::string_view kFlag = "--data-dir";
    if (argv != nullptr) {
        for (int i = 1; i < argc; ++i) {
            if (argv[i] == nullptr) {
                continue;
            }
            const std::string_view argument(argv[i]);
            if (argument == kFlag) {
                if (i + 1 < argc && argv[i + 1] != nullptr) {
                    return Normalize(argv[i + 1]);
                }
                continue;
            }
            if (argument.size() > kFlag.size() && argument.compare(0, kFlag.size(), kFlag) == 0 &&
                argument[kFlag.size()] == '=') {
                return Normalize(argument.substr(kFlag.size() + 1));
            }
        }
    }
    return DataRootFromEnvironment(environmentVariable);
}

}  // namespace paths
