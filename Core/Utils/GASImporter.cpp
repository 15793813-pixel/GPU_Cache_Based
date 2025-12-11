#pragma once
#include "GASImporter.h"
#include "GASDataConverter.h" // 刚才写的工具类
#include "GASLogging.h"           // 假设你有日志类

// Assimp Headers
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

GASImporter::GASImporter() {}
GASImporter::~GASImporter() {}

bool GASImporter::ImportFromFile(const std::string& FilePath,
    std::shared_ptr<GASSkeleton>& OutSkeleton,
    std::vector<std::shared_ptr<GASAnimation>>& OutAnimations)
{
    Assimp::Importer Importer;

    // 设置导入标志位
    // 1. Triangulate: 保证所有网格是三角形
    // 2. LimitBoneWeights: 限制每个顶点最多4根骨骼 (这是GPU蒙皮的标准限制)
    // 3. ConvertToLeftHanded: 自动处理大部分坐标系转换 (Z反转, 面剔除顺序等)
    //    配合我们自己的 GASDataConverter 做类型转换
    const unsigned int Flags = aiProcess_Triangulate |
        aiProcess_LimitBoneWeights |
        aiProcess_GenSmoothNormals |
        aiProcess_ConvertToLeftHanded |
        aiProcess_PopulateArmatureData; // 这一步很重要，帮助Assimp识别骨架

    const aiScene* Scene = Importer.ReadFile(FilePath, Flags);

    if (!Scene || Scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !Scene->mRootNode)
    {
        // GAS_LOG_ERROR("Assimp Import Failed: %s", Importer.GetErrorString());
        return false;
    }

    // 1. 创建骨骼对象
    OutSkeleton = std::make_shared<GASSkeleton>();
    OutSkeleton->AssetName = FilePath; // 简单起见用路径做名字

    // 2. 处理骨骼 (这是后续动画的基础)
    if (!ProcessSkeleton(Scene, OutSkeleton.get()))
    {
        return false;
    }

    // 3. 处理动画
    if (Scene->mNumAnimations > 0)
    {
        ProcessAnimations(Scene, OutSkeleton.get(), OutAnimations);
    }

    return true;
}

// =========================================================
// 骨骼处理逻辑
// =========================================================

bool GASImporter::ProcessSkeleton(const aiScene* Scene, GASSkeleton* TargetSkeleton)
{
    ValidBoneMap.clear();
    InverseBindMatrixMap.clear();

    // 步骤 A: 扫描所有 Mesh，收集真正有权重的骨骼信息
    // Assimp 把 IBM (Inverse Bind Matrix) 存在 Mesh 里
    if (Scene->HasMeshes())
    {
        for (unsigned int i = 0; i < Scene->mNumMeshes; ++i)
        {
            const aiMesh* Mesh = Scene->mMeshes[i];
            for (unsigned int b = 0; b < Mesh->mNumBones; ++b)
            {
                const aiBone* Bone = Mesh->mBones[b];
                std::string BoneName = Bone->mName.C_Str();

                // 规范化名称 (使用 GASDataConverter)
                std::string NormalizedName = GASDataConverter::NormalizeBoneName(BoneName);

                // 标记为有效骨骼
                ValidBoneMap[NormalizedName] = true;

                // 存储逆绑定矩阵 (Assimp 也是 Row-Major，可以直接转)
                InverseBindMatrixMap[NormalizedName] = GASDataConverter::ToMatrix4x4(Bone->mOffsetMatrix);
            }
        }
    }

    // 步骤 B: 递归遍历 Node 树，构建线性骨骼数组
    // 从 Root Node 开始
    RecursivelyProcessBoneNode(Scene->mRootNode, -1, TargetSkeleton);

    // 步骤 C: 完成构建，更新 Header
    TargetSkeleton->SkeletonHeader.BoneCount = TargetSkeleton->Bones.Num();
    TargetSkeleton->RebuildBoneMap(); // 构建 Name->Index 查找表

    return TargetSkeleton->Bones.Num() > 0;
}

