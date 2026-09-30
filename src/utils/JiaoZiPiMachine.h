#pragma once

// 机种（平台核心）定义与徽标配色。
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
        case Machine::FC:     return Rgba(0xE60012);
        case Machine::SFC:    return Rgba(0x8E44AD);
        case Machine::GB:     return Rgba(0x6C8E1F);
        case Machine::GBC:    return Rgba(0x00897B);
        case Machine::GBA:    return Rgba(0x3F51B5);
        case Machine::NDS:    return Rgba(0x607D8B);
        case Machine::N3DS:   return Rgba(0xA3121F);
        case Machine::NGC:    return Rgba(0x4527A0);
        case Machine::WII:    return Rgba(0x00A0E9);
        case Machine::MD:     return Rgba(0x0060A8);
        case Machine::SS:     return Rgba(0x455A64);
        case Machine::DC:     return Rgba(0xF57C00);
        case Machine::PS1:    return Rgba(0x0070D1);
        case Machine::PSP:    return Rgba(0x263238);
        case Machine::ARCADE: return Rgba(0xD81B60);
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

constexpr Color kMachineBadgeTextColor = Rgba(0xFFFFFF);
constexpr Color kMachineBadgeFallbackColor = Rgba(0x9E9E9E);

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
