#include "GASDataConverter.h"
#include <cmath>
#include <cctype>
#include <algorithm> 
#include <string>
#include <vector>

// 基础数据类型转换

FGASVector3 GASDataConverter::ToVector3(const aiVector3D& InVec)
{
    return FGASVector3{ InVec.x, InVec.y, InVec.z };
}

FGASQuaternion GASDataConverter::ToQuaternion(const aiQuaternion& InQuat)
{
    // Assimp: w, x, y, z
    //确保 FGASQuaternion 定义顺序也是 (x, y, z, w) 或者对应的构造函数参数顺序一致
    return FGASQuaternion{ InQuat.x, InQuat.y, InQuat.z, InQuat.w };
}

FGASMatrix4x4 GASDataConverter::ToMatrix4x4(const aiMatrix4x4& InMat)
{
    FGASMatrix4x4 OutMat;

    // 如果 FGASMatrix4x4 是行优先存储 (Row-Major)，可以直接一一赋值
    OutMat.M[0][0] = InMat.a1; OutMat.M[0][1] = InMat.a2; OutMat.M[0][2] = InMat.a3; OutMat.M[0][3] = InMat.a4;
    OutMat.M[1][0] = InMat.b1; OutMat.M[1][1] = InMat.b2; OutMat.M[1][2] = InMat.b3; OutMat.M[1][3] = InMat.b4;
    OutMat.M[2][0] = InMat.c1; OutMat.M[2][1] = InMat.c2; OutMat.M[2][2] = InMat.c3; OutMat.M[2][3] = InMat.c4;
    OutMat.M[3][0] = InMat.d1; OutMat.M[3][1] = InMat.d2; OutMat.M[3][2] = InMat.d3; OutMat.M[3][3] = InMat.d4;

    return OutMat;
}

// 坐标系与空间标准化 (核心功能)
FGASVector3 GASDataConverter::ConvertPositionToLeftHanded(const aiVector3D& InPos)
{
    return FGASVector3{ InPos.x, InPos.y, -InPos.z };
}

FGASQuaternion GASDataConverter::ConvertRotationToLeftHanded(const aiQuaternion& InQuat)
{
    // 对应位置 Z 轴的翻转
    // 旋转变换标准公式: (x, y, -z, -w) 等价于 (-x, -y, z, w)
    return FGASQuaternion{ -InQuat.x, -InQuat.y, InQuat.z, InQuat.w };
}
// 命名与字符串处理


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
    Result.erase(std::remove_if(Result.begin(), Result.end(), ::isspace), Result.end());

    // 3. 全部转小写 
    std::transform(Result.begin(), Result.end(), Result.begin(),
        [](unsigned char c) { return std::tolower(c); });

    return Result;
}

// 辅助计算
void GASDataConverter::DecomposeMatrix(const FGASMatrix4x4& InMat, FGASVector3& OutScale, FGASQuaternion& OutRot, FGASVector3& OutTrans)
{
    
}