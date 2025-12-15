#pragma once

#include "../Types/GASCoreTypes.h"
#include <cmath>
#include <algorithm>
#include <limits>

namespace GASMath
{
    static const float PI = 3.1415926535897932f;
    static const float SMALL_NUMBER = 1.e-8f;

    // 1. 向量运算 (Vector3 Operations)

    inline FGASVector3 Add(const FGASVector3& A, const FGASVector3& B) { return { A.X + B.X, A.Y + B.Y, A.Z + B.Z }; }
    inline FGASVector3 Subtract(const FGASVector3& A, const FGASVector3& B) { return { A.X - B.X, A.Y - B.Y, A.Z - B.Z }; }
    inline FGASVector3 Scale(const FGASVector3& V, float S) { return { V.X * S, V.Y * S, V.Z * S }; }

    inline float Dot(const FGASVector3& A, const FGASVector3& B) { return A.X * B.X + A.Y * B.Y + A.Z * B.Z; }

    inline FGASVector3 Cross(const FGASVector3& A, const FGASVector3& B)
    {
        return {
            A.Y * B.Z - A.Z * B.Y,
            A.Z * B.X - A.X * B.Z,
            A.X * B.Y - A.Y * B.X
        };
    }

    inline float LengthSq(const FGASVector3& V) { return Dot(V, V); }
    inline float Length(const FGASVector3& V) { return std::sqrt(LengthSq(V)); }

    inline FGASVector3 Normalize(const FGASVector3& V)
    {
        float LenSq = LengthSq(V);
        if (LenSq < SMALL_NUMBER) return { 0, 0, 0 };
        float InvLen = 1.0f / std::sqrt(LenSq);
        return Scale(V, InvLen);
    }

    // 2. 四元数运算 (Quaternion Operations)

    inline FGASQuaternion IdentityQuat() { return { 0.f, 0.f, 0.f, 1.f }; }

    // 四元数乘法 (组合旋转: Result = A * B)
    inline FGASQuaternion Multiply(const FGASQuaternion& A, const FGASQuaternion& B)
    {
        FGASQuaternion R;
        R.X = A.W * B.X + A.X * B.W + A.Y * B.Z - A.Z * B.Y;
        R.Y = A.W * B.Y - A.X * B.Z + A.Y * B.W + A.Z * B.X;
        R.Z = A.W * B.Z + A.X * B.Y - A.Y * B.X + A.Z * B.W;
        R.W = A.W * B.W - A.X * B.X - A.Y * B.Y - A.Z * B.Z;
        return R;
    }

    inline FGASQuaternion Normalize(const FGASQuaternion& Q)
    {
        float LenSq = Q.X * Q.X + Q.Y * Q.Y + Q.Z * Q.Z + Q.W * Q.W;
        if (LenSq < SMALL_NUMBER) return IdentityQuat();
        float InvLen = 1.0f / std::sqrt(LenSq);
        return { Q.X * InvLen, Q.Y * InvLen, Q.Z * InvLen, Q.W * InvLen };
    }

    /**
     * 四元数球面插值 (Slerp)
     * 骨骼动画核心算法：用于在两个关键帧之间平滑过渡
     */
    inline FGASQuaternion Slerp(const FGASQuaternion& A, const FGASQuaternion& B, float Alpha)
    {
        // 计算夹角余弦
        float CosTheta = A.X * B.X + A.Y * B.Y + A.Z * B.Z + A.W * B.W;

        FGASQuaternion Target = B;

        if (CosTheta < 0.0f)
        {
            Target.X = -B.X; Target.Y = -B.Y; Target.Z = -B.Z; Target.W = -B.W;
            CosTheta = -CosTheta;
        }

        float ScaleA, ScaleB;
        if (CosTheta > 0.9995f)
        {
            ScaleA = 1.0f - Alpha;
            ScaleB = Alpha;
        }
        else
        {
            float Theta = std::acos(CosTheta);
            float SinTheta = std::sin(Theta);

            ScaleA = std::sin((1.0f - Alpha) * Theta) / SinTheta;
            ScaleB = std::sin(Alpha * Theta) / SinTheta;
        }

        FGASQuaternion Result;
        Result.X = A.X * ScaleA + Target.X * ScaleB;
        Result.Y = A.Y * ScaleA + Target.Y * ScaleB;
        Result.Z = A.Z * ScaleA + Target.Z * ScaleB;
        Result.W = A.W * ScaleA + Target.W * ScaleB;

        return Normalize(Result);
    }


