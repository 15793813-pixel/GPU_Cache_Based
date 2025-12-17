#pragma once
#include"GASDebug.h"


// 辅助函数：确保目录存在
void EnsureDirectoriesExist()
{
    // 递归创建目录
    try {\
        if (!fs::exists(GAS_CONFIG::BINARY_CACHE_PATH)) {
            fs::create_directories(GAS_CONFIG::BINARY_CACHE_PATH);
            std::cout << "[System] Created binary cache directory: " << GAS_CONFIG::BINARY_CACHE_PATH << std::endl;
        }

        // 确保数据库目录也存在 (Metadata.db 的父目录)
        fs::path DBPath(GAS_CONFIG::DATABASE_PATH);
        if (!fs::exists(DBPath.parent_path())) {
            fs::create_directories(DBPath.parent_path());
        }
    }
    catch (const std::exception& e) {
        std::cerr << "[System] Error creating directories: " << e.what() << std::endl;
    }
}

bool RunImportTest(const std::string& SourceFBX)
{
    std::cout << "\n------------------------------------------" << std::endl;
    std::cout << "[Test] Starting test for: " << SourceFBX << std::endl;

    if (!fs::exists(SourceFBX))
    {
        std::cerr << "[Test] Error: Source file not found: " << SourceFBX << std::endl;
        return false;
    }
    GASAssetManager& AssetManager = GASAssetManager::Get();

    std::cout << "[Test] Importing..." << std::endl;
    uint64_t AssetGUID = AssetManager.ImportAsset(SourceFBX);

    if (AssetGUID == 0)
    {
        std::cerr << "[Test] Import FAILED for " << SourceFBX << std::endl;
        return false;
    }
    std::cout << "[Test] Import SUCCESS. Main GUID: " << AssetGUID << std::endl;
    std::cout << "[Test] Verifying data from disk..." << std::endl;
    FGASAssetMetadata Meta;
    if (AssetManager.GetGASMetadataStorage().QueryAssetByGUID(AssetGUID, Meta))
    {
        auto LoadedAsset = AssetManager.LoadAsset(AssetGUID); 

        if (LoadedAsset)
        {
            std::cout << "[Test] Verification SUCCESS!" << std::endl;
            std::cout << "       - Asset Name: " << LoadedAsset->AssetName << std::endl;
            std::cout << "[Test] Verification SUCCESS!" << std::endl;
            std::cout << "       - Asset Name: " << (LoadedAsset->AssetName.empty() ? "N/A" : LoadedAsset->AssetName) << std::endl;
            std::cout << "       - Type ID: " << (int)LoadedAsset->GetType() << std::endl;
            if (LoadedAsset->GetType() == EGASAssetType::Skeleton)
            {
                auto Skel = std::static_pointer_cast<GASSkeleton>(LoadedAsset);
                std::cout << "       - Bone Count: " << Skel->GetNumBones() << std::endl;
            }
            return true;
        }

        else
        {
            std::cerr << "[Test] Verification FAILED: Binary file corrupted or not found at " << SourceFBX<< std::endl;
        }
    }
    else
    {
        std::cerr << "[Test] Verification FAILED: Metadata for GUID " << AssetGUID << " missing in DB." << std::endl;
    }

    return false;
}
