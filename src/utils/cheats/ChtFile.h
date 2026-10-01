#pragma once

// 金手指 .cht 解析器（独立模块，不依赖宿主项目）
//
// 兼容 RetroArch 的 .cht（key = value 配置格式，非 JSON）：
//   cheats = 2
//   cheat0_desc = "Infinite Health"
//   cheat0_code = "82003208 0063"
//   cheat0_enable = false
//   cheat0_big_endian = false
//   cheat0_handler = 0            <- 未知字段，默认原样保留
//
// 设计要点：
//   - 只碰文件格式，不碰路径、不建目录、不依赖其它模块（调用方用 src/utils/paths 定位文件）
//   - 多行代码保持 `A+B+C` 原样，前端不解析核心代码（由核心解释）
//   - 无法识别的字段默认原样保留（Options::preserveUnknownKeys），避免改一条就把 RA 写的
//     handler / rumble_* / repeat_* 等元数据抹掉
//   - 读写容错：BOM、CRLF、注释行（# ;）、空行、字段顺序、条目编号跳号都能处理
//
// 迁移到其他项目：复制本目录即可。

#include <cstddef>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace cht {

struct Entry {
    std::string description;  // desc
    std::string code;         // code，多行用 '+' 连接
    bool enabled = false;     // enable
    bool bigEndian = false;   // big_endian
    // 保留的未知字段（原样，含引号），如 handler / cheat_type / rumble_*
    std::vector<std::pair<std::string, std::string>> extra;
};

class File {
public:
    struct Options {
        bool preserveUnknownKeys = true;
    };

    File();
    explicit File(Options options);

    // --- 读写 ---------------------------------------------------------------
    bool Load(std::string_view path);
    bool LoadFromString(std::string_view text, std::string_view source = "<memory>");
    // 写回磁盘；父目录需已存在（目录创建交给 src/utils/paths）
    bool Save(std::string_view path) const;
    // 序列化为 .cht 文本，便于测试或写到别处
    std::string ToString() const;

    // --- 读取 ---------------------------------------------------------------
    std::size_t Size() const;
    bool Empty() const;
    const std::vector<Entry>& Entries() const;
    std::vector<Entry>& Entries();
    const Entry* Get(std::size_t index) const;
    Entry* Get(std::size_t index);

    std::size_t CountEnabled() const;
    int IndexOfCode(std::string_view code) const;

    // --- 修改 ---------------------------------------------------------------
    std::size_t Add(Entry entry);
    std::size_t Add(std::string_view description, std::string_view code, bool enabled = false);
    bool Remove(std::size_t index);
    void Clear();

    bool SetDescription(std::size_t index, std::string_view description);
    bool SetCode(std::size_t index, std::string_view code);
    bool SetEnabled(std::size_t index, bool enabled);
    bool SetBigEndian(std::size_t index, bool bigEndian);
    // 批量开关
    void EnableAll(bool enabled);

    // --- 诊断 ---------------------------------------------------------------
    std::string LastError() const;
    const std::vector<std::string>& Warnings() const;
    void ClearWarnings();

private:
    Options options_;
    std::vector<Entry> entries_;
    std::vector<std::pair<std::string, std::string>> fileExtras_;  // 文件级未知键
    mutable std::string lastError_;
    std::vector<std::string> warnings_;
};

}  // namespace cht
