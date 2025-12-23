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
    static bool Initialize();//初始化
    static void RunLoop();//glfw渲染循环
    static void Shutdown();//关闭
    static void OnFileDrop(GLFWwindow* window, int count, const char** paths);//文件拖拽

private:
    //画地面参考网格：
    static void DrawLine(const FGASVector3& Start, const FGASVector3& End, const FGASVector3& Color);
    static void DrawGrid();

    static void RenderImporterPanel();   // 左侧面板 
    static void RenderHierarchyPanel();  // 右侧面板 

    //递归画骨骼树
    static void DrawImGuiTreeRecursive(const aiNode* Node);

    // 骨骼绘制函数 
    static void DrawAssimpNodeRecursive(const aiNode* Node, const aiMatrix4x4& ParentTransform, const std::map<std::string, bool>& BoneMap);
    
    //在备份目录查找源文件
    static std::string FindBackupFile(const std::string& AssetName);

    //加载现有资产到编辑器
    static void LoadExistingAsset(const std::string& AssetName, uint64_t AssetHash);

    // --- 状态变量 ---
    static GLFWwindow* m_Window;
    static Assimp::Importer m_PreviewImporter;
    static const aiScene* m_PreviewScene;

    // 摄像机
    static float m_CameraDistance;
    static float m_CameraYaw;
    static float m_CameraPitch;

    // 路径
    static const std::string BACKUP_SOURCE_DIR;// 固定的源文件备份目录
    static std::string m_SourceFilePath;    // 拖入的源文件路径
    static std::string m_ExistingAssetPath; // 已存在的资产路径

    //文字与弹窗：
    static std::string m_StatusMessage;     // 左上角状态栏文字
    static bool m_ShowDuplicatePopup;       // 是否显示重复文件弹窗

    //文件相关：
    static std::vector<FGASAssetMetadata> m_ImportedResults;//导入后的结果
    static uint64_t m_CurrentSourceHash;//当前导入的文件哈希
    static std::map<std::string, bool> m_BoneMap;//存入骨骼是否有权重
    static std::vector<FGASAssetMetadata> m_DatabaseAssetList;//当前所有的.gsa文件的信息
    
};