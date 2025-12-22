#include "GASUI.h"
#include "../Core/Types/GASHeader.h"         

#include <GLFW/glfw3.h>
#include "imgui-master/imgui.h"
#include "imgui-master/backends/imgui_impl_glfw.h"
#include "imgui-master/backends/imgui_impl_opengl3.h"
#pragma comment(lib, "opengl32.lib")

namespace fs = std::filesystem;


// --- 状态变量 ---
GLFWwindow* GASUI::m_Window = nullptr;
Assimp::Importer GASUI::m_PreviewImporter;
const aiScene* GASUI::m_PreviewScene = nullptr;

// --- 摄像机 ---
float GASUI::m_CameraDistance = -400.0f;
float GASUI::m_CameraYaw = 0.0f;
float GASUI::m_CameraPitch = 20.0f;

// --- 路径 ---
const std::string GASUI::BACKUP_SOURCE_DIR = "H:\\GPU_Cache_Based\\MyAnimationSystem\\Assets\\Sources";
std::string GASUI::m_SourceFilePath = "Drag .your file here...";
std::string GASUI::m_ExistingAssetPath = "";

// --- 文字与弹窗 ---
std::string GASUI::m_StatusMessage = "Ready";
bool GASUI::m_ShowDuplicatePopup = false;

// --- 文件相关 ---
std::vector<FGASAssetMetadata> GASUI::m_ImportedResults;
uint64_t GASUI::m_CurrentSourceHash = 0;
std::map<std::string, bool> GASUI::m_BoneMap;
std::vector<FGASAssetMetadata>  GASUI::m_DatabaseAssetList;

// --- 颜色常量 ---
const FGASVector3 COLOR_BONE(0.0f, 1.0f, 0.0f);    // 绿
const FGASVector3 COLOR_NODE(0.5f, 0.5f, 0.5f);    // 灰
const FGASVector3 COLOR_ROOT(1.0f, 0.0f, 0.0f);    // 红
const FGASVector3 COLOR_VIRTUAL(1.0f, 1.0f, 0.0f); // 黄