void GASImporter::RecursivelyProcessBoneNode(const aiNode* Node, int32_t ParentBoneIndex, GASSkeleton* TargetSkeleton)
{
    std::string NodeName = Node->mName.C_Str();
    std::string NormalizedName = GASDataConverter::NormalizeBoneName(NodeName);

    int32_t CurrentBoneIndex = -1;

    // 检查这个节点是不是我们在 Mesh 中发现的骨骼，或者它是否是骨架结构的一部分
    // (有时候 Root 节点不是 Bone，但我们需要它作为层级根)
    // 简单的判定逻辑：如果它在 ValidMap 里，或者是有效的父节点，或者是Root
    bool bIsBone = (ValidBoneMap.find(NormalizedName) != ValidBoneMap.end());

    // 如果是骨骼，或者是必须保留的层级节点，则加入 Skeleton
    if (bIsBone || ParentBoneIndex != -1) // 只要父节点是骨骼，我们也默认保留子节点以维持层级
    {
        FGASBoneDefinition NewBone;
        SetGASBoneName(NewBone, NodeName.c_str());
        NewBone.ParentIndex = ParentBoneIndex;

        // 设置逆绑定矩阵 (如果不是 Skinned Bone，比如末端节点，可能没有 IBM，设为 Identity)
        if (InverseBindMatrixMap.count(NormalizedName))
        {
            NewBone.InverseBindMatrix = InverseBindMatrixMap[NormalizedName];
        }
        else
        {
            // 默认单位矩阵
            NewBone.InverseBindMatrix = FGASMatrix4x4(); // Identity
        }

        // 记录索引
        CurrentBoneIndex = TargetSkeleton->Bones.Num();

        // 这里的 Add 假设你的 GASArray 有 Add 方法，或者用 std::vector 的 push_back
        // TargetSkeleton->Bones.push_back(NewBone); 
        // 适配 GASArray (假设你需要手动扩容或者有 Add)
        // 这里演示逻辑：
        // TargetSkeleton->Bones.Add(NewBone); 
        // 为了演示完整性，我们假设 GASArray 是类似 std::vector 的封装：
        // 实际上需要根据 GASArray 实现来写，这里伪代码：
        TargetSkeleton->Bones.Resize(CurrentBoneIndex + 1);
        TargetSkeleton->Bones[CurrentBoneIndex] = NewBone;
    }

    // 递归处理子节点
    for (unsigned int i = 0; i < Node->mNumChildren; ++i)
    {
        RecursivelyProcessBoneNode(Node->mChildren[i], CurrentBoneIndex, TargetSkeleton);
    }
}

// =========================================================
// 动画处理逻辑 (Baking)
// =========================================================

