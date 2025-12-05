#pragma once

#include <string>
#include <vector>
#include <algorithm>

// 引入你的核心类型定义
#include "../Types/GASCoreTypes.h"

// 引入 Assimp 数学库 (确保你的项目包含了 Assimp include 路径)
#include <assimp/vector3.h>
#include <assimp/quaternion.h>
#include <assimp/matrix4x4.h>

/**
 * @class GASDataConverter
 * @brief 负责将外部异构数据（主要是 Assimp 数据）转换为系统内部标准格式 (FGAS*)。
 * 处理坐标系变换、单位换算、命名规范化等脏活。
 */
class GASDataConverter
{
public:
    // =============================================================
    // 基础数据类型转换
    // =============================================================

    /** 将 Assimp 向量转换为内部向量 */
    static FGASVector3 ToVector3(const aiVector3D& InVec);

    /** 将 Assimp 四元数转换为内部四元数 */
    static FGASQuaternion ToQuaternion(const aiQuaternion& InQuat);

    /** * 将 Assimp 矩阵转换为内部 4x4 矩阵
     * 注意：这里会处理 Row-Major (行优先) 到 Column-Major (列优先) 的转换（如果需要）
     */
    static FGASMatrix4x4 ToMatrix4x4(const aiMatrix4x4& InMat);

    // =============================================================
    // 坐标系与空间标准化 (核心功能)
    // =============================================================

    /**
     * 将位置从 Assimp (通常是右手系 Y-Up) 转换到 目标引擎坐标系 (如左手系)
     * 常见操作：翻转 Z 轴 (z = -z)
     */
    static FGASVector3 ConvertPositionToLeftHanded(const aiVector3D& InPos);

    /**
     * 将旋转从 Assimp (右手系) 转换到 目标引擎坐标系 (左手系)
     * 四元数转换通常需要修改 x,y,z,w 的符号
     */
    static FGASQuaternion ConvertRotationToLeftHanded(const aiQuaternion& InRot);

    // =============================================================
    // 命名与字符串处理
    // =============================================================

    /**
     * 规范化骨骼名称
     * 1. 去除首尾空格
     * 2. (可选) 转为小写
     * 3. 去除特殊符号，确保作为 Map Key 的安全性
     */
    static std::string NormalizeBoneName(const std::string& InName);

    // =============================================================
    // 辅助计算
    // =============================================================

    /**
     * 从矩阵中提取 TRS (Translation, Rotation, Scale)
     * 用于数据压缩前的数据分离
     */
    static void DecomposeMatrix(const FGASMatrix4x4& InMat, FGASVector3& OutScale, FGASQuaternion& OutRot, FGASVector3& OutTrans);
}; #pragma once
