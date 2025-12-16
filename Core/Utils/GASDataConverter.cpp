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
    // 第一行 (UE: X轴基向量)
    OutMat.m[0][0] = InMat.a1; OutMat.m[0][1] = InMat.b1; OutMat.m[0][2] = InMat.c1; OutMat.m[0][3] = InMat.d1;

    // 第二行 (UE: Y轴基向量)
    OutMat.m[1][0] = InMat.a2; OutMat.m[1][1] = InMat.b2; OutMat.m[1][2] = InMat.c2; OutMat.m[1][3] = InMat.d2;

    // 第三行 (UE: Z轴基向量)
    OutMat.m[2][0] = InMat.a3; OutMat.m[2][1] = InMat.b3; OutMat.m[2][2] = InMat.c3; OutMat.m[2][3] = InMat.d3;

    // 第四行 (UE: 平移 Translation)
    OutMat.m[3][0] = InMat.a4; OutMat.m[3][1] = InMat.b4; OutMat.m[3][2] = InMat.c4; OutMat.m[3][3] = InMat.d4;
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
    // 提取平移 现在位于第 4 行

    OutTrans.x = InMat.m[3][0];
    OutTrans.y = InMat.m[3][1];
    OutTrans.z = InMat.m[3][2];

    //提取缩放 (Scale) -> 缩放是基向量的长度 在 UE 布局中，前三行 (Rows) 是基向量
    FGASVector3 Row0(InMat.m[0][0], InMat.m[0][1], InMat.m[0][2]);
    FGASVector3 Row1(InMat.m[1][0], InMat.m[1][1], InMat.m[1][2]);
    FGASVector3 Row2(InMat.m[2][0], InMat.m[2][1], InMat.m[2][2]);

    OutScale.x = std::sqrt(Row0.x * Row0.x + Row0.y * Row0.y + Row0.z * Row0.z);
    OutScale.y = std::sqrt(Row1.x * Row1.x + Row1.y * Row1.y + Row1.z * Row1.z);
    OutScale.z = std::sqrt(Row2.x * Row2.x + Row2.y * Row2.y + Row2.z * Row2.z);


    // 3. 处理负缩放 (镜像修正) 计算行列式来检测镜像

    float Determinant = Row0.x * (Row1.y * Row2.z - Row2.y * Row1.z) -
        Row0.y * (Row1.x * Row2.z - Row2.x * Row1.z) +
        Row0.z * (Row1.x * Row2.y - Row2.x * Row1.y);

    if (Determinant < 0)
    {
        OutScale.x = -OutScale.x;
        OutScale.y = -OutScale.y;
        OutScale.z = -OutScale.z;
    }

    // 4. 提取旋转 (Rotation)
   
    if (std::abs(OutScale.x) > 1e-6f) { Row0.x /= OutScale.x; Row0.y /= OutScale.x; Row0.z /= OutScale.x; }
    if (std::abs(OutScale.y) > 1e-6f) { Row1.x /= OutScale.y; Row1.y /= OutScale.y; Row1.z /= OutScale.y; }
    if (std::abs(OutScale.z) > 1e-6f) { Row2.x /= OutScale.z; Row2.y /= OutScale.z; Row2.z /= OutScale.z; }

    // 转四元数
    float Trace = Row0.x + Row1.y + Row2.z;
    if (Trace > 0.0f)
    {
        float S = 0.5f / std::sqrt(Trace + 1.0f);
        OutRot.w = 0.25f / S;
        OutRot.x = (Row1.z - Row2.y) * S;
        OutRot.y = (Row2.x - Row0.z) * S;
        OutRot.z = (Row0.y - Row1.x) * S;
    }
    else
    {
        if (Row0.x > Row1.y && Row0.x > Row2.z)
        {
            float S = 2.0f * std::sqrt(1.0f + Row0.x - Row1.y - Row2.z);
            OutRot.w = (Row1.z - Row2.y) / S;
            OutRot.x = 0.25f * S;
            OutRot.y = (Row0.y + Row1.x) / S;
            OutRot.z = (Row0.z + Row2.x) / S;
        }
        else if (Row1.y > Row2.z)
        {
            float S = 2.0f * std::sqrt(1.0f + Row1.y - Row0.x - Row2.z);
            OutRot.w = (Row2.x - Row0.z) / S;
            OutRot.x = (Row0.y + Row1.x) / S;
            OutRot.y = 0.25f * S;
            OutRot.z = (Row1.z + Row2.y) / S;
        }
        else
        {
            float S = 2.0f * std::sqrt(1.0f + Row2.z - Row0.x - Row1.y);
            OutRot.w = (Row0.y - Row1.x) / S;
            OutRot.x = (Row0.z + Row2.x) / S;
            OutRot.y = (Row1.z + Row2.y) / S;
            OutRot.z = 0.25f * S;
        }
    }

    // 归一化
    float LenSq = OutRot.x * OutRot.x + OutRot.y * OutRot.y + OutRot.z * OutRot.z + OutRot.w * OutRot.w;
    if (LenSq > 1e-6f && std::abs(LenSq - 1.0f) > 1e-6f)
    {
        float InvLen = 1.0f / std::sqrt(LenSq);
        OutRot.x *= InvLen; OutRot.y *= InvLen; OutRot.z *= InvLen; OutRot.w *= InvLen;
    }
}