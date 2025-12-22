#pragma once
#include <string>
#include <vector>
#include <map>
#include <assimp/scene.h>
#include <assimp/Importer.hpp>
#include "../Core/Utils/GASMetadataStorage.h"

struct GLFWwindow;

class GASUI
{
public:
    static bool Initialize();
    static void RunLoop();
    static void Shutdown();
    static void OnFileDrop(GLFWwindow* window, int count, const char** paths);

private:
    static void RenderImporterPanel();   // 左侧面板 (导入 + 结果信息)
    static void RenderHierarchyPanel();  // 右侧面板 (树状图)
    static void DrawImGuiTreeRecursive(const aiNode* Node);
    static void RenderScene();

    // --- 新增：更新后的骨骼绘制函数 ---
    static void DrawAssimpNodeRecursive(const aiNode* Node, const aiMatrix4x4& ParentTransform, const std::map<std::string, bool>& BoneMap);
    static void DrawGrid();

    // --- 状态变量 ---
    static GLFWwindow* m_Window;
    static Assimp::Importer m_PreviewImporter;
    static const aiScene* m_PreviewScene;

    // 摄像机
    static float m_CameraDistance;
    static float m_CameraYaw;
    static float m_CameraPitch;

    // 路径与状态
    static std::string m_SourceFilePath;    // 拖入的源文件路径
    static std::string m_StatusMessage;     // 左上角状态栏文字
    static std::vector<FGASAssetMetadata> m_ImportedResults;
    static uint64_t m_CurrentSourceHash;
    static std::map<std::string, bool> m_BoneMap;

    // --- 新增：弹窗控制 ---
    static bool m_ShowDuplicatePopup;       // 是否显示重复文件弹窗
    static std::string m_ExistingAssetPath; // 已存在的资产路径（用于提示）

    // 固定的源文件备份目录
    static const std::string BACKUP_SOURCE_DIR;
};