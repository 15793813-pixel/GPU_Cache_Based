#pragma once
#include "GASImporter.h"
#include "GASDataConverter.h"
#include "GASLogging.h"          
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <filesystem>
#include <assimp/DefaultLogger.hpp>
#include <assimp/LogStream.hpp>
#include "GASHashManager.h"


GASImporter::GASImporter() {}
GASImporter::~GASImporter() {}


struct BoneInfluence
{
    uint32_t Index;
    float Weight;
   // 降序排序，权重大的在前
    bool operator<(const BoneInfluence& other) const {
        return Weight > other.Weight; 
    }
};
// 放在 GASImporter.cpp 的最上面，include 之后
class GASAssimpLogStream : public Assimp::LogStream
{
public:
    // 必须实现这个纯虚函数
    void write(const char* message) override
    {
        // 这里把 Assimp 的日志转发给你的 GAS_LOG 或者直接 printf
        // 注意：Assimp 的 message 通常末尾自带换行符
        printf("[Assimp Internal] %s", message);
    }
};

bool GASImporter::ImportFromFile(const std::string& FilePath,
    std::shared_ptr<GASSkeleton>& OutSkeleton,
    std::vector<std::shared_ptr<GASAnimation>>& OutAnimations,
    std::vector<std::shared_ptr<GASMesh>>& OutMeshes)
{
    Assimp::Importer Importer;

    // 设置导入标志位
    // 1. Triangulate: 保证所有网格是三角形
    // 2. LimitBoneWeights: 限制每个顶点最多4根骨骼 (这是GPU蒙皮的标准限制)
    // 3. ConvertToLeftHanded: 自动处理大部分坐标系转换 (Z反转, 面剔除顺序等)

    const unsigned int Flags = aiProcess_Triangulate |
        aiProcess_LimitBoneWeights |
        aiProcess_GenSmoothNormals |
        aiProcess_ConvertToLeftHanded |
        aiProcess_PopulateArmatureData; // 这一步很重要，帮助Assimp识别骨架

    const aiScene* Scene = Importer.ReadFile(FilePath, Flags);

    if (!Scene  || !Scene->mRootNode)
    {
        GAS_LOG_ERROR("Assimp Import Failed: %s", Importer.GetErrorString());
        return false;
    }

    //创建骨骼对象
    OutSkeleton = std::make_shared<GASSkeleton>();
    OutSkeleton->AssetName = GASFileHelper::GetFileName(FilePath); // 简单起见用路径做名字

    //处理骨骼 (这是后续动画的基础)
    if (!ProcessSkeleton(Scene, OutSkeleton.get()))
    {
        return false;
    }

    // 处理动画
    if (Scene->mNumAnimations > 0)
    {
        ProcessAnimations(Scene, OutSkeleton.get(), OutAnimations);
    }

    //处理mesh
    if (Scene->HasMeshes())
    {
        for (unsigned int i = 0; i < Scene->mNumMeshes; ++i)
        {
            const aiMesh* Mesh = Scene->mMeshes[i];

            bool bHasSkinning = Mesh->HasBones() && Mesh->mNumBones > 0;

            auto NewMesh = std::make_shared<GASMesh>();
            NewMesh->AssetName = FilePath + "_" + Mesh->mName.C_Str();
            NewMesh->SkeletonGUID = OutSkeleton->GetGUID();

            // ProcessMesh (负责提取蒙皮权重和顶点)
            if (ProcessMesh(Mesh, OutSkeleton.get(), NewMesh.get()))
            {
                OutMeshes.push_back(NewMesh);
            }

            if (bHasSkinning)
            {
                NewMesh->SetHasSkin(true);
            }

        }
    }
    return true;
}

