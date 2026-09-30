// 日志系统 + 多语言系统 冒烟测试
//
// 构建（可直接用 tools/build_utils_test.sh）：
//   c++ -std=c++17 -Wall -Wextra -I src/utils -I third_party/spdlog/include \
//       src/utils/log/Logger.cpp src/utils/i18n/I18n.cpp src/utils/paths/DataPaths.cpp \
//       tests/utils_smoke.cpp -o tests/utils_smoke
// 运行：在仓库根目录执行 ./tests/utils_smoke（语言文件按 resources/lang 相对路径加载）

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "i18n/I18n.h"
#include "log/Logger.h"
#include "paths/DataPaths.h"

#if defined(_WIN32)
#include <direct.h>
#define MAKE_DIR(path) _mkdir(path)
#define REMOVE_DIR(path) _rmdir(path)
#else
#include <sys/stat.h>
#include <unistd.h>
#define MAKE_DIR(path) mkdir(path, 0755)
#define REMOVE_DIR(path) rmdir(path)
#endif

namespace {

int g_failures = 0;

void Check(bool condition, const std::string& what) {
    if (condition) {
        std::printf("  ok   %s\n", what.c_str());
    } else {
        std::printf("  FAIL %s\n", what.c_str());
        ++g_failures;
    }
}

std::string ReadTextFile(const std::string& path) {
    std::FILE* file = std::fopen(path.c_str(), "rb");
    if (file == nullptr) {
        return std::string();
    }
    std::string out;
    char buffer[1024];
    std::size_t read = 0;
    while ((read = std::fread(buffer, 1, sizeof(buffer), file)) > 0) {
        out.append(buffer, read);
    }
    std::fclose(file);
    return out;
}

bool FileExists(const std::string& path) {
    std::FILE* file = std::fopen(path.c_str(), "rb");
    if (file == nullptr) {
        return false;
    }
    std::fclose(file);
    return true;
}

void TestI18n() {
    std::printf("[i18n]\n");
    i18n::Translator& tr = i18n::Translator::Default();
    tr.Clear();
    tr.SetFallbackLocale("en");

    Check(tr.LoadFile("resources/lang/language.json"), "加载单个 language.json");
    const std::vector<std::string> locales = tr.Locales();
    Check(locales.size() == 3 && locales[0] == "en" && locales[1] == "ja" && locales[2] == "zh",
          "语言代号取自 JSON：en / ja / zh");

    Check(tr.SetLocale("zh"), "切换到 zh");
    Check(tr.Tr("app.name") == "饺子皮", "中文取值正确");
    Check(tr.Tr("greeting", "玩家") == "你好，玩家", "占位符格式化");
    Check(tr.Tr("library.count", 12) == "共 12 个游戏", "整数参数格式化");

    Check(tr.SetLocale("ja"), "切换到 ja");
    Check(tr.Tr("menu.library") == "ライブラリ" && tr.Tr("menu.quit") == "終了", "日文取值正确");
    Check(tr.Tr("app.build") == "development build", "该语言缺失的 key 回退到 fallback(en)");

    Check(tr.SetLocale("zh_TW"), "切换到 zh_TW（表中只有 zh）");
    Check(tr.Tr("menu.settings") == "设置", "语言前缀回退 zh_TW → zh");

    Check(!tr.SetLocale("ko"), "切换到未支持语言返回 false");
    Check(tr.Tr("menu.quit") == "Quit", "未支持语言回退到 en");

    tr.ClearMissingKeys();
    Check(tr.Tr("no.such.key") == "no.such.key", "缺失 key 返回 key 本身");
    const std::vector<std::string> missing = tr.MissingKeys();
    Check(missing.size() == 1 && missing.front() == "no.such.key", "记录缺失 key");

    Check(tr.LoadString(R"({"t.num":{"en":42},"t.obj":{"en":{"nested":"x"}},"t.flat":"文本"})"),
          "LoadString 解析内存语言表");
    Check(tr.Tr("t.num") == "42", "非字符串值转成文本");
    Check(!tr.Has("t.obj") && !tr.Has("t.flat"), "结构不符的条目（值不是语言映射）被忽略");
    Check(!tr.LoadString("{ 这不是 JSON"), "非法 JSON 返回 false");
    Check(!tr.LastError().empty(), "LastError 记录失败原因");

    tr.Set("fr", "app.name", "JiaoZiPi FR");
    Check(tr.SetLocale("fr") && tr.Tr("app.name") == "JiaoZiPi FR", "运行时注册的语言可直接切换");

    tr.SetLocale("zh");
    Check(i18n::Tr("app.tagline") == "多核心模拟器前端", "全局便捷接口 i18n::Tr");
    Check(i18n::Tr("menu.cores") == "核心管理", "全局便捷接口：无参数");
}

// 数据根目录模块
void TestDataPaths() {
    std::printf("[paths]\n");
    MAKE_DIR("tests/tmp_utils_test");

    Check(paths::Normalize("a\\b//c/") == "a/b/c", "Normalize：反斜杠、重复与末尾斜杠");
    Check(paths::Normalize("sdmc://JiaoZiPi//config") == "sdmc:/JiaoZiPi/config", "Normalize：设备前缀");
    Check(paths::Normalize("C:\\Games\\JiaoZiPi") == "C:/Games/JiaoZiPi", "Normalize：盘符");
    Check(paths::Join("sdmc:/JiaoZiPi", "logs") == "sdmc:/JiaoZiPi/logs", "Join 拼接子目录");
    Check(paths::Join("sdmc:/JiaoZiPi", "/abs") == "/abs", "Join：child 为绝对路径时覆盖");
    Check(paths::ParentDirectory("sdmc:/JiaoZiPi/logs") == "sdmc:/JiaoZiPi", "ParentDirectory");
    Check(paths::FileName("a/b/c.txt") == "c.txt", "FileName");
    Check(paths::IsAbsolute("/x") && paths::IsAbsolute("C:/x") && paths::IsAbsolute("sdmc:/x") &&
              !paths::IsAbsolute("x/y"),
          "IsAbsolute");

    const std::string root = "tests/tmp_utils_test/JiaoZiPi";
    Check(paths::MakeDirectories(root + "/config/deep"), "递归创建多级目录");
    Check(paths::DirectoryExists(root + "/config/deep"), "创建的目录存在");
    Check(paths::IsWritableDirectory(root), "目录可写探测（探针文件自删）");

    paths::DataRoot& dataRoot = paths::DataRoot::Default();
    dataRoot.Clear();
    Check(!dataRoot.IsSet() && dataRoot.Get().empty(), "默认未设置数据根目录");
    Check(!dataRoot.EnsureLayout() && !dataRoot.LastError().empty(), "未设置时创建失败并记录原因");

    paths::SetDataRoot(root + "/");  // 顺带验证规范化
    Check(paths::DataRootPath() == root, "设置数据根目录（自动规范化）");
    Check(paths::DataPath("logs") == root + "/logs", "DataPath 拼接");
    Check(paths::EnsureDataDirectories(), "按规范创建全部子目录");

    bool allCreated = true;
    for (const char* name : paths::kSubDirectories) {
        if (!paths::DirectoryExists(paths::DataPath(name))) {
            allCreated = false;
        }
    }
    Check(allCreated, "规范里的 10 个子目录都已创建");

    char arg0[] = "app";
    char arg1[] = "--data-dir";
    char arg2[] = "rel/dir";
    char* argvSeparate[] = {arg0, arg1, arg2, nullptr};
    Check(paths::ResolveDataRootOverride(3, argvSeparate) == "rel/dir", "覆盖：--data-dir <path>");
    char argEquals[] = "--data-dir=rel/eq";
    char* argvEquals[] = {arg0, argEquals, nullptr};
    Check(paths::ResolveDataRootOverride(2, argvEquals) == "rel/eq", "覆盖：--data-dir=<path>");
    char* argvEmpty[] = {arg0, nullptr};
    Check(paths::ResolveDataRootOverride(1, argvEmpty).empty(), "无参数且无环境变量时返回空");
#if defined(_WIN32)
    _putenv_s("JZP_TEST_DATA_DIR", "env/dir");
#else
    setenv("JZP_TEST_DATA_DIR", "env/dir", 1);
#endif
    Check(paths::ResolveDataRootOverride(1, argvEmpty, "JZP_TEST_DATA_DIR") == "env/dir",
          "覆盖：回退到环境变量");

    REMOVE_DIR((root + "/config/deep").c_str());
    for (const char* name : paths::kSubDirectories) {
        REMOVE_DIR(paths::DataPath(name).c_str());
    }
    REMOVE_DIR(root.c_str());
}

void TestLogger() {
    std::printf("[log]\n");
    MAKE_DIR("tests/tmp_utils_test");

    logging::Level parsed = logging::Level::Off;
    Check(logging::ParseLevel("warn", parsed) && parsed == logging::Level::Warn, "ParseLevel(\"warn\")");
    Check(logging::ParseLevel("ERROR", parsed) && parsed == logging::Level::Error, "ParseLevel 大小写无关");
    Check(!logging::ParseLevel("nope", parsed), "无法解析的等级返回 false");
    Check(std::strcmp(logging::LevelName(logging::Level::Fatal), "FATAL") == 0, "LevelName(Fatal)");

    const std::string logPath = "tests/tmp_utils_test/app.log";
    std::remove(logPath.c_str());

    logging::Logger logger;
    logger.SetLevel(logging::Level::Warn);
    logger.SetPattern("[{level}] {file}:{line} {message}");

    logging::FileSink::Options fileOptions;
    fileOptions.path = logPath;
    fileOptions.append = false;
    fileOptions.flushEveryWrite = true;
    auto fileSink = logging::AddFileSink(logger, fileOptions);

    std::vector<std::string> captured;
    logger.AddSink(std::make_shared<logging::CallbackSink>(
        [&captured](const logging::Record& record, std::string_view line) {
            captured.push_back(std::string(logging::LevelName(record.level)) + "|" + std::string(line));
        }));

    Check(logger.SinkCount() == 2, "挂载两个输出端");
    Check(!logger.ShouldLog(logging::Level::Info), "等级过滤：Warn 级别下 Info 被拦截");

    logger.Log(logging::Level::Info, __FILE__, __LINE__, "被过滤的 Info");
    LOG_WARN_TO(logger, "磁盘剩余 {}", 128);
    LOG_ERROR_TO(logger, "核心加载失败：{}", "mgba");

    const std::string content = ReadTextFile(logPath);
    Check(content.find("被过滤的 Info") == std::string::npos, "低于等级的日志没有落盘");
    Check(content.find("[WARN]") != std::string::npos, "WARN 已落盘");
    Check(content.find("磁盘剩余 128") != std::string::npos, "占位符格式化正确");
    Check(content.find("[ERROR]") != std::string::npos, "ERROR 已落盘");
    Check(captured.size() == 2 && captured.front().find("WARN|") == 0, "回调输出端收到结构化记录");

    // 实时写入：不关闭、不 flush，直接读文件即可看到内容
    Check(content.find("核心加载失败：mgba") != std::string::npos, "实时写入（无需手动 flush）");
    Check(fileSink->IsOpen(), "文件输出端处于打开状态");

    logger.SetLevel(logging::Level::Error);
    const std::string before = ReadTextFile(logPath);
    LOG_WARN_TO(logger, "这条不应出现");
    Check(ReadTextFile(logPath) == before, "运行期重新设置等级立即生效");

    logger.Flush();
    logger.RemoveAllSinks();
    Check(logger.SinkCount() == 0, "移除全部输出端");

    // 轮转（spdlog rotating sink 的备份命名为 rot.1.log / rot.2.log）
    const std::string rotatePath = "tests/tmp_utils_test/rot.log";
    const std::string rotatePath1 = "tests/tmp_utils_test/rot.1.log";
    const std::string rotatePath2 = "tests/tmp_utils_test/rot.2.log";
    std::remove(rotatePath.c_str());
    std::remove(rotatePath1.c_str());
    std::remove(rotatePath2.c_str());
    {
        logging::Logger rotating;
        rotating.SetLevel(logging::Level::Trace);
        rotating.SetPattern("{message}");
        logging::FileSink::Options options;
        options.path = rotatePath;
        options.append = false;
        options.flushEveryWrite = true;
        options.maxBytes = 160;
        options.maxFiles = 2;
        rotating.AddSink(std::make_shared<logging::FileSink>(options));
        for (int i = 0; i < 12; ++i) {
            LOG_INFO_TO(rotating, "0123456789012345678901234567890123456789 #{}", i);
        }
        rotating.Flush();
    }
    Check(FileExists(rotatePath) && FileExists(rotatePath1), "按大小轮转生成 .1 备份");

    // 默认日志器 + 多种 pattern 占位符
    logging::Logger patterned;
    patterned.SetPattern("{datetime} {level} {file} {line} {message}");
    std::string line;
    patterned.AddSink(std::make_shared<logging::CallbackSink>(
        [&line](const logging::Record&, std::string_view text) { line = std::string(text); }));
    patterned.SetLevel(logging::Level::Debug);
    patterned.Log(logging::Level::Debug, "some/dir/file.cpp", 42, "hello");
    Check(line.find("file.cpp") != std::string::npos && line.find(" 42 ") != std::string::npos,
          "{file} 只取文件名、{line} 正确");
    Check(line.find("DEBUG") != std::string::npos && line.find("hello") != std::string::npos,
          "{level} / {message} 正确");

    std::remove(logPath.c_str());
    std::remove(rotatePath.c_str());
    std::remove(rotatePath1.c_str());
    std::remove(rotatePath2.c_str());
    REMOVE_DIR("tests/tmp_utils_test");
}

}  // namespace

int main() {
    TestI18n();
    TestDataPaths();
    TestLogger();
    std::printf("\n%s（失败 %d 项）\n", g_failures == 0 ? "ALL PASS" : "FAILED", g_failures);
    return g_failures == 0 ? 0 : 1;
}
