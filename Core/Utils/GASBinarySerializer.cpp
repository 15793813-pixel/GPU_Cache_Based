#include "GASBinarySerializer.h"
#include <fstream>
#include <iostream>
#include "GASLogging.h"

// =========================================================
// 内部 I/O 辅助函数
// =========================================================

bool GASBinarySerializer::WriteData(std::ofstream& Stream, const void* Data, size_t Size)
{
    if (!Stream.write(reinterpret_cast<const char*>(Data), Size))
    {
        // GAS_LOG_ERROR("Binary Write Failed.");
        return false;
    }
    return true;
}

bool GASBinarySerializer::ReadData(std::ifstream& Stream, void* Data, size_t Size)
{
    if (!Stream.read(reinterpret_cast<char*>(Data), Size))
    {
        // GAS_LOG_ERROR("Binary Read Failed. File corrupted or unexpected EOF.");
        return false;
    }
    return true;
}

// =========================================================
// 序列化 (写) 实现
// =========================================================

bool GASBinarySerializer::SerializeSkeleton(std::ofstream& Stream, const GASSkeleton* Skeleton)
{
    // 1. 写入 Skeleton 头部 (包含 BoneCount)
    if (!WriteData(Stream, &Skeleton->SkeletonHeader, sizeof(FGASSkeletonHeader))) return false;

    // 2. 写入 Bones 数组
    // 骨骼定义 FGASBoneDefinition 包含一个 std::string Name，必须手动序列化
    for (int32_t i = 0; i < Skeleton->Bones.Num(); ++i)
    {
        const FGASBoneDefinition& Bone = Skeleton->Bones[i];

        // 写入名称长度和名称数据 (非 POD 结构必须手动处理)
        uint32_t NameLength = (uint32_t)strlen(Bone.Name);
        if (!WriteData(Stream, &NameLength, sizeof(uint32_t))) return false;
        if (!WriteData(Stream, Bone.Name, NameLength)) return false;

        // 写入 BoneDefinition 的其余 POD 部分 (ParentIndex, IBM)
        // 假设 FGASBoneDefinition 有一个用于序列化的 POD 结构体，这里为了简化，我们只写 ParentIndex 和 IBM
        if (!WriteData(Stream, &Bone.ParentIndex, sizeof(int32_t))) return false;
        if (!WriteData(Stream, &Bone.InverseBindMatrix, sizeof(FGASMatrix4x4))) return false;
    }

    // 注意：这里的序列化是非零拷贝的，因为包含 std::string。
    // 对于真正的零拷贝，FGASBoneDefinition 的 Name 应该是一个固定长度的 char 数组，或者 Name 列表单独存放在文件末尾。

    return true;
}

bool GASBinarySerializer::SerializeAnimation(std::ofstream& Stream, const GASAnimation* Animation)
{
    // 1. 写入 Animation 头部
    if (!WriteData(Stream, &Animation->AnimHeader, sizeof(FGASAnimationHeader))) return false;

    // 2. 写入 Tracks 数组
    // Tracks 数组是核心数据，且是 POD 类型（包含 FGASTransform），可以零拷贝式写入
    size_t TotalSize = Animation->Tracks.Num() * sizeof(FGASAnimTrackData);
    if (TotalSize > 0)
    {
        if (!WriteData(Stream, Animation->Tracks.GetData(), TotalSize)) return false;
    }

    return true;
}


bool GASBinarySerializer::SaveAssetToDisk(const GASAsset* Asset, const std::string& FilePath)
{
    std::ofstream FileStream(FilePath, std::ios::binary);
    if (!FileStream.is_open())
    {
        // GAS_LOG_ERROR("Failed to open file for writing: %s", FilePath.c_str());
        return false;
    }

    // 1. 写入通用头部
    if (!WriteData(FileStream, &Asset->BaseHeader, sizeof(FGASAssetHeader))) return false;

    // 2. 写入特定数据
    switch (Asset->GetType())
    {
    case EGASAssetType::Skeleton:
        return SerializeSkeleton(FileStream, static_cast<const GASSkeleton*>(Asset));
    case EGASAssetType::Animation:
        return SerializeAnimation(FileStream, static_cast<const GASAnimation*>(Asset));
    default:
        // GAS_LOG_ERROR("Unknown Asset Type.");
        return false;
    }
}

