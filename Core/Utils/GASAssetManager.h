#pragma once

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include "../Types/GASAsset.h"
#include "GASMetadataStorage.h"
#include "GASImporter.h"
#include "GASBinarySerializer.h"
#include "../Types/GASConfig.h" // 包含配置路径

/**
 * @class GASAssetManager
 * @brief 资源管理器，系统的核心单例。
 * 负责资产的导入、持久化、运行时加载和内存缓存管理。
 */
class GASAssetManager
{
private:
    // Singleton pattern: 私有化构造函数
    GASAssetManager();
    ~GASAssetManager();

public:
    /** 获取单例实例 */
    static GASAssetManager& Get();

    /** 初始化管理器：建立数据库连接、设置路径 */
    bool Initialize();

    // =========================================================
    // 资产导入与持久化 (Offline / Editor-Time)
    // =========================================================

    /**
     * 执行导入、标准化、烘焙、序列化和注册的全流程
     * @param SourceFilePath 原始 FBX/GLTF 文件路径
     * @return 成功导入的 Skeleton GUID，失败返回 0
     */
    uint64_t ImportAsset(const std::string& SourceFilePath);

    // =========================================================
    // 运行时资产加载与缓存 (Runtime / Online)
    // =========================================================

    /**
     * 运行时请求资产，优先从内存缓存中获取。
     * 如果不在缓存中，则通过 MetadataStorage 查找路径，并从磁盘加载。
     * 这是未来异步加载的核心目标函数。
     * @param GUID 资产的唯一标识
     * @return 资产对象指针 (可能是 Skeleton 或 Animation)，失败返回 nullptr
     */
    std::shared_ptr<GASAsset> LoadAsset(uint64_t GUID);

    /** 从内存缓存中获取资产 (不触发磁盘加载) */
    std::shared_ptr<GASAsset> GetCachedAsset(uint64_t GUID) const;

    /** 从数据库中查询元数据 */
    bool QueryMetadata(uint64_t GUID, FGASAssetMetadata& OutMetadata) const;

private:
    /** 内存缓存：存储已加载到内存的资产 */
    std::unordered_map<uint64_t, std::shared_ptr<GASAsset>> MemoryCache;

    /** 数据库管理器：负责元数据索引 */
    GASMetadataStorage MetadataStorage;

    /** Assimp 导入器 (只在导入时使用) */
    GASImporter Importer;

    // 互斥锁：用于保护 MemoryCache 和 MetadataStorage 在多线程访问时的安全
    // (这是为异步调度做准备)
    // std::mutex Mutex; 
};