// 骨骼处理逻辑
bool GASImporter::ProcessSkeleton(const aiScene* Scene, GASSkeleton* TargetSkeleton)
{
    ValidBoneMap.clear();
    InverseBindMatrixMap.clear();

    //扫描所有 Mesh，收集真正有权重的骨骼信息 Assimp 把 IBM (Inverse Bind Matrix) 存在 Mesh 里
    if (Scene->HasMeshes())
    {
        for (unsigned int i = 0; i < Scene->mNumMeshes; ++i)
        {
            const aiMesh* Mesh = Scene->mMeshes[i];
            for (unsigned int b = 0; b < Mesh->mNumBones; ++b)
            {
                const aiBone* Bone = Mesh->mBones[b];
                std::string BoneName = Bone->mName.C_Str();

                // 规范化名称
                std::string NormalizedName = GASDataConverter::NormalizeBoneName(BoneName);
                // 标记为有效骨骼
                ValidBoneMap[NormalizedName] = true;

                InverseBindMatrixMap[NormalizedName] = GASDataConverter::ToMatrix4x4(Bone->mOffsetMatrix);
            }
        }
    }

    // 递归遍历 Node 树，构建线性骨骼数组  从 Root Node 开始
    RecursivelyProcessBoneNode(Scene->mRootNode, -1, TargetSkeleton);
    //补全 Header
    TargetSkeleton->BaseHeader.Magic = GAS_ASSET_MAGIC;
    TargetSkeleton->BaseHeader.Version = 1;
    TargetSkeleton->BaseHeader.AssetType = EGASAssetType::Skeleton;
    TargetSkeleton->BaseHeader.HeaderSize = sizeof(FGASAssetHeader) + sizeof(FGASSkeletonHeader);
    TargetSkeleton->SkeletonHeader.BoneCount = (uint32_t)TargetSkeleton->Bones.Num();
    TargetSkeleton->BaseHeader.DataSize = TargetSkeleton->SkeletonHeader.BoneCount * sizeof(FGASBoneDefinition);
    TargetSkeleton->BaseHeader.XXHash64 = CalculateXXHash64(TargetSkeleton->Bones.GetData(), TargetSkeleton->BaseHeader.DataSize);


    // 完成构建，重建映射表
    TargetSkeleton->RebuildBoneMap(); 

    return TargetSkeleton->Bones.Num() > 0;
}

// 动画处理逻辑 
bool GASImporter::ProcessAnimations(const aiScene* Scene, const GASSkeleton* Skeleton, std::vector<std::shared_ptr<GASAnimation>>& TargetAnimList)
{
    for (unsigned int i = 0; i < Scene->mNumAnimations; ++i)
    {
        const aiAnimation* SrcAnim = Scene->mAnimations[i];
        auto NewAnim = std::make_shared<GASAnimation>();

        //设置头部信息  Assimp 的 Duration 是 Ticks，需要除以 TicksPerSecond 得到秒
        double TicksPerSecond = (SrcAnim->mTicksPerSecond != 0) ? SrcAnim->mTicksPerSecond : 25.0;
        NewAnim->AnimHeader.Duration = (float)(SrcAnim->mDuration / TicksPerSecond);
        NewAnim->AnimHeader.FrameRate = 30.0f;
        NewAnim->AnimHeader.TrackCount = Skeleton->GetNumBones();

        // 计算总帧数
        int32_t FrameCount = (int32_t)(NewAnim->AnimHeader.Duration * NewAnim->AnimHeader.FrameRate) + 1;
        NewAnim->AnimHeader.FrameCount = FrameCount;

        // 准备数据容器 大小 = 帧数 * 骨骼数
        int32_t TotalDataSize = FrameCount * NewAnim->AnimHeader.TrackCount;
        NewAnim->Tracks.Resize(TotalDataSize);

        // 建立 BoneName -> aiNodeAnim* 的映射，加速查找
        std::map<std::string, const aiNodeAnim*> NodeAnimMap;
        for (unsigned int ch = 0; ch < SrcAnim->mNumChannels; ++ch)
        {
            std::string NodeName = GASDataConverter::NormalizeBoneName(SrcAnim->mChannels[ch]->mNodeName.C_Str());
            NodeAnimMap[NodeName] = SrcAnim->mChannels[ch];
        }

        //重采样循环 
        double TimePerFrame = 1.0 / NewAnim->AnimHeader.FrameRate;

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
                    // 无动画：使用默认姿态 
                    TrackData.LocalTransform.Scale = FGASVector3(1, 1, 1);
                    TrackData.LocalTransform.Translation = FGASVector3(0, 0, 0);
                    TrackData.LocalTransform.Rotation = FGASQuaternion(0, 0, 0, 1);
                }

                // 存入扁平化数组 Index = Frame * NumBones + BoneIndex
                int32_t GlobalIndex = Frame * NewAnim->AnimHeader.TrackCount + BoneIdx;
                NewAnim->Tracks[GlobalIndex] = TrackData;
            }
        }


        NewAnim->BaseHeader.Magic = GAS_ASSET_MAGIC;
        NewAnim->BaseHeader.Version = 1;
        NewAnim->BaseHeader.AssetType = EGASAssetType::Animation;
        NewAnim->BaseHeader.HeaderSize = sizeof(FGASAnimationHeader) + sizeof(FGASAssetHeader);
        NewAnim->BaseHeader.DataSize = (uint32_t)(TotalDataSize * sizeof(FGASAnimTrackData));
        NewAnim->BaseHeader.XXHash64 = CalculateXXHash64(NewAnim->Tracks.GetData(), NewAnim->BaseHeader.DataSize);
        // 动画特有字段
        NewAnim->AnimHeader.TargetSkeletonGUID = Skeleton->GetGUID(); // 引用所属骨骼
        NewAnim->AnimHeader.FrameCount = (uint32_t)FrameCount;
        NewAnim->AnimHeader.TrackCount = (uint32_t)Skeleton->GetNumBones();
        NewAnim->AnimHeader.FrameRate = 30.0f;
        NewAnim->AnimHeader.Duration = (float)(SrcAnim->mDuration / TicksPerSecond);

        TargetAnimList.push_back(NewAnim);
    }

    return true;
}

