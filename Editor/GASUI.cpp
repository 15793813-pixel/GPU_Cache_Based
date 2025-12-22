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
std::vector<FGASAssetMetadata> GASUI::m_ImportedResults;
uint64_t GASUI::m_CurrentSourceHash = 0;
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
        m_ImportedResults.clear(); // 清空上次的结果
        m_ShowDuplicatePopup = false;

        // 计算 Hash
        std::vector<uint8_t> FileData = GASFileHelper::ReadRawFile(m_SourceFilePath);
        m_CurrentSourceHash = CalculateXXHash64(FileData.data(), FileData.size()); // 保存 Hash!

        uint64_t ExpectedGUID = GenerateGUID64(m_SourceFilePath);

        FGASAssetMetadata ExistingMeta;
        bool bIsDuplicate = false;

        // 查询 AssetManager
        if (GASAssetManager::Get().GetGASMetadataStorage().QueryAssetByGUID(ExpectedGUID, ExistingMeta))
        {
            // 如果 GUID 存在，进一步检查内容 Hash
            if (ExistingMeta.FileHash == m_CurrentSourceHash)
            {
                bIsDuplicate = true;
                m_ExistingAssetPath = ExistingMeta.BinaryFilePath; 
            }
        }

        if (bIsDuplicate)
        {
            // --- 触发弹窗 ---
            m_ShowDuplicatePopup = true;
            m_StatusMessage = "Duplicate Detected!";
            GAS_LOG("[UI] Duplicate file detected at: %s", m_ExistingAssetPath.c_str());
        }

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


