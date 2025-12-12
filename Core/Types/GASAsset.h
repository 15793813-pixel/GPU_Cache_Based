#pragma once

#include "GASCoreTypes.h"
#include "GASArray.h"
#include <string>
#include <unordered_map>
#include <memory>



class GASAsset
{
public:
    virtual ~GASAsset() = default;

    /** 获取资产的全局唯一标识 */
    uint64_t GetGUID() const { return BaseHeader.AssetGUID; }

    /** 获取资产类型 */
    EGASAssetType GetType() const { return static_cast<EGASAssetType>(BaseHeader.AssetType); }

    /** 检查是否有效 */
    virtual bool IsValid() const { return BaseHeader.Magic == GAS_FILE_MAGIC; }

public:
    // 基础头部信息 (所有子类都包含)
    FGASAssetHeader BaseHeader;

    // 调试/编辑器用：资产的文件路径或名称
    std::string AssetName;
};

// 2. 骨骼资产 (Skeleton Asset)
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

    //名称查找骨骼索引 (O(1) 复杂度)
    int32_t FindBoneIndex(const std::string& Name) const
    {
        auto It = BoneNameToIndexMap.find(Name);
        return (It != BoneNameToIndexMap.end()) ? It->second : -1;
    }

    /** 获取父骨骼索引 */
    int32_t GetParentIndex(int32_t BoneIndex) const
    {
        if (Bones.IsValidIndex(BoneIndex))
        {
            return Bones[BoneIndex].ParentIndex;
        }
        return -1;
    }

    /** 获取骨骼数量 */
    int32_t GetNumBones() const { return Bones.Num(); }

    /** 获取特定骨骼的逆绑定矩阵 (用于蒙皮) */
    const FGASMatrix4x4& GetInverseBindMatrix(int32_t BoneIndex) const
    {
        // 这里的 assert 用于开发期调试，确保没有越界访问
        assert(Bones.IsValidIndex(BoneIndex));
        return Bones[BoneIndex].InverseBindMatrix;
    }

public:
    // 具体的骨骼头部信息 (包含 BoneCount 等)
    FGASSkeletonHeader SkeletonHeader;

    // 骨骼定义数据数组
    GASArray<FGASBoneDefinition> Bones;

private:
    // 运行时加速结构：骨骼名称 -> 索引的映射
    // 不序列化到磁盘，Load 后重建
    std::unordered_map<std::string, int32_t> BoneNameToIndexMap;
};


// 3. 动画资产 (Animation Asset)

class GASAnimation : public GASAsset
{
public:
    GASAnimation() = default;

    /**
     * 获取特定骨骼在特定帧的局部变换
     * @param FrameIndex 帧索引 [0, FrameCount-1]
     * @param TrackIndex 轨道索引 (通常对应骨骼索引) [0, TrackCount-1]
     */
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

    /** 获取总帧数 */
    int32_t GetNumFrames() const { return AnimHeader.FrameCount; }

    /** 获取总时长 (秒) */
    float GetDuration() const { return AnimHeader.Duration; }

    /** 获取每秒帧率 */
    float GetFrameRate() const { return AnimHeader.FrameRate; }

public:
    // 具体的动画头部信息
    FGASAnimationHeader AnimHeader;

    // 巨大的扁平化动画数据数组
    // 大小 = FrameCount * TrackCount
    GASArray<FGASAnimTrackData> Tracks;
};