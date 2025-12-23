#include "GASAssetManager.h"
#include <sstream>
#include <filesystem>
#include "GASLogging.h"



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
    namespace fs = std::filesystem;
    fs::path SrcPath(SourceFilePath);


    std::vector<uint8_t> FileData = GASFileHelper::ReadRawFile(SourceFilePath);
    uint64_t CurrentFileHash = CalculateXXHash64(FileData.data(), FileData.size());

    // 确定文件夹名称
    std::string FolderName = SrcPath.stem().string();
    uint64_t ExpectedGUID = GenerateGUID64(FolderName);
    fs::path TargetFolder = fs::path(GAS_CONFIG::BINARY_CACHE_PATH) / FolderName;

    if (!fs::exists(TargetFolder)) {
        fs::create_directories(TargetFolder);
    }

    //判断是否是已有文件，是的话跳弹窗检测
    FGASAssetMetadata ExistingMeta;
    if (MetadataStorage.QueryAssetByGUID(ExpectedGUID, ExistingMeta))
    {
        
        if (ExistingMeta.FileHash == CurrentFileHash)
        {
            GAS_LOG("Asset content identical, skipping: %s", SourceFilePath.c_str());
            return ExpectedGUID;
        }
        // 如果内容不一致，说明文件更新了，弹窗询问
        int Decision = ShowConflictDialog(SourceFilePath);

        if (Decision == 0) 
        {
            return ExpectedGUID;
        }

    }

    //  执行导入解析
    std::shared_ptr<GASSkeleton> SkeletonAsset = nullptr;
    std::vector<std::shared_ptr<GASAnimation>> AnimationAssets;
    std::vector<std::shared_ptr<GASMesh>> MeshAssets;

    if (!Importer.ImportFromFile(SourceFilePath, SkeletonAsset, AnimationAssets, MeshAssets))
    {
        GAS_LOG_ERROR("Failed to import asset from %s", SourceFilePath.c_str());
        return 0;
    }

    // 生成 GUID 
    uint64_t SkeletonGUID = GenerateGUID64(FolderName);

    // --- 处理 Skeleton ---
    if (SkeletonAsset)
    {
        SkeletonAsset->BaseHeader.AssetGUID = SkeletonGUID;
        SkeletonAsset->AssetName = FolderName; // 默认使用文件名

        std::string SkeletonFileName = std::to_string(SkeletonGUID) + ".skeleton.gas";
        fs::path FullPath = TargetFolder / SkeletonFileName;
        // 存储到数据库的相对路径: "TestSkin/GUID.skeleton.gas"
        std::string RelativePath = (fs::path(FolderName) / SkeletonFileName).string();

        if (GASBinarySerializer::SaveAssetToDisk(SkeletonAsset.get(), FullPath.string()))
        {
            FGASAssetMetadata Metadata;
            Metadata.GUID = SkeletonGUID;
            Metadata.Name = SkeletonAsset->AssetName;
            Metadata.Type = EGASAssetType::Skeleton;
            Metadata.BinaryFilePath = RelativePath;
            Metadata.BoneCount = SkeletonAsset->GetNumBones();
            Metadata.FileHash = CurrentFileHash;

            MetadataStorage.RegisterAsset(Metadata);

            std::unique_lock<std::shared_mutex> lock(CacheMutex);
            MemoryCache[SkeletonGUID] = SkeletonAsset;
        }
    }

    // --- 处理 Animations ---
    for (size_t i = 0; i < AnimationAssets.size(); ++i)
    {
        auto& AnimAsset = AnimationAssets[i];
        if (!AnimAsset) continue;

        // 生成唯一动画 GUID (基于源文件 + 动画索引/名称)
        std::string AnimKey = FolderName + "_Anim_" + std::to_string(i);

        uint64_t AnimGUID = GenerateGUID64(AnimKey);
        AnimAsset->BaseHeader.AssetGUID = AnimGUID;
        if (AnimAsset->AssetName.empty()) AnimAsset->AssetName = FolderName + "_Anim_" + std::to_string(i);

        std::string AnimFileName = std::to_string(AnimGUID) + ".anim.gas";
        fs::path FullPath = TargetFolder / AnimFileName;
        std::string RelativePath = (fs::path(FolderName) / AnimFileName).string();

        if (GASBinarySerializer::SaveAssetToDisk(AnimAsset.get(), FullPath.string()))
        {
            FGASAssetMetadata Metadata;
            Metadata.GUID = AnimGUID;
            Metadata.Name = AnimAsset->AssetName;
            Metadata.Type = EGASAssetType::Animation;
            Metadata.BinaryFilePath = RelativePath;
            Metadata.FrameCount = AnimAsset->GetNumFrames();
            Metadata.Duration = AnimAsset->GetDuration();
            Metadata.FileHash = CurrentFileHash;
            MetadataStorage.RegisterAsset(Metadata);

            std::unique_lock<std::shared_mutex> lock(CacheMutex);
            MemoryCache[AnimGUID] = AnimAsset;
        }
    }

    // --- 处理 Meshes ---
    for (const auto& MeshAsset : MeshAssets)
    {
        if (!MeshAsset) continue;

        std::string& TexPath = MeshAsset->DiffuseTexturePath;
        if (TexPath.empty())
            // 计算源文件的绝对路径
        {
            fs::path FbxDir = fs::path(SourceFilePath).parent_path();
            fs::path TexSourceAbsPath = FbxDir / TexPath;

            // 目标: .../Assets/Textures/资产名/
            fs::path TexCacheDir = fs::path(GAS_CONFIG::TEXTURE_ARCHIVE_PATH) / FolderName;


            if (!fs::exists(TexCacheDir)) {
                fs::create_directories(TexCacheDir);
            }

            //计算目标文件的绝对路径
            std::string TexFileName = TexSourceAbsPath.filename().string();
            fs::path TexTargetAbsPath = TexCacheDir / TexFileName;

            // 执行文件拷贝
            if (fs::exists(TexSourceAbsPath))
            {
                try {
                    fs::copy_file(TexSourceAbsPath, TexTargetAbsPath, fs::copy_options::overwrite_existing);
                    GAS_LOG("Import: Copied texture to %s", TexTargetAbsPath.string().c_str());
                }
                catch (const std::exception& e) {
                    GAS_LOG_ERROR("Import: Failed to copy texture: %s", e.what());
                }
            }
            else
            {
                GAS_LOG_WARN("Import: Source texture not found at %s", TexSourceAbsPath.string().c_str());
            }

            TexPath = "Textures/" + FolderName + "/" + TexFileName;
        }
        // 生成唯一 Mesh GUID
        std::string MeshKey = FolderName + "_Mesh_" + MeshAsset->AssetName;

        uint64_t MeshGUID = GenerateGUID64(MeshKey);
        MeshAsset->BaseHeader.AssetGUID = MeshGUID;

        // 设置骨骼关联
        if (MeshAsset->HasSkin() && SkeletonAsset) {
            MeshAsset->SkeletonGUID = SkeletonGUID;
            MeshAsset->BaseHeader.AssetType = EGASAssetType::Mesh;
        }
        else {
            MeshAsset->BaseHeader.AssetType = EGASAssetType::Mesh;
        }

        std::string MeshFileName = std::to_string(MeshGUID) + ".mesh.gas";
        fs::path FullPath = TargetFolder / MeshFileName;
        std::string RelativePath = (fs::path(FolderName) / MeshFileName).string();

        if (GASBinarySerializer::SaveAssetToDisk(MeshAsset.get(), FullPath.string()))
        {
            FGASAssetMetadata Metadata;
            Metadata.GUID = MeshGUID;
            Metadata.Name = MeshAsset->AssetName;
            Metadata.Type = static_cast<EGASAssetType>(MeshAsset->BaseHeader.AssetType);
            Metadata.BinaryFilePath = RelativePath;
            Metadata.VerticeCount = MeshAsset->GetNumVertices();
            Metadata.FileHash = CurrentFileHash;
            MetadataStorage.RegisterAsset(Metadata);

            std::unique_lock<std::shared_mutex> lock(CacheMutex);
            MemoryCache[MeshGUID] = MeshAsset;
        }
    }

    GAS_LOG("Import SUCCESS. Main GUID: %llu,FileNme: %s", ExpectedGUID,GASFileHelper::GetFileName(SourceFilePath).c_str());


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

    std::filesystem::path FullPath = std::filesystem::path(GAS_CONFIG::BINARY_CACHE_PATH) / Metadata.BinaryFilePath;
    std::shared_ptr<GASAsset> LoadedAsset = GASBinarySerializer::LoadAssetFromDisk(FullPath.string());
    if (LoadedAsset)
    {
        std::unique_lock<std::shared_mutex> lock(CacheMutex);
        LoadedAsset->AssetName = Metadata.Name;
        LoadedAsset->BaseHeader.AssetGUID = GUID;

        MemoryCache[GUID] = LoadedAsset;
        return LoadedAsset;
    }

    GAS_LOG_ERROR("Failed to load binary file: %s", FullPath.string().c_str());
    return nullptr;
}