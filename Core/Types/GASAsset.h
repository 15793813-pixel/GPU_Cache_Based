#pragma once
#include "GASArray.h"
#include "GASCoreTypes.h"


class GASAsset
{
public:
    virtual ~GASAsset() = default;

    //获取资产的全局唯一标识
    uint64_t GetGUID() const { return BaseHeader.AssetGUID; }

    // 获取资产类型
    EGASAssetType GetType() const { return static_cast<EGASAssetType>(BaseHeader.AssetType); }

    //检查是否有效 
    virtual bool IsValid() const { return BaseHeader.Magic == GAS_ASSET_MAGIC; }

public:
    // 基础头部信息 (所有子类都包含)
    FGASAssetHeader BaseHeader;

    // 调试/编辑器用：资产的文件路径或名称
    std::string AssetName;
};


// 2. 骨骼资产
class GASSkeleton : public GASAsset
{
public:
    GASSkeleton() = default;


     //构建加速查找表 从二进制加载完 Bones 数组后，必须手动调用一次这个函数
    void RebuildBoneMap()
    {
        BoneNameToIndexMap.clear();
        for (int32_t i = 0; i < Bones.Num(); ++i)
        {
            BoneNameToIndexMap[Bones[i].Name] = i;
        }
    }

    //名称查找骨骼索引 
    int32_t FindBoneIndex(const std::string& Name) const
    {
        auto It = BoneNameToIndexMap.find(Name);
        return (It != BoneNameToIndexMap.end()) ? It->second : -1;
    }

    //获取父骨骼索引
    int32_t GetParentIndex(int32_t BoneIndex) const
    {
        if (Bones.IsValidIndex(BoneIndex))
        {
            return Bones[BoneIndex].ParentIndex;
        }
        return -1;
    }

    //获取骨骼数量 
    int32_t GetNumBones() const { return Bones.Num(); }

    //获取特定骨骼的逆绑定矩阵 (用于蒙皮)
    const FGASMatrix4x4& GetInverseBindMatrix(int32_t BoneIndex) const
    {
        assert(Bones.IsValidIndex(BoneIndex));
        return Bones[BoneIndex].InverseBindMatrix;
    }

public:
    // 具体的骨骼头部信息 (包含 BoneCount 等)
    FGASSkeletonHeader SkeletonHeader;

    // 骨骼定义数据数组
    GASArray<FGASBoneDefinition> Bones;

private:
    // 运行时加速结构：骨骼名称 -> 索引的映射 不序列化到磁盘，Load 后重建
    std::unordered_map<std::string, int32_t> BoneNameToIndexMap;
};


// 3. 动画资产 
class GASAnimation : public GASAsset
{
public:
    GASAnimation() = default;

    
    //获取特定骨骼在特定帧的局部变换
    const FGASTransform* GetTransform(int32_t FrameIndex, int32_t TrackIndex) const
    {
        // 计算扁平化数组中的偏移量
        int32_t TrackCount = AnimHeader.TrackCount;
        int32_t GlobalIndex = (FrameIndex * TrackCount) + TrackIndex;

        if (Tracks.IsValidIndex(GlobalIndex))
        {
            return &Tracks[GlobalIndex].LocalTransform;
        }
        return nullptr;
    }

    //获取总帧数
    int32_t GetNumFrames() const { return AnimHeader.FrameCount; }

    //获取总时长 (秒)
    float GetDuration() const { return AnimHeader.Duration; }

    //获取每秒帧率 
    float GetFrameRate() const { return AnimHeader.FrameRate; }

public:
    // 具体的动画头部信息
    FGASAnimationHeader AnimHeader;

    // 巨大的扁平化动画数据数组 大小 = FrameCount * TrackCount
    GASArray<FGASAnimTrackData> Tracks;
};

//4.网格资产
class GASMesh : public GASAsset
{
public:
    GASMesh()
    {
        // 默认为 Mesh 类型，具体是不是蒙皮由 MeshHasSkin 决定
        BaseHeader.AssetType = EGASAssetType::Mesh;
    }

    /** 获取顶点数量 */
    int32_t GetNumVertices() const { return Vertices.Num(); }

    /** 获取索引数量 */
    int32_t GetNumIndices() const { return Indices.Num(); }

    /** 获取包围盒 (用于剔除) */
    const FGASAABB& GetAABB() const { return MeshHeader.AABB; }

    inline void SetHasSkin(bool Has) { MeshHasSkin = Has; }
    inline bool HasSkin() { return MeshHasSkin; }
public:

    FGASMeshHeader MeshHeader;

    // 关联数据的 GUID 如果 MeshHasSkin=true，这里必须存储有效的 SkeletonGUID
    uint64_t SkeletonGUID = 0;

    // 注意：这里包含了 Position, Normal, UV, *BoneIndices*, *BoneWeights*
    GASArray<FGASSkinVertex> Vertices;

    //  索引数据 认为每三个点组成三角形，直接三个三个读取
    GASArray<uint32_t> Indices;

    bool MeshHasSkin = false;
};