// --- 初始化 ---
bool GASUI::Initialize()
{
    if (!glfwInit()) return false;
    m_Window = glfwCreateWindow(1280, 720, "GAS Asset Editor", NULL, NULL);
    if (!m_Window) {
        glfwTerminate();
        return false;
    }
    glfwMakeContextCurrent(m_Window);
    glfwSwapInterval(1); 

    // 注册拖拽回调
    glfwSetDropCallback(m_Window, OnFileDrop);

    // 初始化 ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // 允许键盘控制
    ImGui::StyleColorsDark();

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
        m_ImportedResults.clear(); 
        m_ShowDuplicatePopup = false;

        //计算并保存hash
        std::vector<uint8_t> FileData = GASFileHelper::ReadRawFile(m_SourceFilePath);
        m_CurrentSourceHash = CalculateXXHash64(FileData.data(), FileData.size()); 

        uint64_t ExpectedGUID = GenerateGUID64(std::filesystem::path(m_SourceFilePath).filename().string());

        FGASAssetMetadata ExistingMeta;
        bool bIsDuplicate = false;

        // 查询 AssetManager
        if (GASAssetManager::Get().GetGASMetadataStorage().QueryAssetByGUID(ExpectedGUID, ExistingMeta))
        {
            
            if (ExistingMeta.FileHash == m_CurrentSourceHash)
            {
                bIsDuplicate = true;
                m_ExistingAssetPath = ExistingMeta.BinaryFilePath; 
            }
        }
        //如果已有
        if (bIsDuplicate)
        {
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

//画左侧面板
void GASUI::RenderImporterPanel()
{
    // 设置面板位置和大小 
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(350, ImGui::GetIO().DisplaySize.y));
    ImGui::Begin("Importer Panel", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);

    //  顶部资产库下拉菜单  展开状态
    bool bHeaderOpen = ImGui::TreeNodeEx("Asset Library (Database)", ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_SpanAvailWidth);

    if (bHeaderOpen)
    {
        // 刷新
        if (ImGui::Button("Refresh DB", ImVec2(-1, 0)))
        {
            m_DatabaseAssetList = GASAssetManager::Get().GetGASMetadataStorage().QueryAllAssets();
        }
        ImGui::BeginChild("LibList", ImVec2(0, 150), true);
        for (const auto& Asset : m_DatabaseAssetList)
        {
            if (Asset.Type != EGASAssetType::Skeleton)
                continue;

            bool isSelected = (m_CurrentSourceHash == Asset.FileHash);

            std::string Label = "[Asset] " + Asset.Name;
            //加载
            if (ImGui::Selectable(Label.c_str(), isSelected))
            {
                LoadExistingAsset(Asset.Name, Asset.FileHash);
            }
        }

        ImGui::EndChild();
        ImGui::TreePop();
    }

    ImGui::Separator();


    // ---  源文件信息 ---
    ImGui::TextColored(ImVec4(1, 1, 0, 1), "Source File:");
    ImGui::TextWrapped("%s", m_SourceFilePath.c_str());
    ImGui::Separator();
    ImGui::Text("Status: %s", m_StatusMessage.c_str());
    ImGui::Separator();

    // ---  操作按钮 ---
    if (m_PreviewScene)
    {
        ImGui::BeginDisabled(m_ShowDuplicatePopup);
        if (ImGui::Button("CONVERT TO .GAS", ImVec2(-1, 30)))
        {
            GAS_LOG("UI: Starting Import...");

            // 执行导入
            uint64_t MainGUID = GASAssetManager::Get().ImportAsset(m_SourceFilePath);

            if (MainGUID != 0)
            {
                //拷贝备份 
                try {
                    fs::path Src(m_SourceFilePath);
                    fs::path DestDir(BACKUP_SOURCE_DIR);
                    if (!fs::exists(DestDir)) fs::create_directories(DestDir);
                    fs::copy_file(Src, DestDir / Src.filename(), fs::copy_options::overwrite_existing);
                }
                catch (...) {}

                // 查询生成结果 
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

    // --- 输出结果列表 ---

    if (!m_ImportedResults.empty())
    {
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::TextColored(ImVec4(0, 1, 0, 1), "Output Assets (%d):", (int)m_ImportedResults.size());

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

            ImGui::TextColored(TypeColor, "%s", TypeLabel.c_str());
            ImGui::SameLine();
            ImGui::Text("%s", FileName.c_str());

            // 显示详细数据 
            ImGui::Indent(15.0f);
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.7f, 0.7f, 1.0f)); // 灰色

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
            // 动画信息 
            else if (Asset.Type == EGASAssetType::Animation)
            {
                ImGui::BulletText("Frames:     %d", Asset.FrameCount);
                ImGui::BulletText("Duration:   %.3f s", Asset.Duration);
                float FPS = (Asset.Duration > 0.0001f) ? ((float)Asset.FrameCount / Asset.Duration) : 0.0f;
                ImGui::BulletText("FrameRate:  %.1f fps", FPS);
            }

            ImGui::PopStyleColor(); // 恢复颜色
            ImGui::Unindent(15.0f);

            ImGui::Separator(); 
        }
        ImGui::EndChild();
    }
    if (ImGui::BeginPopup("SuccessPopup")) {
        ImGui::Text("Import Completed!");
        if (ImGui::Button("Close")) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    // 重复文件弹窗
    if (m_ShowDuplicatePopup)
    {
        ImGui::OpenPopup("Duplicate File Detected");
        m_ShowDuplicatePopup = false;
    }

    if (ImGui::BeginPopupModal("Duplicate File Detected", NULL, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::Text("This file already exists in the library!");
        ImGui::Separator();

        ImGui::TextColored(ImVec4(1, 0, 0, 1), "Existing Path:");
        // 限制一下宽度
        ImGui::PushTextWrapPos(300.0f);
        ImGui::Text("%s", m_ExistingAssetPath.c_str());
        ImGui::PopTextWrapPos();

        ImGui::Separator();

        if (ImGui::Button("Cancel Import", ImVec2(120, 0)))
        {
            m_StatusMessage = "Import Cancelled";
            ImGui::CloseCurrentPopup(); 
        }

        ImGui::SameLine();
        if (ImGui::Button("Overwrite", ImVec2(120, 0)))
        {
            m_StatusMessage = "Duplicate Warning Ignored";
            ImGui::CloseCurrentPopup(); 
        }

        ImGui::EndPopup();
    }

    ImGui::End();
}

// 画右侧场景层级面板
void GASUI::RenderHierarchyPanel()
{
    // 如果没有预览场景，不绘制
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
        
        if (ImGui::CollapsingHeader("Stats", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::Text("Meshes: %d", m_PreviewScene->mNumMeshes);
            ImGui::Text("Anims:  %d", m_PreviewScene->mNumAnimations);
        }
        ImGui::Separator();

        ImGui::BeginChild("TreeScroll", ImVec2(0, -1), false, ImGuiWindowFlags_HorizontalScrollbar);
        DrawImGuiTreeRecursive(m_PreviewScene->mRootNode);
        ImGui::EndChild();
    }

    ImGui::End();
}

//画3D场景
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

        //如果有长度，画线
        if (DistSq > 0.0001f) {
            DrawLine(P1, P2, Color);
        }
        // 如果是纯旋转节点，画一个黄色大点
        else {
            glPointSize(8.0f);
            glBegin(GL_POINTS);
            glColor3f(COLOR_VIRTUAL.x, COLOR_VIRTUAL.y, COLOR_VIRTUAL.z);
            glVertex3f(P2.x, P2.y, P2.z);
            glEnd();
            glPointSize(1.0f);
        }
    }
    for (unsigned int i = 0; i < Node->mNumChildren; ++i)
    {
        DrawAssimpNodeRecursive(Node->mChildren[i], CurrentGlobal, BoneMap);
    }
}

//画骨骼树
void GASUI::DrawImGuiTreeRecursive(const aiNode* Node)
{
    if (!Node) return;

    std::string NodeName = Node->mName.C_Str();
    if (NodeName.empty()) NodeName = "Unnamed_Node";

    std::string CleanName = GASDataConverter::NormalizeBoneName(NodeName);
    bool bColorPushed = false;

    if (m_BoneMap.count(CleanName))
    {
        // 有权重的真骨骼 -> 亮绿色
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.2f, 1.0f, 0.2f, 1.0f));
        bColorPushed = true;
    }
    else if (NodeName.find("_$AssimpFbx$_") != std::string::npos)
    {
        // Assimp 生成的虚拟节点 -> 黄色
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 0.0f, 1.0f));
        bColorPushed = true;
    }

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

    // 鼠标悬停显示详细信息
    if (ImGui::IsItemHovered())
    {
        ImGui::BeginTooltip();
        ImGui::Text("Node Name: %s", NodeName.c_str());
        ImGui::Text("Children: %d", Node->mNumChildren);
        ImGui::Text("Meshes Attached: %d", Node->mNumMeshes);

        aiVector3D pos, scale;
        aiQuaternion rot;
        Node->mTransformation.Decompose(scale, rot, pos);
        ImGui::Text("Local Pos: (%.2f, %.2f, %.2f)", pos.x, pos.y, pos.z);
        ImGui::EndTooltip();
    }

    if (bNodeOpen)
    {
        for (unsigned int i = 0; i < Node->mNumChildren; ++i)
        {
            DrawImGuiTreeRecursive(Node->mChildren[i]);
        }
        ImGui::TreePop();
    }
}


