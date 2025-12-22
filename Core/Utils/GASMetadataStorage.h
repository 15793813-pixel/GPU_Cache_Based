#pragma once

#include <string>
#include <vector>
#include <memory>
#include <sqlite/sqlite3.h>
#include "../Types/GASCoreTypes.h"

// 用于数据库查询结果的轻量级结构体
struct FGASAssetMetadata
{
    uint64_t GUID = 0;
    std::string Name;
    EGASAssetType Type = EGASAssetType::Skeleton;
    std::string BinaryFilePath; // 存储 .gas 文件路径
    uint64_t FileHash;
    int32_t FrameCount = 0;
    float Duration = 0.0f;
    int32_t BoneCount = 0;
    int32_t VerticeCount = 0;
    int32_t MeshCount = 0;
};



//负责管理资产的元数据索引，基于 SQLite 实现。
class GASMetadataStorage
{
public:
    GASMetadataStorage();
    ~GASMetadataStorage();

    // 初始化数据库，创建表结构 
    bool Initialize(const std::string& DBPath);

    //导入成功后，向数据库注册资产的元数据 
    bool RegisterAsset(const FGASAssetMetadata& Metadata);

    //通过 GUID 查找元数据
    bool QueryAssetByGUID(uint64_t GUID, FGASAssetMetadata& OutMetadata) const;

    // 通过Hash找到所有的.gsa文件
    bool QueryAssetsByFileHash(uint64_t FileHash, std::vector<FGASAssetMetadata>& OutList);

 

    // 查找所有资产元数据
    std::vector<FGASAssetMetadata> QueryAllAssets() const;

private:
    sqlite3* DB = nullptr;
};