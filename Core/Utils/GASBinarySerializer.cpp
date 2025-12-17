#include "GASBinarySerializer.h"
#include <fstream>
#include <iostream>
#include <vector>
#include "GASLogging.h"


//辅助：写入字符串(长度 + 内容)
bool WriteString(std::ofstream & Stream, const std::string & Str)
{
    uint32_t Len = static_cast<uint32_t>(Str.size());
    if (!Stream.write(reinterpret_cast<const char*>(&Len), sizeof(uint32_t))) return false;
    if (Len > 0)
    {
        if (!Stream.write(Str.data(), Len)) return false;
    }
    return true;
}

// 辅助：读取字符串 (长度 + 内容)
bool ReadString(std::ifstream& Stream, std::string& OutStr)
{
    uint32_t Len = 0;
    if (!Stream.read(reinterpret_cast<char*>(&Len), sizeof(uint32_t))) return false;
    if (Len > 0)
    {
        OutStr.resize(Len);
        if (!Stream.read(&OutStr[0], Len)) return false;
    }
    else
    {
        OutStr.clear();
    }
    return true;
}


bool GASBinarySerializer::WriteData(std::ofstream& Stream, const void* Data, size_t Size)
{
    if (!Stream.write(reinterpret_cast<const char*>(Data), Size))
    {
        return false;
    }
    return true;
}

//读数据
bool GASBinarySerializer::ReadData(std::ifstream& Stream, void* Data, size_t Size)
{
    if (!Stream.read(reinterpret_cast<char*>(Data), Size))
    {
        return false;
    }
    return true;
}

//保存到磁盘
bool GASBinarySerializer::SaveAssetToDisk(const GASAsset* Asset, const std::string& FilePath)
{
    if (!Asset) return false;

    std::ofstream FileStream(FilePath, std::ios::binary | std::ios::trunc);
    if (!FileStream.is_open())
    {
        GAS_LOG_ERROR("Failed to open file for writing: %s", FilePath.c_str());
        return false;
    }

    if (!WriteData(FileStream, &Asset->BaseHeader, sizeof(FGASAssetHeader)))
    {
        return false;
    }

    EGASAssetType Type = static_cast<EGASAssetType>(Asset->BaseHeader.AssetType);

    switch (Type)
    {
    case EGASAssetType::Skeleton:
        return SerializeSkeleton(FileStream, static_cast<const GASSkeleton*>(Asset));

    case EGASAssetType::Animation:
        return SerializeAnimation(FileStream, static_cast<const GASAnimation*>(Asset));

    case EGASAssetType::Mesh:
        return SerializeMesh(FileStream, static_cast<const GASMesh*>(Asset));

    default:
        GAS_LOG_ERROR("Unknown Asset Type during save. Type: %d", (int)Type);
        return false;
    }
}

//从磁盘读取资产
std::shared_ptr<GASAsset> GASBinarySerializer::LoadAssetFromDisk(const std::string& FilePath)
{
    std::ifstream FileStream(FilePath, std::ios::binary);
    if (!FileStream.is_open())
    {
        GAS_LOG_ERROR("Failed to open file for reading: %s", FilePath.c_str());
        return nullptr;
    }

    FGASAssetHeader Header;
    if (!ReadData(FileStream, &Header, sizeof(FGASAssetHeader)))
    {
        return nullptr;
    }

    if (Header.Magic != GAS_ASSET_MAGIC)
    {
        GAS_LOG_ERROR("Invalid Magic Number in file: %s", FilePath.c_str());
        return nullptr;
    }

    std::shared_ptr<GASAsset> ResultAsset = nullptr;
    EGASAssetType Type = static_cast<EGASAssetType>(Header.AssetType);

    switch (Type)
    {
    case EGASAssetType::Skeleton:
    {
        auto Skeleton = std::make_shared<GASSkeleton>();
        Skeleton->BaseHeader = Header;
        if (DeserializeSkeleton(FileStream, Skeleton.get()))
        {
            ResultAsset = Skeleton;
        }
        break;
    }
    case EGASAssetType::Animation:
    {
        auto Anim = std::make_shared<GASAnimation>();
        Anim->BaseHeader = Header;
        if (DeserializeAnimation(FileStream, Anim.get()))
        {
            ResultAsset = Anim;
        }
        break;
    }
    case EGASAssetType::Mesh:
    {
        auto Mesh = std::make_shared<GASMesh>();
        Mesh->BaseHeader = Header;
        if (DeserializeMesh(FileStream, Mesh.get()))
        {
            ResultAsset = Mesh;
        }
        break;
    }
    default:
        GAS_LOG_ERROR("Unknown Asset Type in header: %d", (int)Type);
        break;
    }

    return ResultAsset;
}

bool GASBinarySerializer::SerializeSkeleton(std::ofstream& Stream, const GASSkeleton* Skeleton)
{
    if (!WriteData(Stream, &Skeleton->SkeletonHeader, sizeof(FGASSkeletonHeader))) return false;

    for (int32_t i = 0; i < Skeleton->Bones.Num(); ++i)
    {
        const FGASBoneDefinition& Bone = Skeleton->Bones[i];

        if (!WriteString(Stream, Bone.Name)) return false;
        if (!WriteData(Stream, &Bone.ParentIndex, sizeof(int32_t))) return false;
        if (!WriteData(Stream, &Bone.InverseBindMatrix, sizeof(FGASMatrix4x4))) return false;
    }
    return true;
}

