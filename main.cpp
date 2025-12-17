#include <iostream>
#include <filesystem>
#include "Core/Utils/GASAssetManager.h"
#include "Core/Types/GASConfig.h"
#include "Core/Utils/GASDebug.h"

namespace fs = std::filesystem;


int main()
{
    GASLogger::Get().Initialize("Logs/GAS_Log.txt");
    std::cout << "==========================================" << std::endl;
    std::cout << "    GAS Animation System - Testing Suite   " << std::endl;
    std::cout << "==========================================" << std::endl;
    EnsureDirectoriesExist();

    if (!GASAssetManager::Get().GetGASMetadataStorage().Initialize(GAS_CONFIG::DATABASE_PATH))
    {
        std::cerr << "[Main] Failed to initialize Metadata Database." << std::endl;
        return -1;
    }

    RunImportTest("Assets/Raw/TestSkin.fbx");
    RunImportTest("Assets/Raw/TestSkin.fbx");

    std::cout << "\n==========================================" << std::endl;
    std::cout << "             All Tests Finished           " << std::endl;
    std::cout << "==========================================" << std::endl;

    return 0;
}