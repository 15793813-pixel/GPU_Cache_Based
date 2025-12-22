#include <iostream>
#include <filesystem>
#include "Core/Utils/GASAssetManager.h"
#include "Core/Types/GASConfig.h"
#include "Core/Utils/GASDebug.h"
#include "Editor/GASUI.h"



int main()
{
    //日志系统初始化
    GASLogger::Get().Initialize("Logs/GAS_Log.txt");
    //路径确认
    EnsureDirectoriesExist();
    //数据库初始化
    GASAssetManager::Get().GetGASMetadataStorage().Initialize(GAS_CONFIG::DATABASE_PATH);
    if (!GASUI::Initialize())
    {
        GAS_LOG_ERROR("Failed to initialize Editor UI.");
        return -1;
    }

    // 2. 进入主循环 (RunLoop 会一直运行直到窗口关闭)
    GASUI::RunLoop();

    // 3. 清理
    GASUI::Shutdown();

    return 0;
}