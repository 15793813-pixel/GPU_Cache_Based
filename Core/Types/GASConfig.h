#pragma once
#include <cstdint> 


// 文件系统配置 
namespace GAS_CONFIG
{
    // 资产缓存目录：所有生成的 .gas 二进制文件存放的根目录。
    constexpr const char* BINARY_CACHE_PATH = "Assets\\GAS_Cache\\Binaries\\";

    // 数据库文件路径：SQLite 数据库文件存放的位置。
    constexpr const char* DATABASE_PATH = "Assets\\GAS_Cache\\Metadata.db";

    // 导入源文件临时目录 (可选，用于存储导入的原始文件备份)
    constexpr const char* SOURCE_ARCHIVE_PATH = "Assets\\GAS_Cache\\Sources\\";
}

// 核心常量与标识符 
constexpr float DEFAULT_ANIMATION_FPS = 30.0f;

// GPU蒙皮最大骨骼数量
constexpr uint32_t MAX_GPU_BONES = 128; // 常见限制，可根据目标硬件调整