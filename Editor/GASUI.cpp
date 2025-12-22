#include "GASUI.h"
#include "../Core/Utils/GASLogging.h"         
#include "../Core/Utils/GASDataConverter.h" 
#include "../Core/Utils/GASAssetManager.h" 
#include "../Core/Utils/GASFileHelper.h" // 假设你有这个用于读二进制
#include "../Core/Utils/GASHashManager.h"

#include <filesystem>
#include <GLFW/glfw3.h>
#include "imgui-master/imgui.h"
#include "imgui-master/backends/imgui_impl_glfw.h"
#include "imgui-master/backends/imgui_impl_opengl3.h"
#pragma comment(lib, "opengl32.lib")

namespace fs = std::filesystem;

// --- 静态变量初始化 ---
GLFWwindow* GASUI::m_Window = nullptr;
Assimp::Importer GASUI::m_PreviewImporter;
const aiScene* GASUI::m_PreviewScene = nullptr;
float GASUI::m_CameraDistance = -400.0f;
float GASUI::m_CameraYaw = 0.0f;
float GASUI::m_CameraPitch = 20.0f;
std::string GASUI::m_SourceFilePath = "Drag .fbx here...";
std::string GASUI::m_StatusMessage = "Ready";
std::map<std::string, bool> GASUI::m_BoneMap;
std::string GASUI::m_ConvertedPath = "None";
bool GASUI::m_ShowDuplicatePopup = false;
std::string GASUI::m_ExistingAssetPath = "";

// 定义你的源文件备份目录
const std::string GASUI::BACKUP_SOURCE_DIR = "H:\\GPU_Cache_Based\\MyAnimationSystem\\Assets\\Sources";

// 颜色定义
const FGASVector3 COLOR_BONE(0.0f, 1.0f, 0.0f);    // 绿
const FGASVector3 COLOR_NODE(0.5f, 0.5f, 0.5f);    // 灰
const FGASVector3 COLOR_ROOT(1.0f, 0.0f, 0.0f);    // 红
const FGASVector3 COLOR_VIRTUAL(1.0f, 1.0f, 0.0f); // 黄

// --- 初始化 ---
bool GASUI::Initialize()
{
    if (!glfwInit()) return false;

    // 创建窗口
    m_Window = glfwCreateWindow(1280, 720, "GAS Asset Editor", NULL, NULL);
    if (!m_Window) {
        glfwTerminate();
        return false;
    }
    glfwMakeContextCurrent(m_Window);
    glfwSwapInterval(1); // 开启垂直同步

    // 注册拖拽回调
    glfwSetDropCallback(m_Window, OnFileDrop);

    // 初始化 ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // 允许键盘控制

    ImGui::StyleColorsDark(); // 设置暗色主题

    // 初始化 ImGui 后端
    ImGui_ImplGlfw_InitForOpenGL(m_Window, true);
    ImGui_ImplOpenGL3_Init("#version 130");

    return true;
}

// --- 清理 ---
void GASUI::Shutdown()
{
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    if (m_Window) {
        glfwDestroyWindow(m_Window);
    }
    glfwTerminate();
}