// =========================================================
// 反序列化 (读) 实现
// =========================================================

bool GASBinarySerializer::DeserializeSkeleton(std::ifstream& Stream, GASSkeleton* Skeleton)
{
    // 1. 读取 Skeleton 头部
    if (!ReadData(Stream, &Skeleton->SkeletonHeader, sizeof(FGASSkeletonHeader))) return false;
    int32_t BoneCount = Skeleton->SkeletonHeader.BoneCount;

    // 2. 读取 Bones 数组
    Skeleton->Bones.Resize(BoneCount);
    for (int32_t i = 0; i < BoneCount; ++i)
    {
        FGASBoneDefinition& Bone = Skeleton->Bones[i];

        // 读取名称长度和名称数据
        uint32_t NameLength;
        if (!ReadData(Stream, &NameLength, sizeof(uint32_t))) return false;

        std::string NameBuffer(NameLength, '\0');
        if (!ReadData(Stream, NameBuffer.data(), NameLength)) return false;
        SetGASBoneName(Bone, NameBuffer.c_str());

        // 读取 BoneDefinition 的其余 POD 部分
        if (!ReadData(Stream, &Bone.ParentIndex, sizeof(int32_t))) return false;
        if (!ReadData(Stream, &Bone.InverseBindMatrix, sizeof(FGASMatrix4x4))) return false;
    }

    // 3. 重建运行时加速结构
    Skeleton->RebuildBoneMap();

    return true;
}

bool GASBinarySerializer::DeserializeAnimation(std::ifstream& Stream, GASAnimation* Animation)
{
    // 1. 读取 Animation 头部
    if (!ReadData(Stream, &Animation->AnimHeader, sizeof(FGASAnimationHeader))) return false;

    // 2. 核心：计算 Tracks 数组大小
    int32_t TotalTracks = Animation->AnimHeader.FrameCount * Animation->AnimHeader.TrackCount;
    Animation->Tracks.Resize(TotalTracks);

    // 3. 零拷贝读取 Tracks 数组 (因为 FGASAnimTrackData 是 POD 类型)
    size_t TotalSize = TotalTracks * sizeof(FGASAnimTrackData);
    if (TotalSize > 0)
    {
        // 直接将文件内容读取到连续内存块中
        if (!ReadData(Stream, Animation->Tracks.GetData(), TotalSize)) return false;
    }

    return true;
}


std::shared_ptr<GASAsset> GASBinarySerializer::LoadAssetFromDisk(const std::string& FilePath)
{
    std::ifstream FileStream(FilePath, std::ios::binary);
    if (!FileStream.is_open())
    {
        // GAS_LOG_ERROR("Failed to open file for reading: %s", FilePath.c_str());
        return nullptr;
    }

    // 1. 读取通用头部
    FGASAssetHeader BaseHeader;
    if (!ReadData(FileStream, &BaseHeader, sizeof(FGASAssetHeader))) return nullptr;

    if (BaseHeader.Magic != GAS_FILE_MAGIC)
    {
        // GAS_LOG_ERROR("File Magic Number Mismatch. Not a valid .gas file.");
        return nullptr;
    }

    std::shared_ptr<GASAsset> LoadedAsset = nullptr;

    // 2. 根据类型创建特定资产对象，并反序列化其数据
    switch (static_cast<EGASAssetType>(BaseHeader.AssetType))
    {
    case EGASAssetType::Skeleton:
    {
        auto Skeleton = std::make_shared<GASSkeleton>();
        Skeleton->BaseHeader = BaseHeader;
        if (DeserializeSkeleton(FileStream, Skeleton.get()))
        {
            LoadedAsset = Skeleton;
        }
        break;
    }
    case EGASAssetType::Animation:
    {
        auto Animation = std::make_shared<GASAnimation>();
        Animation->BaseHeader = BaseHeader;
        if (DeserializeAnimation(FileStream, Animation.get()))
        {
            LoadedAsset = Animation;
        }
        break;
    }
    default:
        // GAS_LOG_ERROR("Unknown Asset Type in file header.");
        break;
    }

    return LoadedAsset;
}