//绘制主循环
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
        RenderImporterPanel();  
        RenderHierarchyPanel(); 

        ImGui::Render();

        // --- 渲染 3D ---
        int display_w, display_h;
        glfwGetFramebufferSize(m_Window, &display_w, &display_h);

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
        if (m_PreviewScene && m_PreviewScene->mRootNode)
        {
            aiMatrix4x4 Identity; // 单位矩阵
            DrawAssimpNodeRecursive(m_PreviewScene->mRootNode, Identity, m_BoneMap);
        }

        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(m_Window);
    }
}

// 画线
void DrawLine(const FGASVector3& Start, const FGASVector3& End, const FGASVector3& Color)
{
    glLineWidth(2.0f);
    glBegin(GL_LINES);
    glColor3f(Color.x, Color.y, Color.z);
    glVertex3f(Start.x, Start.y, Start.z);
    glVertex3f(End.x, End.y, End.z);
    glEnd();
}

//画网格
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

// 在备份目录查找源文件
std::string GASUI::FindBackupFile(const std::string& AssetName)
{
    fs::path BackupDir(BACKUP_SOURCE_DIR);
    if (!fs::exists(BackupDir)) return "";

    try {
        for (const auto& entry : fs::directory_iterator(BackupDir))
        {
            if (entry.is_regular_file())
            {
                std::string StemName = entry.path().stem().string();
                if (StemName.find(AssetName) != std::string::npos)
                {
                    return entry.path().string();
                }
            }
        }
    }
    catch (const std::exception& e)
    {
        GAS_LOG_ERROR("UI: Error finding backup file: %s", e.what());
    }

    return "";
}