    // 假设：列主序 (Column-Major)，符合 OpenGL/主流图形学标准
    // =========================================================

    inline FGASMatrix4x4 IdentityMatrix() { return FGASMatrix4x4(); }

    // 矩阵乘法
    inline FGASMatrix4x4 Multiply(const FGASMatrix4x4& A, const FGASMatrix4x4& B)
    {
        FGASMatrix4x4 R;
        for (int i = 0; i < 4; i++) for (int j = 0; j < 4; j++) R.M[i][j] = 0.f;

        for (int r = 0; r < 4; ++r) {
            for (int c = 0; c < 4; ++c) {
                // R[r][c] = Row(A, r) dot Col(B, c)
                R.M[r][c] = A.M[r][0] * B.M[0][c] +
                    A.M[r][1] * B.M[1][c] +
                    A.M[r][2] * B.M[2][c] +
                    A.M[r][3] * B.M[3][c];
            }
        }
        return R;
    }

    // 四元数转旋转矩阵
    inline FGASMatrix4x4 ToMatrix(const FGASQuaternion& Q)
    {
        FGASMatrix4x4 R;
        float x2 = Q.X + Q.X, y2 = Q.Y + Q.Y, z2 = Q.Z + Q.Z;
        float xx = Q.X * x2, xy = Q.X * y2, xz = Q.X * z2;
        float yy = Q.Y * y2, yz = Q.Y * z2, zz = Q.Z * z2;
        float wx = Q.W * x2, wy = Q.W * y2, wz = Q.W * z2;

        R.M[0][0] = 1.0f - (yy + zz); R.M[0][1] = xy - wz;        R.M[0][2] = xz + wy;        R.M[0][3] = 0.0f;
        R.M[1][0] = xy + wz;        R.M[1][1] = 1.0f - (xx + zz); R.M[1][2] = yz - wx;        R.M[1][3] = 0.0f;
        R.M[2][0] = xz - wy;        R.M[2][1] = yz + wx;        R.M[2][2] = 1.0f - (xx + yy); R.M[2][3] = 0.0f;
        R.M[3][0] = 0.0f;           R.M[3][1] = 0.0f;           R.M[3][2] = 0.0f;           R.M[3][3] = 1.0f;
        return R;
    }

    /**
     * 将 TRS 合成为 4x4 变换矩阵
     * 顺序：Scale -> Rotation -> Translation
     */
    inline FGASMatrix4x4 ComposeTransform(const FGASVector3& T, const FGASQuaternion& R, const FGASVector3& S)
    {
        // 1. 旋转
        FGASMatrix4x4 Mat = ToMatrix(R);

        // 2. 缩放 (直接乘在基向量上, Col 0, 1, 2)
        Mat.M[0][0] *= S.X; Mat.M[1][0] *= S.X; Mat.M[2][0] *= S.X;
        Mat.M[0][1] *= S.Y; Mat.M[1][1] *= S.Y; Mat.M[2][1] *= S.Y;
        Mat.M[0][2] *= S.Z; Mat.M[1][2] *= S.Z; Mat.M[2][2] *= S.Z;

        // 3. 位移 (Column 3)
        Mat.M[0][3] = T.X;
        Mat.M[1][3] = T.Y;
        Mat.M[2][3] = T.Z;

        // 确保最后一行标准
        Mat.M[3][0] = 0.f; Mat.M[3][1] = 0.f; Mat.M[3][2] = 0.f; Mat.M[3][3] = 1.f;

        return Mat;
    }

    // Helper: 结构体直接转矩阵
    inline FGASMatrix4x4 ToMatrix(const FGASTransform& T)
    {
        return ComposeTransform(T.Translation, T.Rotation, T.Scale);
    }


     // 4x4 矩阵求逆 (使用代数余子式法)