// --- 文件拖拽回调 ---
void GASUI::OnFileDrop(GLFWwindow* window, int count, const char** paths)
{
    if (count > 0)
    {
        m_SourceFilePath = paths[0];
        m_StatusMessage = "Checking File...";
        m_ConvertedPath = "None"; // 重置转换状态
        m_ShowDuplicatePopup = false;

        // 1. 计算 Hash
        std::vector<uint8_t> FileData = GASFileHelper::ReadRawFile(m_SourceFilePath);
        uint64_t CurrentFileHash = CalculateXXHash64(FileData.data(), FileData.size());

        // 2. 预测 GUID (根据你的 ImportAsset 逻辑，GUID 是基于路径生成的)
        // 注意：这里有个逻辑陷阱，如果你的 GUID 是基于路径生成的，那么不同路径的同一个文件 GUID 会不同。
        // 但通常我们会通过 ContentHash 来查重。这里假设 MetadataStorage 支持通过 Hash 查询，或者我们先按路径生成 GUID 查。
        // 为了演示，我们按照你的 AssetManager 逻辑，生成预期 GUID 去查。
        uint64_t ExpectedGUID = GenerateGUID64(m_SourceFilePath);

        FGASAssetMetadata ExistingMeta;
        bool bIsDuplicate = false;

        // 查询 AssetManager
        if (GASAssetManager::Get().GetGASMetadataStorage().QueryAssetByGUID(ExpectedGUID, ExistingMeta))
        {
            // 如果 GUID 存在，进一步检查内容 Hash
            if (ExistingMeta.FileHash == CurrentFileHash)
            {
                bIsDuplicate = true;
                m_ExistingAssetPath = ExistingMeta.BinaryFilePath; // 获取已存在的路径
            }
        }

        if (bIsDuplicate)
        {
            // --- 触发弹窗 ---
            m_ShowDuplicatePopup = true;
            m_StatusMessage = "Duplicate Detected!";
            GAS_LOG("[UI] Duplicate file detected at: %s", m_ExistingAssetPath.c_str());
        }

        // 无论是否重复，我们都加载预览（让用户能看到他拖进来了什么）
        m_PreviewScene = nullptr;
        m_PreviewImporter.FreeScene();
        m_BoneMap.clear();

        const unsigned int flags = aiProcess_Triangulate | aiProcess_SortByPType;
        m_PreviewScene = m_PreviewImporter.ReadFile(m_SourceFilePath, flags);

        if (m_PreviewScene)
        {
            if (!bIsDuplicate) m_StatusMessage = "Ready to Convert";

            // 构建 BoneMap
            if (m_PreviewScene->HasMeshes())
            {
                for (unsigned int i = 0; i < m_PreviewScene->mNumMeshes; ++i)
                {
                    const aiMesh* mesh = m_PreviewScene->mMeshes[i];
                    for (unsigned int b = 0; b < mesh->mNumBones; ++b)
                    {
                        std::string name = GASDataConverter::NormalizeBoneName(mesh->mBones[b]->mName.C_Str());
                        m_BoneMap[name] = true;
                    }
                }
            }
        }
    }
}

