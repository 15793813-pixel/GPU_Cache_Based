#pragma once

#include <cstdint>

// 资产管理相关枚举
enum class EGASAssetType : uint32_t
{
    Unknown = 0,
    Skeleton = 1,   // 骨骼
    Animation = 2,  // 动画
    Mesh = 3,       // 静态网格

};

/**
 * 导入结果/错误码
 */
enum class EGASImportResult : uint8_t
{
    Success = 0,
    FileNotFound,
    FileCorrupted,
    UnsupportedFormat,
    NoSkeletonFound,
    NoAnimationFound,
    BoneCountExceeded,
    UnknownError
};

// =========================================================
// 数学与坐标系相关枚举
// =========================================================

enum class EGASAxis : uint8_t
{
    X = 0, Y = 1, Z = 2,
    NegativeX = 3, NegativeY = 4, NegativeZ = 5
};

enum class EGASLoopMode : uint8_t
{
    Once, Loop, PingPong, Clamp
    //pingpong是反向播放回去再正向回来
};

enum class EGASTextureFormat : uint8_t
{
    RGBA_Float32, RGBA_Half16, RGB_8_Unorm
};