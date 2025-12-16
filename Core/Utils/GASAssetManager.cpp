#include "GASAssetManager.h"
#include <sstream>
#include <filesystem>
#include "GASLogging.h"

#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/lexical_cast.hpp>


namespace fs = std::filesystem;

GASAssetManager::GASAssetManager() {}

GASAssetManager::~GASAssetManager()
{
    MemoryCache.clear();
}

GASAssetManager& GASAssetManager::Get()
{
    static GASAssetManager Instance;
    return Instance;
}

bool GASAssetManager::Initialize()
{
    fs::create_directories(GAS_CONFIG::BINARY_CACHE_PATH);

    // 初始化元数据数据库
    if (!MetadataStorage.Initialize(GAS_CONFIG::DATABASE_PATH))
    {
        GAS_LOG_ERROR("Failed to initialize Metadata Database at %s", GAS_CONFIG::DATABASE_PATH);
        return false;
    }

     GAS_LOG("GAS Asset Manager Initialized successfully.");
    return true;
}

// 资产导入与持久化

uint64_t GASAssetManager::ImportAsset(const std::string& SourceFilePath)
{
    std::shared_ptr<GASSkeleton> SkeletonAsset = nullptr;
    std::vector<std::shared_ptr<GASAnimation>> AnimationAssets;
    std::vector<std::shared_ptr<GASMesh>> MeshAssets;

    if (!Importer.ImportFromFile(SourceFilePath, SkeletonAsset, AnimationAssets, MeshAssets))
    {
        GAS_LOG_ERROR("Failed to import asset from %s", SourceFilePath.c_str());
        return 0;
    }

    boost::uuids::name_generator_sha1 gen(boost::uuids::ns::dns());

    boost::uuids::uuid u = gen(SourceFilePath);
    uint64_t* ptr = (uint64_t*)&u;
    uint64_t SkeletonGUID = ptr[0] ^ ptr[1];

    if (SkeletonAsset)
    {
        SkeletonAsset->BaseHeader.AssetGUID = SkeletonGUID;
        SkeletonAsset->BaseHeader.AssetType = EGASAssetType::Skeleton;
        SkeletonAsset->AssetName = fs::path(SourceFilePath).stem().string() + "_Skeleton";

        std::string BinaryPath = (fs::path(GAS_CONFIG::BINARY_CACHE_PATH) / (std::to_string(SkeletonGUID) + ".skeleton.gas")).string();

        if (GASBinarySerializer::SaveAssetToDisk(SkeletonAsset.get(), BinaryPath))
        {
            FGASAssetMetadata Metadata;
            Metadata.GUID = SkeletonGUID;
            Metadata.Name = SkeletonAsset->AssetName;
            Metadata.Type = EGASAssetType::Skeleton;
            Metadata.BinaryFilePath = BinaryPath;
            Metadata.BoneCount = SkeletonAsset->GetNumBones();

            MetadataStorage.RegisterAsset(Metadata);
            std::unique_lock<std::shared_mutex> lock(CacheMutex);
            MemoryCache[SkeletonGUID] = SkeletonAsset;
        }
    }

    for (const auto& AnimAsset : AnimationAssets)
    {
        if (!AnimAsset) continue;

        std::string AnimUniqueKey = SourceFilePath + "_Anim_" + AnimAsset->AssetName;
        boost::uuids::uuid anim_u = gen(AnimUniqueKey);
        uint64_t* anim_ptr = (uint64_t*)&anim_u;
        uint64_t AnimGUID = anim_ptr[0] ^ anim_ptr[1];

        AnimAsset->BaseHeader.AssetGUID = AnimGUID;
        AnimAsset->BaseHeader.AssetType = EGASAssetType::Animation;

        std::string BinaryPath = (fs::path(GAS_CONFIG::BINARY_CACHE_PATH) / (std::to_string(AnimGUID) + ".anim.gas")).string();

        if (GASBinarySerializer::SaveAssetToDisk(AnimAsset.get(), BinaryPath))
        {
            FGASAssetMetadata Metadata;
            Metadata.GUID = AnimGUID;
            Metadata.Name = AnimAsset->AssetName;
            Metadata.Type = EGASAssetType::Animation;
            Metadata.BinaryFilePath = BinaryPath;
            Metadata.FrameCount = AnimAsset->GetNumFrames();
            Metadata.Duration = AnimAsset->GetDuration();

            MetadataStorage.RegisterAsset(Metadata);
            std::unique_lock<std::shared_mutex> lock(CacheMutex);
            MemoryCache[AnimGUID] = AnimAsset;
        }
    }

    for (const auto& MeshAsset : MeshAssets)
    {
        if (!MeshAsset) continue;

        std::string MeshUniqueKey = SourceFilePath + "_Mesh_" + MeshAsset->AssetName;
        boost::uuids::uuid mesh_u = gen(MeshUniqueKey);
        uint64_t* mesh_ptr = (uint64_t*)&mesh_u;
        uint64_t MeshGUID = mesh_ptr[0] ^ mesh_ptr[1];

        MeshAsset->BaseHeader.AssetGUID = MeshGUID;

        if (MeshAsset->HasSkin())
        {
            MeshAsset->BaseHeader.AssetType = EGASAssetType::Mesh;
            if (SkeletonAsset)
            {
                MeshAsset->SkeletonGUID = SkeletonGUID;
            }
        }
        else
        {
            MeshAsset->BaseHeader.AssetType = EGASAssetType::Mesh;
        }

        std::string BinaryPath = (fs::path(GAS_CONFIG::BINARY_CACHE_PATH) / (std::to_string(MeshGUID) + ".mesh.gas")).string();

        if (GASBinarySerializer::SaveAssetToDisk(MeshAsset.get(), BinaryPath))
        {
            FGASAssetMetadata Metadata;
            Metadata.GUID = MeshGUID;
            Metadata.Name = MeshAsset->AssetName;
            Metadata.Type = static_cast<EGASAssetType>(MeshAsset->BaseHeader.AssetType);
            Metadata.BinaryFilePath = BinaryPath;

            MetadataStorage.RegisterAsset(Metadata);
            std::unique_lock<std::shared_mutex> lock(CacheMutex);
            MemoryCache[MeshGUID] = MeshAsset;
        }
    }

    return SkeletonGUID;
}


// 运行时资产加载与缓存
std::shared_ptr<GASAsset> GASAssetManager::GetCachedAsset(uint64_t GUID) const
{
    std::shared_lock<std::shared_mutex> lock(CacheMutex);
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
    //  尝试从缓存获取
    auto CachedAsset = GetCachedAsset(GUID);
    if (CachedAsset)
    {
        return CachedAsset;
    }
    //  缓存未命中，查询元数据获取路径
    FGASAssetMetadata Metadata;
    if (!QueryMetadata(GUID, Metadata))
    {
        GAS_LOG_ERROR("Asset GUID %llu not found in Metadata database.", GUID);
        return nullptr;
    }

    //从磁盘加载 (耗时操作)
    std::shared_ptr<GASAsset> LoadedAsset = GASBinarySerializer::LoadAssetFromDisk(Metadata.BinaryFilePath);
    if (LoadedAsset)
    {
        std::unique_lock<std::shared_mutex> lock(CacheMutex);
        LoadedAsset->AssetName = Metadata.Name; 
        MemoryCache[GUID] = LoadedAsset;
        return LoadedAsset;
    }
    GAS_LOG_ERROR("Failed to load binary file: %s", Metadata.BinaryFilePath);
    return nullptr;
}