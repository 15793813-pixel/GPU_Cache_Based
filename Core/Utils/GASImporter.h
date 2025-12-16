#pragma once
#pragma once

#include <string>
#include <vector>
#include <memory>
#include <map>
#include "../Types/GASAsset.h"


struct aiScene;
struct aiNode;
struct aiAnimation;
struct aiNodeAnim;
struct aiMesh;

// 负责加载外部模型文件 (FBX/GLTF)，并生成 GASSkeleton 和 GASAnimation 对象

class GASImporter
{
public:
    GASImporter();
    ~GASImporter();

    //从文件加载并处理资产
    bool ImportFromFile(const std::string& FilePath, std::shared_ptr<GASSkeleton>& OutSkeleton, std::vector<std::shared_ptr<GASAnimation>>& OutAnimations, std::vector<std::shared_ptr<GASMesh>>& OutMeshes);

private:

    //处理骨骼结构：构建骨骼列表、层级关系、提取逆绑定矩阵 
    bool ProcessSkeleton(const aiScene* Scene, GASSkeleton* TargetSkeleton);

    // 处理动画：对所有动画轨道进行重采样 (Baking)
    bool ProcessAnimations(const aiScene* Scene, const GASSkeleton* Skeleton, std::vector<std::shared_ptr<GASAnimation>>& TargetAnimList);

    //处理mesh
    bool ProcessMesh(const aiMesh* Mesh, const GASSkeleton* Skeleton, GASMesh* TargetMesh);

    // 辅助工具

    // 递归遍历节点树，展平为骨骼数组 
    void RecursivelyProcessBoneNode(const aiNode* Node, int32_t ParentBoneIndex, GASSkeleton* TargetSkeleton);
    
    //辅助：在 Assimp 动画通道中采样特定时间的变换
    void EvaluateChannel(const aiNodeAnim* Channel, double Time, FGASTransform& OutTransform);


    


private:
    // 临时缓存：记录哪些节点是真正的骨骼 (Name -> IsBone)
    // Assimp 中很多节点只是辅助节点，我们需要通过 Mesh 中的 Bone 列表来标记哪些是有用的
    std::map<std::string, bool> ValidBoneMap;

    // 临时缓存：记录骨骼名称对应的逆绑定矩阵 (Name -> Matrix) 因为逆绑定矩阵存在 aiMesh 中，而层级结构在 aiNode 中，需要暂存
    std::map<std::string, FGASMatrix4x4> InverseBindMatrixMap;
};