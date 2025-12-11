#pragma once

#include <cstdint>
#include <cstring> // for std::memset
#include "GASEnums.h" // 引用枚举定义


// 用于在读取二进制文件时校验这是否是本系统的合法文件
static const uint32_t GAS_FILE_MAGIC = 0x46534147;

// 文件版本号
static const uint32_t GAS_FILE_VERSION = 1;

// 最大骨骼名称长度 (定长，方便二进制读写)
static const int32_t GAS_MAX_BONE_NAME_LEN = 64;

// =========================================================
// 基础数学结构 (POD)
// =========================================================

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

// 内存大小: 12 + 16 + 12 = 40 Bytes
struct FGASTransform
{
    FGASVector3 Translation;
    FGASQuaternion Rotation;
    FGASVector3 Scale;

    FGASTransform()
        : Scale(1.f, 1.f, 1.f) // 缩放默认为 1
    {
    }
};

/**
 * 通用资产头文件
 * 所有 .gas 文件的前 48字节都是这个结构
 */
struct FGASAssetHeader
{
    uint32_t Magic;         // [0-3]   校验码 "GASF"
    uint32_t Version;       // [4-7]   文件版本号

    // 资产本身的 GUID
    uint64_t AssetGUID;     // [8-15]  8字节对齐

    // 资产类型 (来自于 GASEnums.h, 占4字节)
    EGASAssetType AssetType; // [16-19]

    // 通用标志位 (Bitmask)
    // Bit 0: IsCompressed (是否压缩)
    // Bit 1: IsCooked (是否已针对特定平台优化)
    uint32_t Flags;         // [20-23]

    // 头部总大小 (字节)
    // 读取器应根据此值跳转到数据区，而不是 sizeof(FGASAssetHeader)
    uint32_t HeaderSize;    // [24-27]

    // 数据区大小 (字节)
    // Header 之后紧跟的数据体长度，用于快速内存分配
    uint32_t DataSize;      // [28-31]

    // 预留空间，保证 Header 总大小对齐到 64 字节 (Cache Line Friendly)
    // 当前已用 32 字节，剩余 32 字节
    uint32_t Reserved[4];   // [32-47]
};

//16字节
struct FGASSkeletonHeader : public FGASAssetHeader
{
    // 骨骼数量
    uint32_t BoneCount;
    uint32_t BonesReserved[3];
};

/** 单个骨骼的定义 */
struct FGASBoneDefinition
{
    char Name[GAS_MAX_BONE_NAME_LEN];
    int32_t ParentIndex;

    // 蒙皮用逆绑定矩阵 (Inverse Bind Pose Matrix)
    // 作用：将顶点从模型空间变换到骨骼局部空间
    FGASMatrix4x4 InverseBindMatrix;

    // 局部绑定姿态 (Local Bind Pose)
    // 作用：默认姿态，相对于父骨骼的相对变换
    FGASTransform LocalBindPose;
};

inline void SetGASBoneName(FGASBoneDefinition& BoneDef, const char* InName)
{
    if (InName)
    {
        // 1. 使用标准库 strncpy 进行拷贝std::strncpy 会拷贝最多 N 个字符。如果源字符串长度小于 N，剩余部分会自动填充 \0。
        // 如果源字符串长度大于等于 N，它不会自动添加 \0。
        strncpy_s(BoneDef.Name, InName, GAS_MAX_BONE_NAME_LEN);
        // 2. 【关键安全措施】强制将数组最后一位设为 \0
        // 这样即使输入的 InName 超长，也能保证字符串被截断且是合法的 C-Style 字符串
        BoneDef.Name[GAS_MAX_BONE_NAME_LEN - 1] = '\0';
    }
    else
    {
        // 如果传入空指针，将名字置为空字符串
        BoneDef.Name[0] = '\0';
    }
}

/**
 * @brief 设置父骨骼索引
 * @param BoneDef 目标骨骼定义结构体
 * @param InIndex 父骨骼的索引
 */
inline void SetGASBoneParentIndex(FGASBoneDefinition& BoneDef, int32_t InIndex)
{
    BoneDef.ParentIndex = InIndex;
}

// ---------------------------------------------------------
// 骨骼文件的二进制布局逻辑：
// [FGASSkeletonHeader (64 + 16 bytes)]
// [FGASBoneDefinition * BoneCount]
// ---------------------------------------------------------


// =========================================================
// 动画资产 (Animation Asset)
// =========================================================
//48+24+24=96字节
struct FGASAnimationHeader : public FGASAssetHeader
{
    // [关键] 引用所属骨骼的 GUID
    // 运行时加载动画时，必须检查当前 Mesh 的 Skeleton GUID 是否匹配
    uint64_t TargetSkeletonGUID;

    // 动画信息
    uint32_t FrameCount;    // 总帧数
    uint32_t TrackCount;    // 轨道数 (通常等于骨骼数)
    float FrameRate;        // 帧率
    float Duration;         // 时长
    uint32_t AniReserved[6];
};

/** 单帧数据 */
struct FGASAnimTrackData
{
    FGASTransform LocalTransform;
};

// ---------------------------------------------------------
// 动画文件的二进制布局逻辑：
// [FGASAnimationHeader]
// [FGASAnimTrackData * (FrameCount * TrackCount)] 
// 数据排列顺序：[Frame0_Bone0, Frame0_Bone1...], [Frame1_Bone0...]
// ---------------------------------------------------------