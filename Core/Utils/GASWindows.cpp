#include "GASWindows.h"

int ShowConflictDialog(const std::string& FilePath)
{
    std::string FileName = std::filesystem::path(FilePath).filename().string();
    std::string Msg = "检测到文件内容已变更：\n" + FileName + "\n\n是否覆盖现有资产并重新导入？";

    int Result = MessageBoxA(
        NULL,
        Msg.c_str(),
        "资产冲突",
        MB_YESNO | MB_ICONQUESTION | MB_TOPMOST
    );

    return (Result == IDYES) ? 1 : 0; 
}