    inline FGASMatrix4x4 Inverse(const FGASMatrix4x4& InM)
    {
        float m[16];
        int k = 0;
        // 展平
        for (int i = 0; i < 4; i++) for (int j = 0; j < 4; j++) m[k++] = InM.M[i][j];

        float inv[16], det;

        // 这里省略推导过程，直接展开代数余子式计算
        inv[0] = m[5] * m[10] * m[15] - m[5] * m[11] * m[14] - m[9] * m[6] * m[15] + m[9] * m[7] * m[14] + m[13] * m[6] * m[11] - m[13] * m[7] * m[10];
        inv[4] = -m[4] * m[10] * m[15] + m[4] * m[11] * m[14] + m[8] * m[6] * m[15] - m[8] * m[7] * m[14] - m[12] * m[6] * m[11] + m[12] * m[7] * m[10];
        inv[8] = m[4] * m[9] * m[15] - m[4] * m[11] * m[13] - m[8] * m[5] * m[15] + m[8] * m[7] * m[13] + m[12] * m[5] * m[11] - m[12] * m[7] * m[9];
        inv[12] = -m[4] * m[9] * m[14] + m[4] * m[10] * m[13] + m[8] * m[5] * m[14] - m[8] * m[6] * m[13] - m[12] * m[5] * m[10] + m[12] * m[6] * m[9];
        inv[1] = -m[1] * m[10] * m[15] + m[1] * m[11] * m[14] + m[9] * m[2] * m[15] - m[9] * m[3] * m[14] - m[13] * m[2] * m[11] + m[13] * m[3] * m[10];
        inv[5] = m[0] * m[10] * m[15] - m[0] * m[11] * m[14] - m[8] * m[2] * m[15] + m[8] * m[3] * m[14] + m[12] * m[2] * m[11] - m[12] * m[3] * m[10];
        inv[9] = -m[0] * m[9] * m[15] + m[0] * m[11] * m[13] + m[8] * m[1] * m[15] - m[8] * m[3] * m[13] - m[12] * m[1] * m[11] + m[12] * m[3] * m[9];
        inv[13] = m[0] * m[9] * m[14] - m[0] * m[10] * m[13] - m[8] * m[1] * m[14] + m[8] * m[2] * m[13] + m[12] * m[1] * m[10] - m[12] * m[2] * m[9];
        inv[2] = m[1] * m[6] * m[15] - m[1] * m[7] * m[14] - m[5] * m[2] * m[15] + m[5] * m[3] * m[14] + m[13] * m[2] * m[7] - m[13] * m[3] * m[6];
        inv[6] = -m[0] * m[6] * m[15] + m[0] * m[7] * m[14] + m[4] * m[2] * m[15] - m[4] * m[3] * m[14] - m[12] * m[2] * m[7] + m[12] * m[3] * m[6];
        inv[10] = m[0] * m[5] * m[15] - m[0] * m[7] * m[13] - m[4] * m[1] * m[15] + m[4] * m[3] * m[13] + m[12] * m[1] * m[7] - m[12] * m[3] * m[5];
        inv[14] = -m[0] * m[5] * m[14] + m[0] * m[6] * m[13] + m[4] * m[1] * m[14] - m[4] * m[2] * m[13] - m[12] * m[1] * m[6] + m[12] * m[2] * m[5];
        inv[3] = -m[1] * m[6] * m[11] + m[1] * m[7] * m[10] + m[5] * m[2] * m[11] - m[5] * m[3] * m[10] - m[9] * m[2] * m[7] + m[9] * m[3] * m[6];
        inv[7] = m[0] * m[6] * m[11] - m[0] * m[7] * m[10] - m[4] * m[2] * m[11] + m[4] * m[3] * m[10] + m[8] * m[2] * m[7] - m[8] * m[3] * m[6];
        inv[11] = -m[0] * m[5] * m[11] + m[0] * m[7] * m[9] + m[4] * m[1] * m[11] - m[4] * m[3] * m[9] - m[8] * m[1] * m[7] + m[8] * m[3] * m[5];
        inv[15] = m[0] * m[5] * m[10] - m[0] * m[6] * m[9] - m[4] * m[1] * m[10] + m[4] * m[2] * m[9] + m[8] * m[1] * m[6] - m[8] * m[2] * m[5];

        det = m[0] * inv[0] + m[1] * inv[4] + m[2] * inv[8] + m[3] * inv[12];

        if (std::abs(det) < SMALL_NUMBER) return IdentityMatrix(); // 奇异矩阵，返回单位矩阵

        det = 1.0f / det;

        FGASMatrix4x4 Out;
        k = 0;
        for (int i = 0; i < 4; i++) for (int j = 0; j < 4; j++) Out.M[i][j] = inv[k++] * det;

        return Out;
    }
}