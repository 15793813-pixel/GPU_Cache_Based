#pragma once

#include <cstdint>  //定义了固定字节数的数据
#include <cstring>  //定义了内存操作

// 文件魔数: "GASF" (General Animation System File)用于在读取二进制文件时校验这是否是本系统的合法文件
static const uint32_t GAS_FILE_MAGIC = 0x46534147;
//文件版本
static const uint32_t GAS_FILE_VERSION = 1;
// 最大骨骼名称长度 (定长，方便二进制读写)
static const int32_t GAS_MAX_BONE_NAME_LEN = 64;


enum class EGASAssetType : uint32_t
{
    Unknown = 0,
    Skeleton = 1,   // 骨骼
    Animation = 2,  // 动画
};

struct FGASVector3
{
    float X, Y, Z;

    FGASVector3() : X(0.f), Y(0.f), Z(0.f) {}
    FGASVector3(float InX, float InY, float InZ) : X(InX), Y(InY), Z(InZ) {}
};

struct FGASQuaternion
{
    float X, Y, Z, W;

    FGASQuaternion() : X(0.f), Y(0.f), Z(0.f), W(1.f) {} // 默认为单位四元数
    FGASQuaternion(float InX, float InY, float InZ, float InW) : X(InX), Y(InY), Z(InZ), W(InW) {}
};

struct FGASMatrix4x4
{
    float M[4][4];

    FGASMatrix4x4()
    {
        SetIdentity();
    }

    void SetIdentity()
    {
        // 简单的单位矩阵初始化
        std::memset(M, 0, sizeof(M));
        M[0][0] = M[1][1] = M[2][2] = M[3][3] = 1.0f;
    }
};

//12+16+12=40B
struct FGASTransform
{
    FGASVector3 Translation;
    FGASQuaternion Rotation;
    FGASVector3 Scale;

    FGASTransform()
        : Scale(1.f, 1.f, 1.f) // 缩放默认为 1
    {}
};

struct FGASAssetHeader
{
    uint32_t Magic;         // [0-3]   校验码 "GASF"
    uint32_t Version;       // [4-7]   文件版本号

    // 资产本身的 GUID
    uint64_t AssetGUID;     // [8-15]  8字节对齐

    // 资产类型
    EGASAssetType AssetType; // [16-19]
    // 通用标志位 (Bitmask)
    // Bit 0: IsCompressed (是否压缩)
    // Bit 1: IsCooked (是否已针对特定平台优化)
    uint32_t Flags;         // [20-23]

    // 头部总大小 (字节)
    // 读取器应根据此值跳转到数据区，而不是 sizeof(FGASAssetHeader)
    // 这允许未来版本增加 Header 字段而不破坏旧版读取器
    uint32_t HeaderSize;    // [24-27]

    // 数据区大小 (字节)
    // Header 之后紧跟的数据体长度，用于快速内存分配
    uint32_t DataSize;      // [28-31]

    // 预留空间，保证 Header 总大小对齐到 64 字节 (Cache Line Friendly)
    // 当前已用 32 字节，剩余 32 字节
    uint32_t Reserved[8];   // [32-63]
};

// =========================================================
// 3. 骨骼资产 (Skeleton Asset)
// 作用：只存拓扑结构，不存动画。被多个动画复用。
// =========================================================

struct FGASSkeletonHeader : public FGASAssetHeader
{
    // 骨骼数量
    uint32_t BoneCount;
    // 预留
    uint32_t Padding[3];
};

/** 单个骨骼的定义 */
struct FGASBoneDefinition
{
    char Name[GAS_MAX_BONE_NAME_LEN];
    int32_t ParentIndex;
    FGASMatrix4x4 InverseBindMatrix; // 蒙皮用，快速将一个骨骼移到原点的函数，比如说，小臂在005-0010范围内，想要让它旋转50度，先放到原点，旋转50度，再变回来，这就是放回原点的矩阵
    FGASTransform LocalBindPose;     // 默认姿态，相对于父骨骼的相对变换
};

// 骨骼文件的布局逻辑：
// [FGASSkeletonHeader]
// [FGASBoneDefinition * BoneCount]


struct FGASAnimationHeader : public FGASAssetHeader
{
    // [关键修改] 引用所属骨骼的 GUID
    // 运行时加载动画时，必须检查当前 Mesh 的 Skeleton GUID 是否匹配
    uint64_t TargetSkeletonGUID;

    // 动画信息
    uint32_t FrameCount;    // 总帧数
    uint32_t TrackCount;    // 轨道数
    float FrameRate;        // 帧率
    float Duration;         // 时长
};
//轨道：动态骨骼数，每一帧的要记录的数据数，这里设置为固定值，方便实例化时取值


/** 单帧数据 */
struct FGASAnimTrackData
{
    FGASTransform LocalTransform;
};

// 动画文件的布局逻辑：
// [FGASAnimationHeader]
// [FGASAnimTrackData * (FrameCount * TrackCount)] 




