#include "GASAssetManager.h"
#include <sstream>
#include <filesystem> // C++17 文件系统操作
#include "GASLogging.h"

namespace fs = std::filesystem;

// =========================================================
// Singleton 实现
// =========================================================

GASAssetManager::GASAssetManager() {}

GASAssetManager::~GASAssetManager()
{
    // 清理缓存
    MemoryCache.clear();
}

GASAssetManager& GASAssetManager::Get()
{
    // C++11 Magic Static: 线程安全的单例初始化
    static GASAssetManager Instance;
    return Instance;
}

bool GASAssetManager::Initialize()
{
    // 1. 确保配置路径存在 (假设 GASConfig.h 中定义了路径常量)
    fs::create_directories(GAS_CONFIG::BINARY_CACHE_PATH);

    // 2. 初始化元数据数据库
    if (!MetadataStorage.Initialize(GAS_CONFIG::DATABASE_PATH))
    {
        // GAS_LOG_FATAL("Failed to initialize Metadata Database at %s", GAS_CONFIG::DATABASE_PATH.c_str());
        return false;
    }

    // GAS_LOG_INFO("GAS Asset Manager Initialized successfully.");
    return true;
}

// =========================================================
// 资产导入与持久化
// =========================================================

uint64_t GASAssetManager::ImportAsset(const std::string& SourceFilePath)
{
    // 1. 导入/解析/烘焙 (耗时操作)
    std::shared_ptr<GASSkeleton> SkeletonAsset = nullptr;
    std::vector<std::shared_ptr<GASAnimation>> AnimationAssets;

    if (!Importer.ImportFromFile(SourceFilePath, SkeletonAsset, AnimationAssets))
    {
        // GAS_LOG_ERROR("Failed to import asset from %s", SourceFilePath.c_str());
        return 0;
    }

    // 2. 为所有资产生成 GUID (全局唯一标识符)
    // 实际项目中应使用 UUID 库，这里简单用时间戳+哈希值生成 GUID
    uint64_t SkeletonGUID = (uint64_t)std::hash<std::string>{}(SourceFilePath);

    // 3. 序列化和注册
    // a) 处理 Skeleton
    if (SkeletonAsset)
    {
        SkeletonAsset->BaseHeader.AssetGUID = SkeletonGUID;
        SkeletonAsset->BaseHeader.AssetType = EGASAssetType::Skeleton;

        // 构造 .gas 文件的目标路径
        std::string BinaryPath = (fs::path(GAS_CONFIG::BINARY_CACHE_PATH) / (std::to_string(SkeletonGUID) + ".skeleton.gas")).string();

        // 写入磁盘
        if (GASBinarySerializer::SaveAssetToDisk(SkeletonAsset.get(), BinaryPath))
        {
            // 写入成功，注册元数据
            FGASAssetMetadata Metadata;
            Metadata.GUID = SkeletonGUID;
            Metadata.Name = SkeletonAsset->AssetName;
            Metadata.Type = EGASAssetType::Skeleton;
            Metadata.BinaryFilePath = BinaryPath;
            Metadata.BoneCount = SkeletonAsset->GetNumBones();

            MetadataStorage.RegisterAsset(Metadata);
        }
    }

    // b) 处理 Animations (略，逻辑与 Skeleton 类似，GUID 需要重新生成)
    // 遍历 AnimationAssets 列表，依次序列化和注册...

    // 4. 将 Skeleton 放入内存缓存 (方便编辑期立刻使用)
    MemoryCache[SkeletonGUID] = SkeletonAsset;

    // GAS_LOG_INFO("Asset %s imported and registered successfully. GUID: %llu", SourceFilePath.c_str(), SkeletonGUID);

    return SkeletonGUID;
}

// =========================================================
// 运行时资产加载与缓存
// =========================================================

std::shared_ptr<GASAsset> GASAssetManager::GetCachedAsset(uint64_t GUID) const
{
    // 线程安全考虑：实际应加锁 (Mutex)
    auto It = MemoryCache.find(GUID);
    if (It != MemoryCache.end())
    {
        return It->second;
    }
    return nullptr;
}

bool GASAssetManager::QueryMetadata(uint64_t GUID, FGASAssetMetadata& OutMetadata) const
{
    return MetadataStorage.QueryAssetByGUID(GUID, OutMetadata);
}

std::shared_ptr<GASAsset> GASAssetManager::LoadAsset(uint64_t GUID)
{
    // 1. 尝试从缓存获取
    auto CachedAsset = GetCachedAsset(GUID);
    if (CachedAsset)
    {
        return CachedAsset;
    }

    // 2. 缓存未命中，查询元数据获取路径
    FGASAssetMetadata Metadata;
    if (!QueryMetadata(GUID, Metadata))
    {
        // GAS_LOG_ERROR("Asset GUID %llu not found in Metadata database.", GUID);
        return nullptr;
    }

    // 3. 从磁盘加载 (耗时操作)
    std::shared_ptr<GASAsset> LoadedAsset = GASBinarySerializer::LoadAssetFromDisk(Metadata.BinaryFilePath);

    if (LoadedAsset)
    {
        // 4. 加载成功，更新缓存并返回
        // 线程安全考虑：实际应加锁
        LoadedAsset->AssetName = Metadata.Name; // 从元数据补充资产名称
        MemoryCache[GUID] = LoadedAsset;
        return LoadedAsset;
    }

    // GAS_LOG_ERROR("Failed to load binary file: %s", Metadata.BinaryFilePath.c_str());
    return nullptr;
}