bool GASImporter::ProcessAnimations(const aiScene* Scene, const GASSkeleton* Skeleton, std::vector<std::shared_ptr<GASAnimation>>& TargetAnimList)
{
    for (unsigned int i = 0; i < Scene->mNumAnimations; ++i)
    {
        const aiAnimation* SrcAnim = Scene->mAnimations[i];
        auto NewAnim = std::make_shared<GASAnimation>();

        // 1. 设置头部信息
        // Assimp 的 Duration 是 Ticks，需要除以 TicksPerSecond 得到秒
        double TicksPerSecond = (SrcAnim->mTicksPerSecond != 0) ? SrcAnim->mTicksPerSecond : 25.0;
        NewAnim->AnimHeader.Duration = (float)(SrcAnim->mDuration / TicksPerSecond);
        NewAnim->AnimHeader.FrameRate = 30.0f; // 目标采样率：固定 30 FPS 或 60 FPS
        NewAnim->AnimHeader.TrackCount = Skeleton->GetNumBones(); // Track 数量必须严格等于骨骼数量

        // 计算总帧数
        int32_t FrameCount = (int32_t)(NewAnim->AnimHeader.Duration * NewAnim->AnimHeader.FrameRate) + 1;
        NewAnim->AnimHeader.FrameCount = FrameCount;

        // 2. 准备数据容器
        // 大小 = 帧数 * 骨骼数
        int32_t TotalDataSize = FrameCount * NewAnim->AnimHeader.TrackCount;
        NewAnim->Tracks.Resize(TotalDataSize);

        // 3. 建立 BoneName -> aiNodeAnim* 的映射，加速查找
        std::map<std::string, const aiNodeAnim*> NodeAnimMap;
        for (unsigned int ch = 0; ch < SrcAnim->mNumChannels; ++ch)
        {
            std::string NodeName = GASDataConverter::NormalizeBoneName(SrcAnim->mChannels[ch]->mNodeName.C_Str());
            NodeAnimMap[NodeName] = SrcAnim->mChannels[ch];
        }

        // 4. 重采样循环 (Bake)
        double TimePerFrame = 1.0 / NewAnim->AnimHeader.FrameRate; // 每一帧的时间间隔 (秒)

        for (int32_t Frame = 0; Frame < FrameCount; ++Frame)
        {
            // 当前采样时间点 (转回 Assimp 的 Ticks 单位)
            double AnimTimeSec = Frame * TimePerFrame;
            double AnimTimeTicks = AnimTimeSec * TicksPerSecond;

            // 确保不超过动画时长
            if (AnimTimeTicks > SrcAnim->mDuration) AnimTimeTicks = SrcAnim->mDuration;

            // 遍历每一根骨骼
            for (int32_t BoneIdx = 0; BoneIdx < Skeleton->GetNumBones(); ++BoneIdx)
            {
                // 获取骨骼定义
                const std::string& RawBoneName = Skeleton->Bones[BoneIdx].Name;
                std::string NormalizedName = GASDataConverter::NormalizeBoneName(RawBoneName);

                FGASAnimTrackData TrackData;

                // 查找该骨骼是否有动画数据
                auto It = NodeAnimMap.find(NormalizedName);
                if (It != NodeAnimMap.end())
                {
                    // 有动画：进行插值采样
                    FGASTransform LocalTransform;
                    EvaluateChannel(It->second, AnimTimeTicks, LocalTransform);
                    TrackData.LocalTransform = LocalTransform;
                }
                else
                {
                    // 无动画：使用默认姿态 (Identity 或 BindPose)
                    // 这里的 LocalTransform 应该是相对于父骨骼的
                    // 如果 Assimp 没有动画数据，通常意味着保持 BindPose
                    // 简化处理：设为 Identity (或者你需要从 aiNode 读取 LocalTransform 作为默认值)
                    TrackData.LocalTransform.Scale = FGASVector3(1, 1, 1);
                    TrackData.LocalTransform.Translation = FGASVector3(0, 0, 0);
                    TrackData.LocalTransform.Rotation = FGASQuaternion(0, 0, 0, 1);
                }

                // 存入扁平化数组
                // Index = Frame * NumBones + BoneIndex
                int32_t GlobalIndex = Frame * NewAnim->AnimHeader.TrackCount + BoneIdx;
                NewAnim->Tracks[GlobalIndex] = TrackData;
            }
        }

        TargetAnimList.push_back(NewAnim);
    }

    return true;
}

// 简单的线性插值查找
void GASImporter::EvaluateChannel(const aiNodeAnim* Channel, double Time, FGASTransform& OutTransform)
{
    // Assimp 的 Key 可能很少，我们需要找到 Time 前后的两个 Key 进行插值

    // 1. 位置 (Position)
    aiVector3D ResultPos;
    if (Channel->mNumPositionKeys > 0)
    {
        // 简单查找：这里应该用二分查找优化，为了代码简洁先略过
        // 假设 Assimp 提供了 helper 或者我们手写
        // 这里为了演示，假设只有一个 key
        // 实际上需要实现 FindPositionKey(Time) -> index
        // 然后 Lerp(Keys[index], Keys[index+1], factor)

        // 占位逻辑：取第一帧 (实际开发需要完善插值)
        ResultPos = Channel->mPositionKeys[0].mValue;

        // 正确的逻辑结构提示：
        /*
        unsigned int Index = 0;
        for (; Index < Channel->mNumPositionKeys - 1; Index++) {
            if (Time < Channel->mPositionKeys[Index + 1].mTime) break;
        }
        // 计算 Factor 并 Lerp...
        */
    }

    // 2. 旋转 (Rotation)
    aiQuaternion ResultRot;
    if (Channel->mNumRotationKeys > 0)
    {
        ResultRot = Channel->mRotationKeys[0].mValue; // 占位
    }

    // 3. 缩放 (Scale)
    aiVector3D ResultScale(1, 1, 1);
    if (Channel->mNumScalingKeys > 0)
    {
        ResultScale = Channel->mScalingKeys[0].mValue; // 占位
    }

    // 4. 转换到 GAS 类型
    OutTransform.Translation = GASDataConverter::ToVector3(ResultPos);
    OutTransform.Rotation = GASDataConverter::ToQuaternion(ResultRot);
    OutTransform.Scale = GASDataConverter::ToVector3(ResultScale);
}