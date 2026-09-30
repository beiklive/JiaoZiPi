#pragma once

// 机种（平台核心）定义与徽标配色。
//
// 机种主题色规范：以各机种经典硬件外观、品牌标识和玩家记忆为基础的印象色，
// 统一降低饱和度以适配浅色 32-bit 像素风界面。仅定义主色，不使用机身原色还原。
// 配色为单一数据源：配色预览图由 tools/gen_badge_color_preview.py 解析本文件生成，
// 修改颜色后重新运行该脚本即可同步预览。

#include <cstddef>
#include <cstdint>

namespace jzp {

// ---------------------------------------------------------------------------
// 机种枚举
// 数值会写入配置 / 存档 / 核心注册表，新增机种请追加在 Count 之前，
// 不要调整或复用已有数值。
// ---------------------------------------------------------------------------
enum class Machine : uint8_t {
    Unknown = 0,

    // 任天堂
    FC     = 1,   // Family Computer / NES
    SFC    = 2,   // Super Famicom / SNES
    GB     = 3,   // Game Boy
    GBC    = 4,   // Game Boy Color
    GBA    = 5,   // Game Boy Advance
    NDS    = 6,   // Nintendo DS
    N3DS   = 7,   // Nintendo 3DS
    NGC    = 8,   // Nintendo GameCube
    WII    = 9,   // Nintendo Wii

    // 世嘉
    MD     = 10,  // Mega Drive / Genesis
    SS     = 11,  // Sega Saturn
    DC     = 12,  // Dreamcast

    // 索尼
    PS1    = 13,  // PlayStation
    PSP    = 14,  // PlayStation Portable

    // 街机
    ARCADE = 15,