bool GASBinarySerializer::DeserializeSkeleton(std::ifstream& Stream, GASSkeleton* Skeleton)
{
    //  读 SkeletonHeader
    if (!ReadData(Stream, &Skeleton->SkeletonHeader, sizeof(FGASSkeletonHeader))) return false;

    //  准备内存
    int32_t BoneCount = Skeleton->SkeletonHeader.BoneCount;
    Skeleton->Bones.Resize(BoneCount);

    //  读 Bones 数组
    for (int32_t i = 0; i < BoneCount; ++i)
    {
        FGASBoneDefinition& Bone = Skeleton->Bones[i];

        std::string TempName;
        if (!ReadString(Stream, TempName)) return false;
        size_t CopyLen = TempName.size();
        if (CopyLen > sizeof(Bone.Name) - 1)
        {
            CopyLen = sizeof(Bone.Name) - 1; 
        }
        std::memcpy(Bone.Name, TempName.c_str(), CopyLen);
        Bone.Name[CopyLen] = '\0'; 

        if (!ReadData(Stream, &Bone.ParentIndex, sizeof(int32_t))) return false;
        if (!ReadData(Stream, &Bone.InverseBindMatrix, sizeof(FGASMatrix4x4))) return false; 
    }

    // 重建加速查找表 
    Skeleton->RebuildBoneMap();

    return true;
}

//  动画 (Animation) 实现

bool GASBinarySerializer::SerializeAnimation(std::ofstream& Stream, const GASAnimation* Animation)
{
    //  写 AnimHeader
    if (!WriteData(Stream, &Animation->AnimHeader, sizeof(FGASAnimationHeader))) return false;

    // 写 Tracks 数据
    size_t DataSize = Animation->Tracks.Num() * sizeof(FGASAnimTrackData);
    if (DataSize > 0)
    {
        if (!WriteData(Stream, Animation->Tracks.GetData(), DataSize)) return false;
    }
    return true;
}

bool GASBinarySerializer::DeserializeAnimation(std::ifstream& Stream, GASAnimation* Animation)
{
    //读 AnimHeader
    if (!ReadData(Stream, &Animation->AnimHeader, sizeof(FGASAnimationHeader))) return false;

    // 计算大小并 Resize
    int32_t TotalElements = Animation->AnimHeader.FrameCount * Animation->AnimHeader.TrackCount;
    Animation->Tracks.Resize(TotalElements);

    // 读 Tracks 数据
    size_t DataSize = TotalElements * sizeof(FGASAnimTrackData);
    if (DataSize > 0)
    {
        if (!ReadData(Stream, Animation->Tracks.GetData(), DataSize)) return false;
    }

    return true;
}


// 网格 (Mesh) 实现

bool GASBinarySerializer::SerializeMesh(std::ofstream& Stream, const GASMesh* Mesh)
{
    // MeshHeader
    if (!WriteData(Stream, &Mesh->MeshHeader, sizeof(FGASMeshHeader))) return false;

    // 蒙皮标记 和 SkeletonGUID
    if (!WriteData(Stream, &Mesh->MeshHasSkin, sizeof(bool))) return false;
    if (!WriteData(Stream, &Mesh->SkeletonGUID, sizeof(uint64_t))) return false;

    // 顶点数据 
    size_t VertexDataSize = Mesh->Vertices.Num() * sizeof(FGASSkinVertex);
    if (VertexDataSize > 0)
    {
        if (!WriteData(Stream, Mesh->Vertices.GetData(), VertexDataSize)) return false;
    }

    //写 索引数据 
    size_t IndexDataSize = Mesh->Indices.Num() * sizeof(uint32_t);
    if (IndexDataSize > 0)
    {
        if (!WriteData(Stream, Mesh->Indices.GetData(), IndexDataSize)) return false;
    }

    return true;
}

bool GASBinarySerializer::DeserializeMesh(std::ifstream& Stream, GASMesh* Mesh)
{
    // 读 MeshHeader
    if (!ReadData(Stream, &Mesh->MeshHeader, sizeof(FGASMeshHeader))) return false;

    //  读MeshHasSkin和 SkeletonGUID
    if (!ReadData(Stream, &Mesh->MeshHasSkin, sizeof(bool))) return false;
    if (!ReadData(Stream, &Mesh->SkeletonGUID, sizeof(uint64_t))) return false;

    // 读 顶点数据 根据 Header 里的数量分配内存
    Mesh->Vertices.Resize(Mesh->MeshHeader.NumVertices);
    size_t VertexDataSize = Mesh->Vertices.Num() * sizeof(FGASSkinVertex);
    if (VertexDataSize > 0)
    {
        if (!ReadData(Stream, Mesh->Vertices.GetData(), VertexDataSize)) return false;
    }

    // 读索引数据
    Mesh->Indices.Resize(Mesh->MeshHeader.NumIndices);
    size_t IndexDataSize = Mesh->Indices.Num() * sizeof(uint32_t);
    if (IndexDataSize > 0)
    {
        if (!ReadData(Stream, Mesh->Indices.GetData(), IndexDataSize)) return false;
    }

    return true;
}