void GASUI::RenderUI()
{
    // 设置窗口位置和大小
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(350, ImGui::GetIO().DisplaySize.y));

    ImGui::Begin("Importer Panel", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);

    // --- 1. 状态显示区 ---
    ImGui::TextColored(ImVec4(1, 1, 0, 1), "Source: ");
    ImGui::TextWrapped("%s", m_SourceFilePath.c_str());
    ImGui::Separator();

    ImGui::Text("Status: %s", m_StatusMessage.c_str());

    // 显示转换后的地址
    if (m_ConvertedPath != "None")
    {
        ImGui::TextColored(ImVec4(0, 1, 0, 1), "Output:");
        ImGui::TextWrapped("%s", m_ConvertedPath.c_str());
    }
    ImGui::Separator();

    // --- 2. 转换按钮逻辑 ---
    if (m_PreviewScene)
    {
        // 如果检测到重复，禁用按钮（除非你实现了强制覆盖逻辑）
        ImGui::BeginDisabled(m_ShowDuplicatePopup);

        // 按钮：转换并备份
        if (ImGui::Button("CONVERT TO .GAS & COPY", ImVec2(-1, 50)))
        {
            GAS_LOG("UI: Starting Import Process...");

            // A. 执行转换 (调用 AssetManager)
            uint64_t MainGUID = GASAssetManager::Get().ImportAsset(m_SourceFilePath);

            if (MainGUID != 0)
            {
                // B. 获取转换后的相对路径并显示
                FGASAssetMetadata NewMeta;
                if (GASAssetManager::Get().GetGASMetadataStorage().QueryAssetByGUID(MainGUID, NewMeta))
                {
                    m_ConvertedPath = NewMeta.BinaryFilePath;
                }

                // C. 拷贝源文件到指定目录
                try {
                    fs::path Src(m_SourceFilePath);
                    fs::path DestDir(BACKUP_SOURCE_DIR);

                    if (!fs::exists(DestDir)) fs::create_directories(DestDir);

                    fs::path DestFile = DestDir / Src.filename();
                    fs::copy_file(Src, DestFile, fs::copy_options::overwrite_existing);

                    GAS_LOG("UI: Source file backed up to %s", DestFile.string().c_str());
                    m_StatusMessage = "Import & Backup Success!";
                    ImGui::OpenPopup("SuccessPopup");
                }
                catch (std::exception& e) {
                    GAS_LOG_ERROR("UI: Backup failed: %s", e.what());
                    m_StatusMessage = "Import OK, Backup Failed";
                }
            }
            else
            {
                m_StatusMessage = "Import Failed!";
            }
        }
        ImGui::EndDisabled();
    }
    else
    {
        ImGui::TextDisabled("Drag .fbx file to start...");
    }

    // --- 3. 弹窗处理 ---

    // 成功弹窗
    if (ImGui::BeginPopup("SuccessPopup")) {
        ImGui::Text("Asset Imported Successfully!");
        ImGui::Text("Location: %s", m_ConvertedPath.c_str());
        if (ImGui::Button("Close")) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    // 重复文件警告弹窗
    if (m_ShowDuplicatePopup)
    {
        ImGui::OpenPopup("Duplicate File Detected");
    }

    if (ImGui::BeginPopupModal("Duplicate File Detected", NULL, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::Text("This file already exists in the library!");
        ImGui::Separator();
        ImGui::TextColored(ImVec4(1, 0, 0, 1), "Existing Path:");
        ImGui::TextWrapped("%s", m_ExistingAssetPath.c_str());

        ImGui::Separator();

        if (ImGui::Button("Cancel Import", ImVec2(120, 0)))
        {
            m_ShowDuplicatePopup = false;
            m_StatusMessage = "Import Cancelled";
            ImGui::CloseCurrentPopup();
        }

        ImGui::SameLine();
        if (ImGui::Button("Ignore & Continue", ImVec2(120, 0)))
        {
            m_ShowDuplicatePopup = false;
            m_StatusMessage = "Duplicate Warning Ignored";
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }

    // --- 4. 树状图显示 ---
    if (m_PreviewScene)
    {
        ImGui::Separator();
        ImGui::Text("Scene Hierarchy:");
        ImGui::BeginChild("TreeRegion", ImVec2(0, -1), true, ImGuiWindowFlags_HorizontalScrollbar);
        if (m_PreviewScene->mRootNode)
        {
            // 这里调用你的递归绘制 ImGui 树的函数
            // 注意：需要在类里定义 DrawImGuiTreeRecursive，或者直接在这里实现
            // 假设你之前已经写了这个辅助函数，如果没有，请参考之前的回答补充
            // DrawImGuiTreeRecursive(m_PreviewScene->mRootNode); 
        }
        ImGui::EndChild();
    }

    ImGui::End();
}
// --- 主循环 ---
void GASUI::RunLoop()
{
    while (!glfwWindowShouldClose(m_Window))
    {
        glfwPollEvents();

        // 1. 处理鼠标滚轮缩放摄像机 (简单实现)
        if (ImGui::GetIO().MouseWheel != 0.0f) {
            m_CameraDistance += ImGui::GetIO().MouseWheel * 20.0f;
        }

        // 2. ImGui 新帧
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // 3. 渲染 UI
        RenderUI();

        // 4. 渲染 ImGui 数据生成
        ImGui::Render();

        // 5. 渲染 3D 场景 (OpenGL)
        int display_w, display_h;
        glfwGetFramebufferSize(m_Window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f); // 深灰背景
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // 设置透视投影
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        float aspect = (float)display_w / (float)display_h;
        // 简单的 gluPerspective 替代品
        float fov = 45.0f * 3.14159f / 180.0f;
        float f = 1.0f / tan(fov / 2.0f);
        float zNear = 0.1f, zFar = 5000.0f;
        float m[16] = { f / aspect, 0, 0, 0,  0, f, 0, 0,  0, 0, (zFar + zNear) / (zNear - zFar), -1,  0, 0, (2 * zFar * zNear) / (zNear - zFar), 0 };
        glMultMatrixf(m);

        // 设置视图矩阵
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        glTranslatef(0.0f, -100.0f, m_CameraDistance);
        glRotatef(m_CameraPitch, 1.0f, 0.0f, 0.0f);
        glRotatef(m_CameraYaw += 0.2f, 0.0f, 1.0f, 0.0f); // 自动旋转演示，实际可以用鼠标控制

        // 画网格
        DrawGrid();

        // 画骨骼
        RenderScene();

        // 6. 叠加 ImGui
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(m_Window);
    }
}

// 静态辅助：画线
void DrawLine(const FGASVector3& Start, const FGASVector3& End, const FGASVector3& Color)
{
    glLineWidth(2.0f);
    glBegin(GL_LINES);
    glColor3f(Color.x, Color.y, Color.z);
    glVertex3f(Start.x, Start.y, Start.z);
    glVertex3f(End.x, End.y, End.z);
    glEnd();
}

void GASUI::DrawAssimpNodeRecursive(const aiNode* Node, const aiMatrix4x4& ParentTransform, const std::map<std::string, bool>& BoneMap)
{
    // 1. 计算全局矩阵
    aiMatrix4x4 CurrentGlobal = ParentTransform * Node->mTransformation;

    // 2. 计算位置
    aiVector3D Origin(0, 0, 0);
    aiVector3D CurrPos = CurrentGlobal * Origin;
    aiVector3D ParentPos = ParentTransform * Origin;

    // 3. 决定颜色
    std::string NodeName = Node->mName.C_Str();
    std::string CleanName = GASDataConverter::NormalizeBoneName(NodeName);

    FGASVector3 Color = COLOR_NODE; // 默认灰
    if (Node->mParent == nullptr) Color = COLOR_ROOT; // 根节点红
    else if (BoneMap.count(CleanName)) Color = COLOR_BONE; // 骨骼绿
    else if (NodeName.find("$AssimpFbx$") != std::string::npos) Color = COLOR_VIRTUAL; // 虚拟节点黄

    // 4. 绘制连接线 (连接父子)
    if (Node->mParent)
    {
        FGASVector3 P1 = GASDataConverter::ToVector3(ParentPos);
        FGASVector3 P2 = GASDataConverter::ToVector3(CurrPos);

        float DistSq = (P1 - P2).LengthSquared();

        // A. 如果有长度，画线
        if (DistSq > 0.0001f) {
            DrawLine(P1, P2, Color);
        }
        // B. 如果是纯旋转节点（长度为0），画一个大点
        else {
            glPointSize(8.0f);
            glBegin(GL_POINTS);
            // 使用显眼的颜色（比如黄色）或者跟节点同色
            glColor3f(COLOR_VIRTUAL.x, COLOR_VIRTUAL.y, COLOR_VIRTUAL.z);
            glVertex3f(P2.x, P2.y, P2.z);
            glEnd();
            glPointSize(1.0f); // 还原
        }
    }

    // 5. 递归
    for (unsigned int i = 0; i < Node->mNumChildren; ++i)
    {
        DrawAssimpNodeRecursive(Node->mChildren[i], CurrentGlobal, BoneMap);
    }
}

void GASUI::RenderScene()
{
    if (m_PreviewScene && m_PreviewScene->mRootNode)
    {
        aiMatrix4x4 Identity; // 单位矩阵
        DrawAssimpNodeRecursive(m_PreviewScene->mRootNode, Identity, m_BoneMap);
    }
}

// --- 画网格 ---
void GASUI::DrawGrid()
{
    glLineWidth(1.0f);
    glBegin(GL_LINES);
    glColor3f(0.3f, 0.3f, 0.3f);
    for (int i = -10; i <= 10; ++i) {
        glVertex3f((float)i * 100, 0, -1000);
        glVertex3f((float)i * 100, 0, 1000);
        glVertex3f(-1000, 0, (float)i * 100);
        glVertex3f(1000, 0, (float)i * 100);
    }
    glEnd();
}