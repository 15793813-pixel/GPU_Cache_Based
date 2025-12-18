#pragma once
#include <GLFW/glfw3.h>
#include "../Types/GASArray.h"
#include "../Types/GASConfig.h"
#include "GASAssetManager.h"
#include "GASDataConverter.h"


namespace fs = std::filesystem;

void EnsureDirectoriesExist();

bool RunImportTest(const std::string& SourceFBX);

class GASAssimpLogStream : public Assimp::LogStream
{
public:
    inline void write(const char* message) override
    {
        printf("[Assimp Internal] %s", message);
    }
};

class GASDebugAssimp
{
public:
    // 调用这个函数，会阻塞当前线程并弹出一个窗口显示骨骼
    static void ShowDebugWindow(const aiScene* Scene, const std::map<std::string, bool>& BoneMap);

private:
    // 递归绘制辅助函数
    static void DrawNodeHierarchy(const aiNode* Node, const aiMatrix4x4& ParentGlobalTransform, const std::map<std::string, bool>& BoneMap);

    // 具体的画线实现
    static void DrawLine(const FGASVector3& Start, const FGASVector3& End, const FGASVector3& Color);
};