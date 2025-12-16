#pragma once

#include <string>
#include <vector>
#include <algorithm>

#include "../Types/GASCoreTypes.h"
#include <assimp/vector3.h>
#include <assimp/quaternion.h>
#include <assimp/matrix4x4.h>

//负责将外部异构数据（主要是 Assimp 数据）转换为系统内部标准格式 (FGAS*)。

class GASDataConverter
{
public:
    // 基础数据类型转换

    //将 Assimp 向量转换为内部向量
    static FGASVector3 ToVector3(const aiVector3D& InVec);

    //将 Assimp 四元数转换为内部四元数
    static FGASQuaternion ToQuaternion(const aiQuaternion& InQuat);

    //将 Assimp 矩阵转换为内部 4x4 矩阵
    static FGASMatrix4x4 ToMatrix4x4(const aiMatrix4x4& InMat);

    // 坐标系与空间标准化 (核心功能)
    //将位置从 Assimp (通常是右手系 Y-Up) 转换到 目标引擎坐标系 (如左手系)
    static FGASVector3 ConvertPositionToLeftHanded(const aiVector3D& InPos);

    //将旋转从 Assimp (右手系) 转换到 目标引擎坐标系 (左手系)
    static FGASQuaternion ConvertRotationToLeftHanded(const aiQuaternion& InRot);

    // 命名与字符串处理
    //规范化骨骼名称:去除首尾空格-(可选) 转为小写-去除特殊符号，确保作为 Map Key 的安全性
    static std::string NormalizeBoneName(const std::string& InName);

    //从矩阵中提取 TRS (Translation, Rotation, Scale)
    static void DecomposeMatrix(const FGASMatrix4x4& InMat, FGASVector3& OutScale, FGASQuaternion& OutRot, FGASVector3& OutTrans);
}; 
