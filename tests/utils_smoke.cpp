// 日志系统 + 多语言系统 冒烟测试
//
// 构建（可直接用 tools/build_utils_test.sh）：
//   c++ -std=c++17 -Wall -Wextra -I src/utils \
//       -I third_party/spdlog/include -I third_party/json/include \
//       src/utils/log/Logger.cpp src/utils/i18n/I18n.cpp tests/utils_smoke.cpp -o tests/utils_smoke
// 运行：在仓库根目录执行 ./tests/utils_smoke（语言文件按 resources/lang 相对路径加载）

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "i18n/I18n.h"
#include "log/Logger.h"

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
    tr.SetFallbackLocale("en_US");

    const std::size_t textLoaded = tr.LoadDirectory("resources/lang", ".lang");
    Check(textLoaded == 2, "加载 resources/lang 下 2 个文本语言表");

    const std::size_t jsonLoaded = tr.LoadDirectory("resources/lang", ".json");
    const bool jsonOk = i18n::Translator::JsonAvailable();
    if (jsonOk) {
        Check(jsonLoaded == 1, "加载 JSON 语言表 ja_JP.json");
    } else {
        Check(jsonLoaded == 0, "未包含 nlohmann/json，JSON 表被跳过（降级为文本表）");
    }
    Check(tr.Locales().size() == (jsonOk ? 3u : 2u), "语言列表数量正确");

    if (jsonOk) {
        Check(tr.SetLocale("ja_JP"), "切换到 ja_JP（JSON 表）");
        Check(tr.Tr("menu.library") == "ライブラリ", "JSON 嵌套对象展平为点分 key");
        Check(tr.Tr("greeting", "プレイヤー") == "こんにちは、プレイヤー", "JSON 表里的占位符同样生效");
    }

    Check(tr.SetLocale("zh_CN"), "切换到 zh_CN");
    Check(tr.Tr("app.name") == "饺子皮", "中文取值正确");
    Check(tr.Tr("greeting", "玩家") == "你好，玩家", "占位符格式化");
    Check(tr.Tr("library.count", 12) == "共 12 个游戏", "多参数/整数格式化");
    Check(tr.Tr("app.build") == "development build", "本语言缺失时回退到 fallback 语言");

    Check(tr.SetLocale("zh_TW"), "切换到 zh_TW（未加载）");
    Check(tr.Tr("menu.library") == "游戏库", "语言前缀回退 zh_TW → zh_CN");

    Check(!tr.SetLocale("ko_KR"), "切换到未支持语言返回 false");
    Check(tr.Tr("menu.settings") == "Settings", "未支持语言回退到 en_US");

    tr.ClearMissingKeys();
    const std::string missing = tr.Tr("no.such.key");
    Check(missing == "no.such.key", "缺失 key 返回 key 本身");
    const std::vector<std::string> keys = tr.MissingKeys();
    Check(keys.size() == 1 && keys.front() == "no.such.key", "记录缺失 key");

    tr.Set("fr_FR", "app.name", "JiaoZiPi FR");
    tr.SetLocale("fr_FR");
    Check(tr.Tr("app.name") == "JiaoZiPi FR", "运行时注册的语言可直接切换");

    i18n::Translator::Default().SetLocale("zh_CN");
    Check(i18n::Tr("app.tagline") == "多核心模拟器前端", "全局便捷接口 i18n::Tr");
    Check(i18n::Tr("menu.cores") == "核心管理", "全局便捷接口：无参数");
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
    TestLogger();
    std::printf("\n%s（失败 %d 项）\n", g_failures == 0 ? "ALL PASS" : "FAILED", g_failures);
    return g_failures == 0 ? 0 : 1;
}
