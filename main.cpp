#include <iostream>
#include <filesystem>
#include "Core/Utils/GASAssetManager.h"
#include "Core/Types/GASConfig.h"
#include "Core/Utils/GASDebug.h"


int main()
{
    //日志系统初始化
    GASLogger::Get().Initialize("Logs/GAS_Log.txt");
    //路径确认
    EnsureDirectoriesExist();
    //数据库初始化
    GASAssetManager::Get().GetGASMetadataStorage().Initialize(GAS_CONFIG::DATABASE_PATH);

    RunImportTest("Assets/Raw/TestSkin.fbx");
    RunImportTest("Assets/Raw/TestSkin.fbx");

    return 0;
}