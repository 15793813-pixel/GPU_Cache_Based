#include "GASDataConverter.h"

// 【修复1】添加必要的标准库头文件
#include <cmath>
#include <cctype>
#include <algorithm> // 必须！修复 std::transform 和 std::remove_if 报错
#include <string>
#include <vector>

// =============================================================
// 基础数据类型转换
// =============================================================

FGASVector3 GASDataConverter::ToVector3(const aiVector3D& InVec)
{
    // 【修复2】使用 {} 初始化，避免 E0169 错误
    return FGASVector3{ InVec.x, InVec.y, InVec.z };
}

FGASQuaternion GASDataConverter::ToQuaternion(const aiQuaternion& InQuat)
{
    // Assimp: w, x, y, z
    // 注意：请确保你的 FGASQuaternion 定义顺序也是 (x, y, z, w) 或者对应的构造函数参数顺序一致
    return FGASQuaternion{ InQuat.x, InQuat.y, InQuat.z, InQuat.w };
}

FGASMatrix4x4 GASDataConverter::ToMatrix4x4(const aiMatrix4x4& InMat)
{
    FGASMatrix4x4 OutMat;

    // 【修复3】使用大写 M (对应错误 E0135)
    // 如果你的 FGASMatrix4x4 是行优先存储 (Row-Major)，可以直接一一赋值
    OutMat.M[0][0] = InMat.a1; OutMat.M[0][1] = InMat.a2; OutMat.M[0][2] = InMat.a3; OutMat.M[0][3] = InMat.a4;
    OutMat.M[1][0] = InMat.b1; OutMat.M[1][1] = InMat.b2; OutMat.M[1][2] = InMat.b3; OutMat.M[1][3] = InMat.b4;
    OutMat.M[2][0] = InMat.c1; OutMat.M[2][1] = InMat.c2; OutMat.M[2][2] = InMat.c3; OutMat.M[2][3] = InMat.c4;
    OutMat.M[3][0] = InMat.d1; OutMat.M[3][1] = InMat.d2; OutMat.M[3][2] = InMat.d3; OutMat.M[3][3] = InMat.d4;

    // 【备选方案】如果不想管成员变量叫 m 还是 M，且内存布局一致（都是 4x4 float），可以直接内存拷贝：
    // memcpy(&OutMat, &InMat, sizeof(float) * 16);

    return OutMat;
}

// =============================================================
// 坐标系与空间标准化 (核心功能)
// =============================================================

FGASVector3 GASDataConverter::ConvertPositionToLeftHanded(const aiVector3D& InPos)
{
    // 方案：翻转 Z 轴 (适配常见的 RH -> LH 转换)
    return FGASVector3{ InPos.x, InPos.y, -InPos.z };
}

FGASQuaternion GASDataConverter::ConvertRotationToLeftHanded(const aiQuaternion& InQuat)
{
    // 对应位置 Z 轴的翻转
    // 旋转变换标准公式: (x, y, -z, -w) 等价于 (-x, -y, z, w)
    // 使用 {} 初始化
    return FGASQuaternion{ -InQuat.x, -InQuat.y, InQuat.z, InQuat.w };
}

// =============================================================
// 命名与字符串处理
// =============================================================

std::string GASDataConverter::NormalizeBoneName(const std::string& InName)
{
    std::string Result = InName;

    // 1. 移除 'mixamorig:' 等常见前缀
    size_t ColonPos = Result.find(':');
    if (ColonPos != std::string::npos)
    {
        Result = Result.substr(ColonPos + 1);
    }

    // 2. 移除所有空格
    // std::remove_if 需要 <algorithm>
    Result.erase(std::remove_if(Result.begin(), Result.end(), ::isspace), Result.end());

    // 3. 全部转小写 (为了 Map 查找的一致性)
    // 【修复4】使用 Lambda 表达式包装 std::tolower，解决重载函数匹配失败 (E0493/E0260)
    std::transform(Result.begin(), Result.end(), Result.begin(),
        [](unsigned char c) { return std::tolower(c); });

    return Result;
}

// =============================================================
// 辅助计算
// =============================================================

void GASDataConverter::DecomposeMatrix(const FGASMatrix4x4& InMat, FGASVector3& OutScale, FGASQuaternion& OutRot, FGASVector3& OutTrans)
{
    // 这是一个简化版的分解占位符
    // 如果需要实现，通常需要将 FGASMatrix4x4 转回 aiMatrix4x4 并调用 Assimp 的 Decompose
    // 或者引入 math 库 (如 glm / Unreal Math)

    /* 示例实现逻辑：
    aiMatrix4x4 Mat;
    // ... 将 InMat 拷贝给 Mat ...

    aiVector3D Scaling, Position;
    aiQuaternion Rotation;
    Mat.Decompose(Scaling, Rotation, Position);

    OutScale = ToVector3(Scaling);
    OutRot   = ToQuaternion(Rotation);
    OutTrans = ToVector3(Position);
    */
}