#pragma once
#include "GASImporter.h"
#include "GASDataConverter.h"
#include "GASLogging.h"          
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp/DefaultLogger.hpp>
#include <assimp/LogStream.hpp>
#include <filesystem>

#include "GASHashManager.h"
#include "GASDebug.h"


GASImporter::GASImporter() {}
GASImporter::~GASImporter() {}



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

    //将骨骼最大影响设置设置为8
    Importer.SetPropertyInteger(AI_CONFIG_PP_LBW_MAX_WEIGHTS, 8);

    const unsigned int Flags = aiProcess_Triangulate |
        aiProcess_LimitBoneWeights |
        aiProcess_GenSmoothNormals |
        aiProcess_ConvertToLeftHanded |
        aiProcess_PopulateArmatureData; 

    const aiScene* Scene = Importer.ReadFile(FilePath, Flags);

    if (!Scene  || !Scene->mRootNode)
    {
        GAS_LOG_ERROR("Assimp Import Failed: %s", Importer.GetErrorString());
        return false;
    }

    //创建骨骼对象
    OutSkeleton = std::make_shared<GASSkeleton>();
    OutSkeleton->AssetName = GASFileHelper::GetFileName(FilePath); 

    //处理骨骼 
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
            if (ProcessMesh(Scene,Mesh, OutSkeleton.get(), NewMesh.get()))
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

    //收集权重骨骼和 IBM
    if (Scene->HasMeshes())
    {
        for (unsigned int i = 0; i < Scene->mNumMeshes; ++i)
        {
            const aiMesh* Mesh = Scene->mMeshes[i];
            for (unsigned int b = 0; b < Mesh->mNumBones; ++b)
            {
                const aiBone* Bone = Mesh->mBones[b];
                std::string NormalizedName = GASDataConverter::NormalizeBoneName(Bone->mName.C_Str());

                ValidBoneMap[NormalizedName] = true;
                InverseBindMatrixMap[NormalizedName] = GASDataConverter::ToMatrix4x4(Bone->mOffsetMatrix);
            }
        }
    }

    // 标记必须保留的祖先节点
    MarkRequiredNodes(Scene->mRootNode);

    //  递归构建 
    RecursivelyProcessBoneNode(Scene->mRootNode, -1, TargetSkeleton, FGASMatrix4x4());

    // 4. 填充 Header
    TargetSkeleton->BaseHeader.Magic = GAS_ASSET_MAGIC;
    TargetSkeleton->BaseHeader.Version = 1;
    TargetSkeleton->BaseHeader.AssetType = EGASAssetType::Skeleton;

    uint32_t BoneCount = (uint32_t)TargetSkeleton->Bones.Num();
    TargetSkeleton->SkeletonHeader.BoneCount = BoneCount;
    TargetSkeleton->BaseHeader.HeaderSize = sizeof(FGASAssetHeader) + sizeof(FGASSkeletonHeader);
    TargetSkeleton->BaseHeader.DataSize = BoneCount * sizeof(FGASBoneDefinition);
    // 计算 Hash
    TargetSkeleton->BaseHeader.XXHash64 = CalculateXXHash64(TargetSkeleton->Bones.GetData(), TargetSkeleton->BaseHeader.DataSize);

    // 重建名称索引映射
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

        // 时间转换
        double TicksPerSecond = (SrcAnim->mTicksPerSecond != 0) ? SrcAnim->mTicksPerSecond : 25.0;
        NewAnim->AnimHeader.Duration = (float)(SrcAnim->mDuration / TicksPerSecond);
        NewAnim->AnimHeader.FrameRate = 30.0f;
        NewAnim->AnimHeader.TrackCount = Skeleton->GetNumBones();

        int32_t FrameCount = (int32_t)(NewAnim->AnimHeader.Duration * NewAnim->AnimHeader.FrameRate) + 1;
        NewAnim->AnimHeader.FrameCount = FrameCount;

        int32_t TotalDataSize = FrameCount * NewAnim->AnimHeader.TrackCount;
        NewAnim->Tracks.Resize(TotalDataSize);

        // 建立映射
        std::map<std::string, const aiNodeAnim*> NodeAnimMap;
        for (unsigned int ch = 0; ch < SrcAnim->mNumChannels; ++ch)
        {
            std::string NodeName = GASDataConverter::NormalizeBoneName(SrcAnim->mChannels[ch]->mNodeName.C_Str());
            NodeAnimMap[NodeName] = SrcAnim->mChannels[ch];
        }

        double TimePerFrame = 1.0 / NewAnim->AnimHeader.FrameRate;

        for (int32_t Frame = 0; Frame < FrameCount; ++Frame)
        {
            double AnimTimeTicks = (Frame * TimePerFrame) * TicksPerSecond;
            if (AnimTimeTicks > SrcAnim->mDuration) AnimTimeTicks = SrcAnim->mDuration;

            for (int32_t BoneIdx = 0; BoneIdx < Skeleton->GetNumBones(); ++BoneIdx)
            {
                const std::string& BoneName = Skeleton->Bones[BoneIdx].Name;
                std::string NormalizedName = GASDataConverter::NormalizeBoneName(BoneName);

                FGASAnimTrackData TrackData;
                auto It = NodeAnimMap.find(NormalizedName);

                if (It != NodeAnimMap.end())
                {
                    // 有动画数据，正常采样
                    FGASTransform LocalTransform;
                    EvaluateChannel(It->second, AnimTimeTicks, LocalTransform);
                    TrackData.LocalTransform = LocalTransform;
                }
                else
                {
                    // 无动画数据，可能是静态骨骼，尝试去 aiScene 的 Node 树里找它的默认 Static Transform
                    aiNode* TargetNode = Scene->mRootNode->FindNode(BoneName.c_str());
                    if (TargetNode)
                    {
                        aiVector3D scaling, position;
                        aiQuaternion rotation;
                        TargetNode->mTransformation.Decompose(scaling, rotation, position);
                        TrackData.LocalTransform.Translation = GASDataConverter::ToVector3(position);
                        TrackData.LocalTransform.Scale = GASDataConverter::ToVector3(scaling);
                        TrackData.LocalTransform.Rotation = GASDataConverter::ToQuaternion(rotation);
                    }
                    else
                    {
                        //如果也找不到，设为单位矩阵
                        TrackData.LocalTransform.Scale = FGASVector3(1, 1, 1);
                        TrackData.LocalTransform.Translation = FGASVector3(0, 0, 0);
                        TrackData.LocalTransform.Rotation = FGASQuaternion(0, 0, 0, 1);
                    }
                }
                int32_t GlobalIndex = Frame * NewAnim->AnimHeader.TrackCount + BoneIdx;
                NewAnim->Tracks[GlobalIndex] = TrackData;
            }
        }

        // Header 填充
        NewAnim->BaseHeader.Magic = GAS_ASSET_MAGIC;
        NewAnim->BaseHeader.Version = 1;
        NewAnim->BaseHeader.AssetType = EGASAssetType::Animation;
        NewAnim->BaseHeader.HeaderSize = sizeof(FGASAnimationHeader) + sizeof(FGASAssetHeader);
        NewAnim->BaseHeader.DataSize = (uint32_t)(TotalDataSize * sizeof(FGASAnimTrackData));

        NewAnim->BaseHeader.XXHash64 = CalculateXXHash64(NewAnim->Tracks.GetData(), NewAnim->BaseHeader.DataSize);

        NewAnim->AnimHeader.TargetSkeletonGUID = Skeleton->GetGUID();
        NewAnim->AnimHeader.FrameCount = (uint32_t)FrameCount;
        NewAnim->AnimHeader.TrackCount = (uint32_t)Skeleton->GetNumBones();
        NewAnim->AnimHeader.FrameRate = 30.0f;
        NewAnim->AnimHeader.Duration = (float)(SrcAnim->mDuration / TicksPerSecond);

        TargetAnimList.push_back(NewAnim);
    }

    return true;
}