// 加载现有资产到编辑器
void GASUI::LoadExistingAsset(const std::string& AssetName, uint64_t AssetHash)
{
    GAS_LOG("UI: Loading asset from library: %s", AssetName.c_str());

    m_CurrentSourceHash = AssetHash;

    m_ImportedResults.clear();
    GASAssetManager::Get().GetGASMetadataStorage().QueryAssetsByFileHash(m_CurrentSourceHash, m_ImportedResults);

    //试找回原始 FBX 文件进行 3D 预览
    std::string BackupPath = FindBackupFile(AssetName);

    // 加载 Assimp 场景
    if (!BackupPath.empty() && fs::exists(BackupPath))
    {
       
        m_PreviewImporter.FreeScene();
        m_PreviewScene = nullptr;
        m_BoneMap.clear();

        const unsigned int flags = aiProcess_Triangulate | aiProcess_SortByPType;
        m_PreviewScene = m_PreviewImporter.ReadFile(BackupPath, flags);


        if (m_PreviewScene)
        {
            m_SourceFilePath = BackupPath; 
            m_StatusMessage = "Loaded from Database"; 

            if (m_PreviewScene->HasMeshes())
            {
                for (unsigned int i = 0; i < m_PreviewScene->mNumMeshes; ++i)
                {
                    const aiMesh* mesh = m_PreviewScene->mMeshes[i];

                    for (unsigned int b = 0; b < mesh->mNumBones; ++b)
                    {
                        
                        std::string RawBoneName = mesh->mBones[b]->mName.C_Str();
                        std::string CleanName = GASDataConverter::NormalizeBoneName(RawBoneName);

                        m_BoneMap[CleanName] = true;
                    }
                }
            }
        }
        else
        {
            // Assimp 读取失败 
            m_StatusMessage = "Error: Failed to load FBX";
            GAS_LOG_ERROR("UI: Assimp failed to load %s: %s", BackupPath.c_str(), m_PreviewImporter.GetErrorString());
        }
    }
    else
    {
        //如果找不到备份文件
        m_StatusMessage = "Source file missing (Data Only)";
        m_SourceFilePath = "Original file not found in Backup folder";

        // 确保清空预览，防止显示上一个模型的残留
        m_PreviewImporter.FreeScene();
        m_PreviewScene = nullptr;
        m_BoneMap.clear();
    }
}