#include "GASDataConverter.h"
#include <cmath>
#include <cctype>

// =============================================================
// 基础数据类型转换
// =============================================================

FGASVector3 GASDataConverter::ToVector3(const aiVector3D& InVec)
{
    return FGASVector3(InVec.x, InVec.y, InVec.z);
}

FGASQuaternion GASDataConverter::ToQuaternion(const aiQuaternion& InQuat)
{
    // Assimp: w, x, y, z
    // FGAS: 假设你也遵循这个顺序，或者 x, y, z, w。请根据 GASCoreTypes 调整构造函数参数顺序
    return FGASQuaternion(InQuat.x, InQuat.y, InQuat.z, InQuat.w);
}

FGASMatrix4x4 GASDataConverter::ToMatrix4x4(const aiMatrix4x4& InMat)
{
    FGASMatrix4x4 OutMat;

    // Assimp 是 Row-Major (a1, a2, a3, a4 是第一行)
    // 直接内存拷贝通常是最快的方法，但为了清晰和防止填充问题，我们逐个赋值
    // 如果你的 FGASMatrix4x4 也是 Row-Major：
    OutMat.m[0][0] = InMat.a1; OutMat.m[0][1] = InMat.a2; OutMat.m[0][2] = InMat.a3; OutMat.m[0][3] = InMat.a4;
    OutMat.m[1][0] = InMat.b1; OutMat.m[1][1] = InMat.b2; OutMat.m[1][2] = InMat.b3; OutMat.m[1][3] = InMat.b4;
    OutMat.m[2][0] = InMat.c1; OutMat.m[2][1] = InMat.c2; OutMat.m[2][2] = InMat.c3; OutMat.m[2][3] = InMat.c4;
    OutMat.m[3][0] = InMat.d1; OutMat.m[3][1] = InMat.d2; OutMat.m[3][2] = InMat.d3; OutMat.m[3][3] = InMat.d4;

    return OutMat;
}

// =============================================================
// 坐标系与空间标准化 (核心功能)
// =============================================================

FGASVector3 GASDataConverter::ConvertPositionToLeftHanded(const aiVector3D& InPos)
{
    // 方案：翻转 Z 轴 (适配常见的 RH -> LH 转换)
    return FGASVector3(InPos.x, InPos.y, -InPos.z);
}

FGASQuaternion GASDataConverter::ConvertRotationToLeftHanded(const aiQuaternion& InQuat)
{
    // 对应位置 Z 轴的翻转，四元数需要翻转 x 和 y 分量，或者 z 和 w 分量。
    // 这里采用翻转 Z 轴的旋转变换标准公式: (x, y, -z, -w) 等价于 (-x, -y, z, w)
    return FGASQuaternion(-InQuat.x, -InQuat.y, InQuat.z, InQuat.w);
}

// =============================================================
// 命名与字符串处理
// =============================================================

std::string GASDataConverter::NormalizeBoneName(const std::string& InName)
{
    std::string Result = InName;

    // 1. 移除 'mixamorig:' 等常见前缀 (根据项目需求定制)
    size_t ColonPos = Result.find(':');
    if (ColonPos != std::string::npos)
    {
        Result = Result.substr(ColonPos + 1);
    }

    // 2. 移除所有空格
    Result.erase(std::remove_if(Result.begin(), Result.end(), ::isspace), Result.end());

    // 3. 全部转小写 (为了 Map 查找的一致性)
    std::transform(Result.begin(), Result.end(), Result.begin(),
        [](unsigned char c) { return std::tolower(c); });

    return Result;
}

// =============================================================
// 辅助计算
// =============================================================

void GASDataConverter::DecomposeMatrix(const FGASMatrix4x4& InMat, FGASVector3& OutScale, FGASQuaternion& OutRot, FGASVector3& OutTrans)
{
    // 这是一个简化版的分解，如果使用 Assimp，可以直接调用 InMat.Decompose(...)
    // 但既然要在Converter里做解耦，这里可以用 Assimp 的逻辑或者手写
    // 这里演示调用 Assimp 的逻辑 (假设 InMat 已经转回 Assimp 格式或者我们直接操作数据)

    // 注意：这里为了严谨，通常建议直接在 Importer 阶段利用 Assimp 的 Decompose
    // 但如果必须处理 FGASMatrix，你需要实现类似 glm::decompose 或 aiMatrix4x4::Decompose 的数学逻辑

    // 伪代码占位：
    // OutTrans = InMat.GetTranslation();
    // OutScale = InMat.GetScale();
    // OutRot = InMat.GetRotation();
}