//Mesh逻辑处理
bool GASImporter::ProcessMesh(const aiMesh* Mesh, const GASSkeleton* Skeleton, GASMesh* TargetMesh)
{
    if (!Mesh->HasPositions() || !Mesh->HasBones() || Mesh->mNumBones == 0)
    {
        GAS_LOG_WARN("Mesh has no positions or bones, skipping skinning data extraction.");
        return false;
    }

    //预处理：映射骨骼名称到本地索引  Assimp 的 Bone Index 是 Mesh 局部的，我们需要将其映射到 GASSkeleton 的全局索引。
    std::unordered_map<std::string, uint32_t> BoneNameMap;
    for (unsigned int i = 0; i < Mesh->mNumBones; ++i)
    {
        std::string name = GASDataConverter::NormalizeBoneName(Mesh->mBones[i]->mName.C_Str());
        int32_t GlobalIndex = Skeleton->FindBoneIndex(name);
        if (GlobalIndex != -1)
        {
            BoneNameMap[name] = GlobalIndex;
        }
        else
        {
             GAS_LOG_WARN("Mesh Bone '%s' not found in Skeleton asset. Data ignored.", name.c_str());
        }
    }

    //遍历顶点，提取所有蒙皮影响
    std::vector<std::vector<BoneInfluence>> AllInfluences(Mesh->mNumVertices);

    for (unsigned int i = 0; i < Mesh->mNumBones; ++i)
    {
        const aiBone* Bone = Mesh->mBones[i];
        std::string NormalizedName = GASDataConverter::NormalizeBoneName(Bone->mName.C_Str());

        if (BoneNameMap.count(NormalizedName))
        {
            uint32_t GlobalBoneIndex = BoneNameMap[NormalizedName];

            for (unsigned int j = 0; j < Bone->mNumWeights; ++j)
            {
                const aiVertexWeight& Weight = Bone->mWeights[j];
                AllInfluences[Weight.mVertexId].push_back({ GlobalBoneIndex, Weight.mWeight });
            }
        }
    }

    // 遍历顶点，进行权重截断和归一化 
    TargetMesh->Vertices.Resize(Mesh->mNumVertices);

    FGASAABB BBox;
    BBox.Min = FGASVector3(FLT_MAX, FLT_MAX, FLT_MAX);
    BBox.Max = FGASVector3(-FLT_MAX, -FLT_MAX, -FLT_MAX);

    for (unsigned int i = 0; i < Mesh->mNumVertices; ++i)
    {
        FGASSkinVertex& Vertex = TargetMesh->Vertices[i];
        std::vector<BoneInfluence>& Influences = AllInfluences[i];

        const FGASVector3& Pos = TargetMesh->Vertices[i].Position;
        // 提取基础几何信息
        Vertex.Position = GASDataConverter::ToVector3(Mesh->mVertices[i]);
        BBox.Min.x = std::min(BBox.Min.x, Vertex.Position.x);
        BBox.Min.y = std::min(BBox.Min.y, Vertex.Position.y);
        BBox.Min.z = std::min(BBox.Min.z, Vertex.Position.z);
        BBox.Max.x = std::max(BBox.Max.x, Vertex.Position.x);
        BBox.Max.y = std::max(BBox.Max.y, Vertex.Position.y);
        BBox.Max.z = std::max(BBox.Max.z, Vertex.Position.z);

        if (Mesh->HasNormals()) {
            Vertex.Normal = GASDataConverter::ToVector3(Mesh->mNormals[i]);
        }
        if (Mesh->HasTangentsAndBitangents()) {
            Vertex.Tangent = GASDataConverter::ToVector3(Mesh->mTangents[i]);
        }
        if (Mesh->HasTextureCoords(0)) {
            // 只取 UV 坐标的 x, y 分量
            Vertex.UV.x = Mesh->mTextureCoords[0][i].x;
            Vertex.UV.y = Mesh->mTextureCoords[0][i].y;
            Vertex.UV.z = 0; 
        }

        // 权重处理：截断
        if (Influences.size() > MAX_BONE_INFLUENCES)
        {
            // 使用部分排序找到前 MAX_BONE_INFLUENCES 个最大的影响
            std::partial_sort(
                Influences.begin(),
                Influences.begin() + MAX_BONE_INFLUENCES,
                Influences.end()
            );
            Influences.resize(MAX_BONE_INFLUENCES);
            // GAS_LOG_WARN("Vertex %d had %d influences, truncated to %d.", i, OriginalSize, MAX_BONE_INFLUENCES);
        }

        // 权重处理：归一化
        float TotalWeight = 0.0f;
        for (const auto& inf : Influences)
        {
            TotalWeight += inf.Weight;
        }

        if (TotalWeight > 0.0f)
        {
            float InvTotalWeight = 1.0f / TotalWeight;

            for (size_t j = 0; j < Influences.size(); ++j)
            {
                // 存储归一化后的权重和索引
                Vertex.BoneWeights.Weights[j] = Influences[j].Weight * InvTotalWeight;
                Vertex.BoneIndices.Indices[j] = Influences[j].Index;
            }
        }
        else
        {
            // GAS_LOG_WARN("Vertex %d has zero total weight after truncation/mapping.", i);
            // 保持所有权重和索引为零/默认值
        }
    }

    //提取索引数据 
    if (Mesh->HasFaces())
    {
        // 索引数量 = Face数量 * 每个Face的索引数量 (Assimp 保证是三角形，即 3)
        TargetMesh->Indices.Resize(Mesh->mNumFaces * 3);
        uint32_t CurrentIndex = 0;
        for (unsigned int i = 0; i < Mesh->mNumFaces; ++i)
        {
            const aiFace& Face = Mesh->mFaces[i];
            if (Face.mNumIndices == 3)
            {
                TargetMesh->Indices[CurrentIndex++] = Face.mIndices[0];
                TargetMesh->Indices[CurrentIndex++] = Face.mIndices[1];
                TargetMesh->Indices[CurrentIndex++] = Face.mIndices[2];
            }
        }
    }

    TargetMesh->BaseHeader.Magic = GAS_ASSET_MAGIC;
    TargetMesh->BaseHeader.Version = 1;
    TargetMesh->BaseHeader.AssetType =  EGASAssetType::Mesh;
    TargetMesh->BaseHeader.HeaderSize = sizeof(FGASMeshHeader);

    TargetMesh->MeshHeader.NumVertices = (uint32_t)Mesh->mNumVertices;
    TargetMesh->MeshHeader.NumIndices = (uint32_t)TargetMesh->Indices.Num();
    TargetMesh->MeshHeader.AABB = BBox;
    uint32_t VertSize = TargetMesh->MeshHeader.NumVertices * sizeof(FGASSkinVertex);
    uint32_t IdxSize = TargetMesh->MeshHeader.NumIndices * sizeof(uint32_t);
    TargetMesh->BaseHeader.DataSize = VertSize + IdxSize;
    uint64_t VertHash = CalculateXXHash64(TargetMesh->Vertices.GetData(), VertSize);
    uint64_t IdxHash = CalculateXXHash64(TargetMesh->Indices.GetData(), IdxSize);
    TargetMesh->BaseHeader.XXHash64 = VertHash ^ (IdxHash + 0x9e3779b9 + (VertHash << 6) + (VertHash >> 2));

    return true;
}


