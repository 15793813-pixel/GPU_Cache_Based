#pragma once
#include"GASDebug.h"
#pragma comment(lib, "opengl32.lib")
#pragma comment(lib, "glu32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "kernel32.lib")


const FGASVector3 COLOR_BONE(0.0f, 1.0f, 0.0f);   // 绿：骨骼
const FGASVector3 COLOR_NODE(0.5f, 0.5f, 0.5f);   // 灰：虚拟节点
const FGASVector3 COLOR_ROOT(1.0f, 0.0f, 0.0f);   // 红：根
const FGASVector3 COLOR_VirtualBone(1.0f, 1.0f, 0.0f);//虚拟节点-黄色

// 简单的摄像机控制变量
float CameraAngleX = 0.0f;
float CameraAngleY = 0.0f;
float CameraZoom = -400.0f; // 根据模型大小可能需要调整这个值(比如 -2.0f 或 -500.0f)

// 辅助函数：确保目录存在
void EnsureDirectoriesExist()
{
    // 递归创建目录
    try {
        if (!fs::exists(GAS_CONFIG::BINARY_CACHE_PATH)) {
            fs::create_directories(GAS_CONFIG::BINARY_CACHE_PATH);
            // 使用 GAS_LOG 记录成功信息
            GAS_LOG("Created binary cache directory: %s", GAS_CONFIG::BINARY_CACHE_PATH);
        }
        // 确保数据库目录也存在 (Metadata.db 的父目录)
        fs::path DBPath(GAS_CONFIG::DATABASE_PATH);
        if (!fs::exists(DBPath.parent_path())) {
            fs::create_directories(DBPath.parent_path());
            GAS_LOG("Created database directory: %s", DBPath.parent_path().string().c_str());
        }
    }
    catch (const std::exception& e) {
        // 使用 GAS_LOG_ERROR 记录异常，宏会自动附加文件名和行号
        GAS_LOG_ERROR("Exception during directory creation: %s", e.what());
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

void GASDebugAssimp::DrawLine(const FGASVector3& Start, const FGASVector3& End, const FGASVector3& Color)
{
    // 使用 OpenGL 立即模式画线
    glLineWidth(2.0f); // 线宽
    glBegin(GL_LINES);
    glColor3f(Color.x, Color.y, Color.z);
    glVertex3f(Start.x, Start.y, Start.z);
    glVertex3f(End.x, End.y, End.z);
    glEnd();
}

void GASDebugAssimp::DrawNodeHierarchy(const aiNode* Node, const aiMatrix4x4& ParentGlobalTransform, const std::map<std::string, bool>& BoneMap)
{
    // 1. 计算全局矩阵
    aiMatrix4x4 CurrentGlobal = ParentGlobalTransform * Node->mTransformation;

    // 2. 计算当前位置
    aiVector3D Origin(0, 0, 0);
    aiVector3D CurrPos = CurrentGlobal * Origin;
    aiVector3D ParentPos = ParentGlobalTransform * Origin;

    // 3. 决定颜色
    std::string NodeName = Node->mName.C_Str();
    std::string CleanName = GASDataConverter::NormalizeBoneName(NodeName);

    FGASVector3 Color = COLOR_NODE;
    if (Node->mParent == nullptr) Color = COLOR_ROOT;
    else if (BoneMap.count(CleanName)) Color = COLOR_BONE;
    else if (NodeName.find("$AssimpFbx$") != std::string::npos) Color = FGASVector3(1, 1, 0); // 黄色虚拟节点

    // 4. 画线 (连接父子)
    if (Node->mParent)
    {
        FGASVector3 P1 = GASDataConverter::ToVector3(ParentPos);
        FGASVector3 P2 = GASDataConverter::ToVector3(CurrPos);

        float DistSq = (P1 - P2).LengthSquared();

        // 如果距离足够长，画线
        if (DistSq > 0.0001f) {
            DrawLine(P1, P2, Color);
        }
        // 【关键修复】如果距离几乎为0 (比如纯旋转节点)，画一个大点
        else {
            glPointSize(8.0f); // 设置点的大小
            glBegin(GL_POINTS);
            glColor3f(COLOR_VirtualBone.x, COLOR_VirtualBone.y, COLOR_VirtualBone.z);
            glVertex3f(P2.x, P2.y, P2.z);
            glEnd();
            glPointSize(1.0f); // 还原默认大小
        }
    }

    // 5. 递归
    for (unsigned int i = 0; i < Node->mNumChildren; ++i)
    {
        DrawNodeHierarchy(Node->mChildren[i], CurrentGlobal, BoneMap);
    }
}

void GASDebugAssimp::ShowDebugWindow(const aiScene* Scene, const std::map<std::string, bool>& BoneMap)
{
    // 1. 初始化 GLFW
    if (!glfwInit()) {
        std::cerr << "Failed to init GLFW" << std::endl;
        return;
    }

    // 2. 创建窗口 (800x600)
    GLFWwindow* window = glfwCreateWindow(800, 600, "GAS Skeleton Debugger", NULL, NULL);
    if (!window) {
        glfwTerminate();
        return;
    }
    glfwMakeContextCurrent(window);

    // 3. 主循环
    while (!glfwWindowShouldClose(window))
    {
        // --- 渲染设置 ---
        int width, height;
        glfwGetFramebufferSize(window, &width, &height);
        glViewport(0, 0, width, height);
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f); // 深灰背景
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // --- 简单的透视投影 ---
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        float aspect = (float)width / (float)height;
        // 手动创建一个简单的透视矩阵 (或者用 gluPerspective 如果你有 GLU)
        float fov = 45.0f * 3.14159f / 180.0f;
        float f = 1.0f / tan(fov / 2.0f);
        float zNear = 0.1f, zFar = 1000.0f;
        float m[16] = {
            f / aspect, 0, 0, 0,
            0, f, 0, 0,
            0, 0, (zFar + zNear) / (zNear - zFar), -1,
            0, 0, (2 * zFar * zNear) / (zNear - zFar), 0
        };
        glMultMatrixf(m);

        // --- 摄像机视图 (简单的旋转) ---
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();

        // 简单的自动旋转
        CameraAngleY += 0.5f;

        // 移远一点看模型 (这里的 CameraZoom 需要根据你的模型单位调整，Mixamo通常是厘米，可能需要 -200)
        glTranslatef(0.0f, -100.0f, CameraZoom);
        glRotatef(20.0f, 1.0f, 0.0f, 0.0f); // 俯视一点
        glRotatef(CameraAngleY, 0.0f, 1.0f, 0.0f); // 旋转

        // --- 绘制坐标轴辅助线 ---
        glBegin(GL_LINES);
        glColor3f(1, 0, 0); glVertex3f(0, 0, 0); glVertex3f(100, 0, 0); // X 红
        glColor3f(0, 1, 0); glVertex3f(0, 0, 0); glVertex3f(0, 100, 0); // Y 绿
        glColor3f(0, 0, 1); glVertex3f(0, 0, 0); glVertex3f(0, 0, 100); // Z 蓝
        glEnd();

        // --- 核心：绘制 Assimp 骨骼 ---
        if (Scene && Scene->mRootNode) {
            aiMatrix4x4 Identity;
            DrawNodeHierarchy(Scene->mRootNode, Identity, BoneMap);
        }

        // --- 交换缓冲 ---
        glfwSwapBuffers(window);
        glfwPollEvents();

        // 处理退出
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
            glfwSetWindowShouldClose(window, true);
    }

    glfwTerminate();
}