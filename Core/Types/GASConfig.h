#pragma once

#include <cstdint> // 引入 uint64_t 等

// =========================================================
// 1. 文件系统配置 (File System Config)
// =========================================================

/**
 * @namespace GAS_CONFIG
 * @brief 存储系统级的常量和路径配置
 */
namespace GAS_CONFIG
{
    // 资产缓存目录：所有生成的 .gas 二进制文件存放的根目录。
    // 建议使用绝对路径或相对于引擎根目录的路径。
    // 在 Windows 上使用反斜杠 '\\'，在跨平台中建议使用 C++17 的 std::filesystem/std::path 库。
    // 这里使用简单字符串，假定运行时路径处理会处理斜杠问题。
    constexpr const char* BINARY_CACHE_PATH = "Assets/GAS_Cache/Binaries/";

    // 数据库文件路径：SQLite 数据库文件存放的位置。
    constexpr const char* DATABASE_PATH = "Assets/GAS_Cache/Metadata.db";

    // 导入源文件临时目录 (可选，用于存储导入的原始文件备份)
    constexpr const char* SOURCE_ARCHIVE_PATH = "Assets/GAS_Cache/Sources/";
}

// =========================================================
// 2. 核心常量与标识符 (Core Constants)
// =========================================================


constexpr float DEFAULT_ANIMATION_FPS = 30.0f;

/**
 * @brief GPU蒙皮最大骨骼数量
 * 这是硬件限制或性能约束，用于 Importer 阶段的预检查。
 * 超过此数量的骨骼可能需要进行 LOD 或截断处理。
 */
constexpr uint32_t MAX_GPU_BONES = 128; // 常见限制，可根据目标硬件调整