void GASUI::RenderImporterPanel()
{
    // 设置面板位置和大小 (固定左侧)
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(350, ImGui::GetIO().DisplaySize.y));

    ImGui::Begin("Importer Panel", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);

    // --- A. 源文件信息 ---
    ImGui::TextColored(ImVec4(1, 1, 0, 1), "Source File:");
    ImGui::TextWrapped("%s", m_SourceFilePath.c_str());
    ImGui::Separator();
    ImGui::Text("Status: %s", m_StatusMessage.c_str());
    ImGui::Separator();

    // --- B. 操作按钮 ---
    if (m_PreviewScene)
    {
        ImGui::BeginDisabled(m_ShowDuplicatePopup);
        // 按钮高度设为 40
        if (ImGui::Button("CONVERT TO .GAS", ImVec2(-1, 40)))
        {
            GAS_LOG("UI: Starting Import...");

            // 1. 执行导入
            uint64_t MainGUID = GASAssetManager::Get().ImportAsset(m_SourceFilePath);

            if (MainGUID != 0)
            {
                // 2. 拷贝备份 (保持原有逻辑)
                try {
                    fs::path Src(m_SourceFilePath);
                    fs::path DestDir(BACKUP_SOURCE_DIR);
                    if (!fs::exists(DestDir)) fs::create_directories(DestDir);
                    fs::copy_file(Src, DestDir / Src.filename(), fs::copy_options::overwrite_existing);
                }
                catch (...) {}

                // 3. 查询生成结果 (这里会调用刚才修改的 QueryAssetsByFileHash)
                m_ImportedResults.clear();
                GASAssetManager::Get().GetGASMetadataStorage().QueryAssetsByFileHash(m_CurrentSourceHash, m_ImportedResults);

                m_StatusMessage = "Import Success!";
                ImGui::OpenPopup("SuccessPopup");
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

    // --- C. 输出结果列表 (Output List) ---
    // 这里是你要修改的核心部分
    if (!m_ImportedResults.empty())
    {
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::TextColored(ImVec4(0, 1, 0, 1), "Output Assets (%d):", (int)m_ImportedResults.size());

        // 创建一个滚动区域显示列表
        ImGui::BeginChild("OutputList", ImVec2(0, -1), true);

        for (const auto& Asset : m_ImportedResults)
        {

            std::string FileName = fs::path(Asset.BinaryFilePath).filename().string();

            ImVec4 TypeColor = ImVec4(0.8f, 0.8f, 0.8f, 1);
            std::string TypeLabel = "[UNK]";

            switch (Asset.Type)
            {
            case EGASAssetType::Skeleton:
                TypeColor = ImVec4(0.2f, 1.0f, 0.2f, 1.0f); // 亮绿
                TypeLabel = "[SKEL]";
                break;
            case EGASAssetType::Mesh:
                TypeColor = ImVec4(0.2f, 0.6f, 1.0f, 1.0f); // 亮蓝
                TypeLabel = "[MESH]";
                break;
            case EGASAssetType::Animation:
                TypeColor = ImVec4(1.0f, 0.6f, 0.0f, 1.0f); // 橙色
                TypeLabel = "[ANIM]";
                break;
            }

            //  显示标题行
            ImGui::TextColored(TypeColor, "%s", TypeLabel.c_str());
            ImGui::SameLine();
            ImGui::Text("%s", FileName.c_str());

            // 显示详细数据 
            ImGui::Indent(15.0f);
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.7f, 0.7f, 1.0f)); // 使用灰色显示详情

            // 骨骼信息 
            if (Asset.Type == EGASAssetType::Skeleton)
            {
                ImGui::BulletText("Bone Count: %d", Asset.BoneCount);
            }
            //  网格信息 
            else if (Asset.Type == EGASAssetType::Mesh)
            {
                ImGui::BulletText("Vertices:   %d", Asset.VerticeCount);
                ImGui::BulletText("SubMeshes:  %d", Asset.MeshCount);
            }
            // --- 动画信息 ---
            else if (Asset.Type == EGASAssetType::Animation)
            {
                ImGui::BulletText("Frames:     %d", Asset.FrameCount);
                ImGui::BulletText("Duration:   %.3f s", Asset.Duration);
                float FPS = (Asset.Duration > 0.0001f) ? ((float)Asset.FrameCount / Asset.Duration) : 0.0f;
                ImGui::BulletText("FrameRate:  %.1f fps", FPS);
            }

            ImGui::PopStyleColor(); // 恢复颜色
            ImGui::Unindent(15.0f);

            ImGui::Separator(); // 分割线
        }
        ImGui::EndChild();
    }
    if (ImGui::BeginPopup("SuccessPopup")) {
        ImGui::Text("Import Completed!");
        if (ImGui::Button("Close")) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
    // Duplicate 弹窗逻辑保持不变...
    if (m_ShowDuplicatePopup) { /* ... */ }
    if (ImGui::BeginPopupModal("Duplicate File Detected", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
        // ... 原有逻辑 ...
        ImGui::EndPopup();
    }

    ImGui::End();
}

// 右侧面板：场景层级 (Scene Hierarchy)
void GASUI::RenderHierarchyPanel()
{
    // 如果没有预览场景，不绘制右侧面板，或者绘制空的
    if (!m_PreviewScene) return;

    float PanelWidth = 300.0f;
    float DisplayW = ImGui::GetIO().DisplaySize.x;
    float DisplayH = ImGui::GetIO().DisplaySize.y;

    // 固定在右侧
    ImGui::SetNextWindowPos(ImVec2(DisplayW - PanelWidth, 0));
    ImGui::SetNextWindowSize(ImVec2(PanelWidth, DisplayH));

    ImGui::Begin("Scene Hierarchy", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);

    if (m_PreviewScene && m_PreviewScene->mRootNode)
    {
        // 统计信息移到这里也不错
        if (ImGui::CollapsingHeader("Stats", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::Text("Meshes: %d", m_PreviewScene->mNumMeshes);
            ImGui::Text("Anims:  %d", m_PreviewScene->mNumAnimations);
        }
        ImGui::Separator();

        // 树状图滚动区
        ImGui::BeginChild("TreeScroll", ImVec2(0, -1), false, ImGuiWindowFlags_HorizontalScrollbar);
        DrawImGuiTreeRecursive(m_PreviewScene->mRootNode);
        ImGui::EndChild();
    }

    ImGui::End();
}


// 更新 RunLoop
void GASUI::RunLoop()
{
    while (!glfwWindowShouldClose(m_Window))
    {
        glfwPollEvents();

        // 摄像机控制
        if (ImGui::GetIO().MouseWheel != 0.0f) {
            m_CameraDistance += ImGui::GetIO().MouseWheel * 20.0f;
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // --- 渲染 UI ---
        RenderImporterPanel();  // 左
        RenderHierarchyPanel(); // 右

        ImGui::Render();

        // --- 渲染 3D ---
        int display_w, display_h;
        glfwGetFramebufferSize(m_Window, &display_w, &display_h);

        // 视口调整：中间的 3D 区域不应该被 UI 遮挡
        // 简单做法：全屏渲染 3D，UI 盖在上面 (目前做法)
        // 进阶做法：调整 glViewport 只渲染中间区域 (左边350，右边300，中间是3D)
        // 这里为了简单，我们还是全屏渲染，但把相机中心偏移一下可能更好

        glViewport(0, 0, display_w, display_h);
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        float aspect = (float)display_w / (float)display_h;
        float fov = 45.0f * 3.14159f / 180.0f;
        float f = 1.0f / tan(fov / 2.0f);
        float zNear = 0.1f, zFar = 5000.0f;
        float m[16] = { f / aspect, 0, 0, 0,  0, f, 0, 0,  0, 0, (zFar + zNear) / (zNear - zFar), -1,  0, 0, (2 * zFar * zNear) / (zNear - zFar), 0 };
        glMultMatrixf(m);

        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        glTranslatef(0.0f, -100.0f, m_CameraDistance);
        glRotatef(m_CameraPitch, 1.0f, 0.0f, 0.0f);
        glRotatef(m_CameraYaw += 0.2f, 0.0f, 1.0f, 0.0f);

        DrawGrid();
        RenderScene();

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

// ---------------------------------------------------------
// 在 GASUI.cpp 中添加
// ---------------------------------------------------------

void GASUI::DrawImGuiTreeRecursive(const aiNode* Node)
{
    if (!Node) return;

    // 1. 获取并处理节点名称
    std::string NodeName = Node->mName.C_Str();
    if (NodeName.empty()) NodeName = "Unnamed_Node";

    // 规范化名称以便查表
    std::string CleanName = GASDataConverter::NormalizeBoneName(NodeName);

    // 2. 决定文字颜色
    bool bColorPushed = false;

    if (m_BoneMap.count(CleanName))
    {
        // 有权重的真骨骼 -> 亮绿色
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.2f, 1.0f, 0.2f, 1.0f));
        bColorPushed = true;
    }
    else if (NodeName.find("_$AssimpFbx$_") != std::string::npos)
    {
        // Assimp 生成的虚拟节点 -> 黄色 (提醒注意)
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 0.0f, 1.0f));
        bColorPushed = true;
    }
    // else: 普通节点保持默认颜色

    // 3. 设置 Tree Node 标记
    // OpenOnArrow: 点击箭头展开，点击名字选中
    // SpanAvailWidth:以此行背景全宽响应点击
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_SpanAvailWidth;

    // 如果没有子节点，标记为 Leaf (显示为圆点，而不是箭头)
    if (Node->mNumChildren == 0)
    {
        flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_Bullet;
    }

    // 4. 绘制节点
    // 使用指针 (void*)Node 作为唯一 ID，防止同名节点导致 UI 冲突
    bool bNodeOpen = ImGui::TreeNodeEx((void*)Node, flags, "%s", NodeName.c_str());

    // 5. 恢复颜色栈 (必须在 TreeNodeEx 之后立即恢复)
    if (bColorPushed)
    {
        ImGui::PopStyleColor();
    }

    // 6. 鼠标悬停显示详细信息 (Tooltip)
    if (ImGui::IsItemHovered())
    {
        ImGui::BeginTooltip();
        ImGui::Text("Node Name: %s", NodeName.c_str());
        ImGui::Text("Children: %d", Node->mNumChildren);
        ImGui::Text("Meshes Attached: %d", Node->mNumMeshes);

        // 可选：显示简单的 Transform 数据
        aiVector3D pos, scale;
        aiQuaternion rot;
        Node->mTransformation.Decompose(scale, rot, pos);
        ImGui::Text("Local Pos: (%.2f, %.2f, %.2f)", pos.x, pos.y, pos.z);
        ImGui::EndTooltip();
    }

    // 7. 递归绘制子节点
    if (bNodeOpen)
    {
        for (unsigned int i = 0; i < Node->mNumChildren; ++i)
        {
            DrawImGuiTreeRecursive(Node->mChildren[i]);
        }
        // 别忘了 Pop，否则层级会一直缩进直到崩溃
        ImGui::TreePop();
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