void GASImporter::RecursivelyProcessBoneNode(const aiNode* Node, int32_t ParentBoneIndex, GASSkeleton* TargetSkeleton)
{
    std::string NodeName = Node->mName.C_Str();
    if (NodeName.find("_$AssimpFbx$_") != std::string::npos)
    {
        for (unsigned int i = 0; i < Node->mNumChildren; ++i)
        {
            RecursivelyProcessBoneNode(Node->mChildren[i], ParentBoneIndex, TargetSkeleton);
        }
        return; 
    }
    std::string NormalizedName = GASDataConverter::NormalizeBoneName(NodeName);

    int32_t CurrentBoneIndex = -1;

    bool bIsBone = (ValidBoneMap.find(NormalizedName) != ValidBoneMap.end());

    // 如果是骨骼，或者是必须保留的层级节点，则加入 Skeleton
    if (bIsBone || ParentBoneIndex != -1) // 只要父节点是骨骼，我们也默认保留子节点以维持层级
    {
        FGASBoneDefinition NewBone;
        SetGASBoneName(NewBone, NormalizedName.c_str());
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

        //添加骨骼 
        TargetSkeleton->Bones.Resize(CurrentBoneIndex + 1);
        TargetSkeleton->Bones.Add(NewBone);
    }

    // 递归处理子节点
    for (unsigned int i = 0; i < Node->mNumChildren; ++i)
    {
        RecursivelyProcessBoneNode(Node->mChildren[i], CurrentBoneIndex, TargetSkeleton);
    }
}