//Mesh逻辑处理
bool GASImporter::ProcessMesh(const aiScene* Scene, const aiMesh* Mesh, const GASSkeleton* Skeleton, GASMesh* TargetMesh)
{
    if (!Mesh->HasPositions() || !Mesh->HasBones() || Mesh->mNumBones == 0)
    {
        GAS_LOG_WARN("Mesh has no positions or bones, skipping skinning data extraction.");
        return false;
    }
    std::string DiffusePath = "";

    // 映射骨骼名称到本地索引
    std::unordered_map<std::string, uint32_t> BoneNameMap;

    if (Scene->HasMaterials())
    {
        const aiMaterial* Material = Scene->mMaterials[Mesh->mMaterialIndex];

        aiString Path;
        if (Material->GetTexture(aiTextureType_DIFFUSE, 0, &Path) == aiReturn_SUCCESS)
        {
            TargetMesh->DiffuseTexturePath = Path.C_Str();
            std::replace(TargetMesh->DiffuseTexturePath.begin(), TargetMesh->DiffuseTexturePath.end(), '\\', '/');
        }
    }

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

    //  收集权重 (逻辑保持不变)
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

    // 3. 处理顶点数据
    TargetMesh->Vertices.Resize(Mesh->mNumVertices);
    FGASAABB BBox;
    BBox.Min = FGASVector3(FLT_MAX, FLT_MAX, FLT_MAX);
    BBox.Max = FGASVector3(-FLT_MAX, -FLT_MAX, -FLT_MAX);

    for (unsigned int i = 0; i < Mesh->mNumVertices; ++i)
    {
        FGASSkinVertex& Vertex = TargetMesh->Vertices[i];
        std::vector<BoneInfluence>& Influences = AllInfluences[i];

        // 基础几何
        Vertex.Position = GASDataConverter::ToVector3(Mesh->mVertices[i]);

        // AABB 更新
        BBox.Min.x = std::min(BBox.Min.x, Vertex.Position.x);
        BBox.Min.y = std::min(BBox.Min.y, Vertex.Position.y);
        BBox.Min.z = std::min(BBox.Min.z, Vertex.Position.z);
        BBox.Max.x = std::max(BBox.Max.x, Vertex.Position.x);
        BBox.Max.y = std::max(BBox.Max.y, Vertex.Position.y);
        BBox.Max.z = std::max(BBox.Max.z, Vertex.Position.z);

        if (Mesh->HasNormals()) Vertex.Normal = GASDataConverter::ToVector3(Mesh->mNormals[i]);
        if (Mesh->HasTangentsAndBitangents()) Vertex.Tangent = GASDataConverter::ToVector3(Mesh->mTangents[i]);
        if (Mesh->HasTextureCoords(0)) {
            Vertex.UV.x = Mesh->mTextureCoords[0][i].x;
            Vertex.UV.y = Mesh->mTextureCoords[0][i].y;
            Vertex.UV.z = 0;
        }

        // 权重排序与截断
        if (Influences.size() > MAX_BONE_INFLUENCES)
        {
            std::partial_sort(Influences.begin(), Influences.begin() + MAX_BONE_INFLUENCES, Influences.end());
            Influences.resize(MAX_BONE_INFLUENCES);
        }

        // 归一化
        float TotalWeight = 0.0f;
        for (const auto& inf : Influences) TotalWeight += inf.Weight;

        if (TotalWeight > 0.0f)
        {
            float InvTotalWeight = 1.0f / TotalWeight;
            for (size_t j = 0; j < Influences.size(); ++j)
            {
                Vertex.BoneWeights.Weights[j] = Influences[j].Weight * InvTotalWeight;
                Vertex.BoneIndices.Indices[j] = Influences[j].Index;
            }
        }
    }

    // 4. 处理索引
    if (Mesh->HasFaces())
    {
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

    // 5. 填充 Header 和 Hash
    TargetMesh->BaseHeader.Magic = GAS_ASSET_MAGIC;
    TargetMesh->BaseHeader.Version = 1;
    TargetMesh->BaseHeader.AssetType = EGASAssetType::Mesh;
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

//向上标记所有祖先
bool GASImporter::MarkRequiredNodes(aiNode* Node)
{
    std::string NormalizedName = GASDataConverter::NormalizeBoneName(Node->mName.C_Str());

    bool bHasValidChild = false;
    for (unsigned int i = 0; i < Node->mNumChildren; ++i)
    {
        if (MarkRequiredNodes(Node->mChildren[i]))
        {
            bHasValidChild = true;
        }
    }

    // 只要子孙有效，或者本身在 ValidMap 里（有权重），就标记为 true
    if (ValidBoneMap.count(NormalizedName) || bHasValidChild)
    {
        ValidBoneMap[NormalizedName] = true;
        return true;
    }

    return false;
}

// 递归建立骨骼节点 
void GASImporter::RecursivelyProcessBoneNode(
    const aiNode* Node,
    int32_t ParentBoneIndex,
    GASSkeleton* TargetSkeleton,
    FGASMatrix4x4 AccumulatedTransform)
{
    std::string NodeName = Node->mName.C_Str();
    std::string NormalizedName = GASDataConverter::NormalizeBoneName(NodeName);

    // 获取当前节点的 Local Matrix
    FGASMatrix4x4 CurrentNodeMatrix = GASDataConverter::ToMatrix4x4(Node->mTransformation);

    //矩阵累积：Total = ParentAccumulated * Current
  
    FGASMatrix4x4 TotalTransform = CurrentNodeMatrix* AccumulatedTransform ;

    //判断当前节点状态
    bool bIsVirtual = (NodeName.find("_$AssimpFbx$_") != std::string::npos);

    bool bIsRequired = (ValidBoneMap.find(NormalizedName) != ValidBoneMap.end());

    int32_t CurrentBoneIndex = ParentBoneIndex;

    FGASMatrix4x4 NextAccumulatedTransform; 

    // 4. 决策：是保留这个节点，还是将其“坍缩”？

    if (!bIsVirtual && bIsRequired)
    {
        FGASBoneDefinition NewBone;
        SetGASBoneName(NewBone, NormalizedName.c_str());
        NewBone.ParentIndex = ParentBoneIndex;

        // 设置 IBM (Inverse Bind Matrix)
        if (InverseBindMatrixMap.count(NormalizedName))
        {
            NewBone.InverseBindMatrix = InverseBindMatrixMap[NormalizedName];
        }
        else
        {
            NewBone.InverseBindMatrix = FGASMatrix4x4(); // Identity
        }
        GASDataConverter::DecomposeMatrix(TotalTransform, NewBone.LocalBindPose.Translation, NewBone.LocalBindPose.Rotation, NewBone.LocalBindPose.Scale);
        CurrentBoneIndex = (int32_t)TargetSkeleton->Bones.Num();
        TargetSkeleton->Bones.Add(NewBone);

        NextAccumulatedTransform = FGASMatrix4x4();
    }
    else
    {
        // 如果是虚拟节点 或者 不需要保留的节点  跳过将当前的 TotalTransform 传递给子节点，让子节点去“吸收”这个变换
        NextAccumulatedTransform = TotalTransform;
    }

    //递归处理子节点
    for (unsigned int i = 0; i < Node->mNumChildren; ++i)
    {
        RecursivelyProcessBoneNode(Node->mChildren[i], CurrentBoneIndex, TargetSkeleton, NextAccumulatedTransform);
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