    Count  = 16,
};

inline constexpr std::size_t kMachineCount = static_cast<std::size_t>(Machine::Count);

// 可遍历机种列表（不含 Unknown / Count），用于列表、筛选、批量注册。
constexpr Machine kAllMachines[] = {
    Machine::FC,  Machine::SFC,    Machine::GB,  Machine::GBC, Machine::GBA,
    Machine::NDS, Machine::N3DS,   Machine::NGC, Machine::WII, Machine::MD,
    Machine::SS,  Machine::DC,     Machine::PS1, Machine::PSP, Machine::ARCADE,
};

constexpr bool IsValidMachine(Machine m) {
    return m > Machine::Unknown && m < Machine::Count;
}

// ---------------------------------------------------------------------------
// 颜色
// ---------------------------------------------------------------------------
struct Color {
    uint8_t r = 0;
    uint8_t g = 0;
    uint8_t b = 0;
    uint8_t a = 0xFF;
};

constexpr Color Rgba(uint32_t rgb, uint8_t a = 0xFF) {
    return Color{static_cast<uint8_t>((rgb >> 16) & 0xFF),
                 static_cast<uint8_t>((rgb >> 8) & 0xFF),
                 static_cast<uint8_t>(rgb & 0xFF), a};
}

// 机种徽标底色（徽标文字统一使用白色）。
constexpr Color MachineBadgeColor(Machine m) {
    switch (m) {
        case Machine::FC:     return Rgba(0xE65B59);  // 珊瑚红
        case Machine::SFC:    return Rgba(0x9B91C9);  // 薰衣草紫
        case Machine::GB:     return Rgba(0x9BAA7A);  // 灰橄榄绿
        case Machine::GBC:    return Rgba(0x55B8A1);  // 透明青绿
        case Machine::GBA:    return Rgba(0x8A6FD1);  // 掌机紫（比 NGC 更亮，避免两紫难分）
        case Machine::NDS:    return Rgba(0x77B7D9);  // 天空蓝
        case Machine::N3DS:   return Rgba(0x4E83D4);  // 亮钴蓝
        case Machine::NGC:    return Rgba(0x7665B6);  // 游戏机紫
        case Machine::WII:    return Rgba(0x80C9D8);  // 冰川青
        case Machine::MD:     return Rgba(0x405A92);  // 世嘉蓝
        case Machine::SS:     return Rgba(0xA68ABF);  // 土星紫
        case Machine::DC:     return Rgba(0xE58B68);  // 梦幻橙
        case Machine::PS1:    return Rgba(0xA7A6A0);  // 经典银灰
        case Machine::PSP:    return Rgba(0x747D91);  // 石板灰
        case Machine::ARCADE: return Rgba(0xE6A83D);  // 街机金
        case Machine::Unknown:
        case Machine::Count:
        default:              return Rgba(0x9E9E9E);
    }
}

// 徽标底色打包为 0xAABBGGRR，可直接传给 IM_COL32 / 各图形后端的 RGBA8888。
constexpr uint32_t MachineBadgeColorU32(Machine m) {
    const Color c = MachineBadgeColor(m);
    return (static_cast<uint32_t>(c.a) << 24) | (static_cast<uint32_t>(c.b) << 16) |
           (static_cast<uint32_t>(c.g) << 8) | static_cast<uint32_t>(c.r);
}

constexpr Color kMachineBadgeFallbackColor = Rgba(0x9E9E9E);
constexpr Color kMachineBadgeInkColor = Rgba(0x2B2B2B);   // 浅色徽标上的深色字
constexpr Color kMachineBadgeWhiteColor = Rgba(0xFFFFFF); // 深色徽标上的白色字

// 通道线性化近似（gamma 2.0），返回 0~1000
constexpr uint32_t ChannelLuminance(uint8_t c) {
    return (static_cast<uint32_t>(c) * c * 1000u) / (255u * 255u);
}

// 相对亮度，返回 0~1000
constexpr uint32_t RelativeLuminance(Color c) {
    return (299u * ChannelLuminance(c.r) + 587u * ChannelLuminance(c.g) +
            114u * ChannelLuminance(c.b)) / 1000u;
}

// 对比度（WCAG 公式，亮度已放大 1000 倍）
constexpr uint32_t ContrastRatio1000(uint32_t l1, uint32_t l2) {
    const uint32_t hi = l1 > l2 ? l1 : l2;
    const uint32_t lo = l1 > l2 ? l2 : l1;
    return (hi + 50u) * 1000u / (lo + 50u);
}

// 徽标文字色：浅色印象色用深色字，深色印象色用白字，按对比度自动选择，
// 避免为每个机种单独维护文字色。
constexpr Color MachineBadgeTextColor(Machine m) {
    const uint32_t bg = RelativeLuminance(MachineBadgeColor(m));
    const uint32_t ink = RelativeLuminance(kMachineBadgeInkColor);
    const uint32_t white = RelativeLuminance(kMachineBadgeWhiteColor);
    return ContrastRatio1000(bg, ink) >= ContrastRatio1000(bg, white)
               ? kMachineBadgeInkColor
               : kMachineBadgeWhiteColor;
}

// ---------------------------------------------------------------------------
// 名称
// ---------------------------------------------------------------------------
// 徽标短标签（ASCII，控制在 3 字符左右，避免徽标内换行）。
constexpr const char* MachineLabel(Machine m) {
    switch (m) {
        case Machine::FC:     return "FC";
        case Machine::SFC:    return "SFC";
        case Machine::GB:     return "GB";
        case Machine::GBC:    return "GBC";
        case Machine::GBA:    return "GBA";
        case Machine::NDS:    return "NDS";
        case Machine::N3DS:   return "3DS";
        case Machine::NGC:    return "NGC";
        case Machine::WII:    return "WII";
        case Machine::MD:     return "MD";
        case Machine::SS:     return "SS";
        case Machine::DC:     return "DC";
        case Machine::PS1:    return "PS1";
        case Machine::PSP:    return "PSP";
        case Machine::ARCADE: return "ARCADE";
        case Machine::Unknown:
        case Machine::Count:
        default:              return "?";
    }
}

// 机种全名。
constexpr const char* MachineName(Machine m) {
    switch (m) {
        case Machine::FC:     return "Famicom / NES";
        case Machine::SFC:    return "Super Famicom / SNES";
        case Machine::GB:     return "Game Boy";
        case Machine::GBC:    return "Game Boy Color";
        case Machine::GBA:    return "Game Boy Advance";
        case Machine::NDS:    return "Nintendo DS";
        case Machine::N3DS:   return "Nintendo 3DS";
        case Machine::NGC:    return "Nintendo GameCube";
        case Machine::WII:    return "Nintendo Wii";
        case Machine::MD:     return "Mega Drive / Genesis";
        case Machine::SS:     return "Sega Saturn";
        case Machine::DC:     return "Dreamcast";
        case Machine::PS1:    return "PlayStation";
        case Machine::PSP:    return "PlayStation Portable";
        case Machine::ARCADE: return "Arcade";
        case Machine::Unknown:
        case Machine::Count:
        default:              return "Unknown";
    }
}

}  // namespace jzp