// 简单的线性插值查找
void GASImporter::EvaluateChannel(const aiNodeAnim* Channel, double Time, FGASTransform& OutTransform)
{
    // 辅助 Lambda: 查找当前时间对应的关键帧索引

    auto FindKeyIndex = [&](unsigned int NumKeys, const auto* Keys) -> unsigned int
        {

            if (NumKeys < 2) return 0;
            for (unsigned int i = 0; i < NumKeys - 1; i++)
            {
                if (Time < Keys[i + 1].mTime)
                {
                    return i;
                }
            }

            return NumKeys - 1; 
        };

    //位置 (Position) - 线性插值 (Lerp)

    aiVector3D ResultPos;

    if (Channel->mNumPositionKeys > 0)
    {
        if (Channel->mNumPositionKeys == 1)
        {
            ResultPos = Channel->mPositionKeys[0].mValue;
        }
        else
        {
            unsigned int Index = FindKeyIndex(Channel->mNumPositionKeys, Channel->mPositionKeys);
            unsigned int NextIndex = (Index + 1 >= Channel->mNumPositionKeys) ? Index : Index + 1;

            if (Index == NextIndex)
            {
                ResultPos = Channel->mPositionKeys[Index].mValue;
            }
            else
            {
                const aiVectorKey& StartKey = Channel->mPositionKeys[Index];
                const aiVectorKey& EndKey = Channel->mPositionKeys[NextIndex];

                // 计算插值因子 Alpha [0, 1]
                double DeltaTime = EndKey.mTime - StartKey.mTime;
                float Factor = (float)((Time - StartKey.mTime) / DeltaTime);

                // 钳制 Factor 防止浮点误差
                if (Factor < 0.0f) Factor = 0.0f;
                if (Factor > 1.0f) Factor = 1.0f;

                // 线性插值: P = P0 + (P1 - P0) * t
                const aiVector3D& Start = StartKey.mValue;
                const aiVector3D& End = EndKey.mValue;
                ResultPos = Start + (End - Start) * Factor;
            }
        }
    }

    //  2. 旋转 (Rotation) - 球面线性插值 (Slerp)
  
    aiQuaternion ResultRot;

    if (Channel->mNumRotationKeys > 0)
    {
        if (Channel->mNumRotationKeys == 1)
        {
            ResultRot = Channel->mRotationKeys[0].mValue;
        }
        else
        {
            unsigned int Index = FindKeyIndex(Channel->mNumRotationKeys, Channel->mRotationKeys);
            unsigned int NextIndex = (Index + 1 >= Channel->mNumRotationKeys) ? Index : Index + 1;

            if (Index == NextIndex)
            {
                ResultRot = Channel->mRotationKeys[Index].mValue;
            }
            else
            {
                const aiQuatKey& StartKey = Channel->mRotationKeys[Index];
                const aiQuatKey& EndKey = Channel->mRotationKeys[NextIndex];

                double DeltaTime = EndKey.mTime - StartKey.mTime;
                float Factor = (float)((Time - StartKey.mTime) / DeltaTime);

                if (Factor < 0.0f) Factor = 0.0f;
                if (Factor > 1.0f) Factor = 1.0f;

                // 使用 Assimp 内置的 Slerp
                const aiQuaternion& Start = StartKey.mValue;
                const aiQuaternion& End = EndKey.mValue;
                aiQuaternion::Interpolate(ResultRot, Start, End, Factor);
                ResultRot.Normalize(); // 这是一个好习惯，防止插值后失去单位化
            }
        }
    }

    //3. 缩放 (Scale) - 线性插值 (Lerp)
    aiVector3D ResultScale(1.0f, 1.0f, 1.0f);

    if (Channel->mNumScalingKeys > 0)
    {
        if (Channel->mNumScalingKeys == 1)
        {
            ResultScale = Channel->mScalingKeys[0].mValue;
        }
        else
        {
            unsigned int Index = FindKeyIndex(Channel->mNumScalingKeys, Channel->mScalingKeys);
            unsigned int NextIndex = (Index + 1 >= Channel->mNumScalingKeys) ? Index : Index + 1;

            if (Index == NextIndex)
            {
                ResultScale = Channel->mScalingKeys[Index].mValue;
            }
            else
            {
                const aiVectorKey& StartKey = Channel->mScalingKeys[Index];
                const aiVectorKey& EndKey = Channel->mScalingKeys[NextIndex];

                double DeltaTime = EndKey.mTime - StartKey.mTime;
                float Factor = (float)((Time - StartKey.mTime) / DeltaTime);

                if (Factor < 0.0f) Factor = 0.0f;
                if (Factor > 1.0f) Factor = 1.0f;

                const aiVector3D& Start = StartKey.mValue;
                const aiVector3D& End = EndKey.mValue;
                ResultScale = Start + (End - Start) * Factor;
            }
        }
    }

    // 数据转换 (Assimp -> GAS)

    OutTransform.Translation = GASDataConverter::ToVector3(ResultPos);
    OutTransform.Rotation = GASDataConverter::ToQuaternion(ResultRot);
    OutTransform.Scale = GASDataConverter::ToVector3(ResultScale);
}

