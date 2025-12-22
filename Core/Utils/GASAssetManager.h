#pragma once
#include "../Types/GASAsset.h"
#include "GASMetadataStorage.h"
#include "GASImporter.h"
#include "GASLogging.h"
#include "GASBinarySerializer.h"
#include <shared_mutex>
#include "../Types/GASConfig.h" 
#include "GASWindows.h"
#include "GASHashManager.h"
#include "GASFileHelper.h"

// 负责资产的导入、持久化、运行时加载和内存缓存管理。

class GASAssetManager
{
private:
    GASAssetManager();
    ~GASAssetManager();

public:
    static GASAssetManager& Get();

    //初始化管理器：建立数据库连接、设置路径 
    bool Initialize();

    // 资产导入与持久化 (Offline / Editor-Time)    //执行导入、标准化、烘焙、序列化和注册的全流程
    uint64_t ImportAsset(const std::string& SourceFilePath);

    // 运行时请求资产，优先从内存缓存中获取。 如果不在缓存中，则通过 MetadataStorage 查找路径，并从磁盘加载。
    std::shared_ptr<GASAsset> LoadAsset(uint64_t GUID);

    //从内存缓存中获取资产 (不触发磁盘加载)
    std::shared_ptr<GASAsset> GetCachedAsset(uint64_t GUID) const;

    //从数据库中查询元数据
    bool QueryMetadata(uint64_t GUID, FGASAssetMetadata& OutMetadata) const;

    GASMetadataStorage& GetGASMetadataStorage(){return MetadataStorage;}
private:
    //内存缓存：存储已加载到内存的资产 
    std::unordered_map<uint64_t, std::shared_ptr<GASAsset>> MemoryCache;

    // 数据库管理器：负责元数据索引
    GASMetadataStorage MetadataStorage;

    //Assimp 导入器 (只在导入时使用) 
    GASImporter Importer;

    // 互斥锁：用于保护 MemoryCache 和 MetadataStorage 在多线程访问时的安全
    mutable std::shared_mutex CacheMutex;
};