#pragma once

#include <string>
#include <memory>
#include "../Types/GASAsset.h"
#include "../Types/GASCoreTypes.h" 

// 负责将 GASAsset 及其子类对象序列化和反序列化为紧凑的自定义二进制文件（.gas）。

class GASBinarySerializer
{
public:
    // 将 GASAsset 对象序列化到磁盘文件。写入顺序：Header -> 骨骼/动画/mesh数据数组。
    static bool SaveAssetToDisk(const GASAsset* Asset, const std::string& FilePath);

    // 从磁盘文件反序列化资产。这里返回的是基类指针，由调用方（如 GASAssetManager）负责进行动态转换。
   
    static std::shared_ptr<GASAsset> LoadAssetFromDisk(const std::string& FilePath);

private:
    //辅助函数：将内存块写入文件
    static bool WriteData(std::ofstream& Stream, const void* Data, size_t Size);

    // 辅助函数：从文件读取内存块 
    static bool ReadData(std::ifstream& Stream, void* Data, size_t Size);

    //辅助函数：写入 Skeleton 专有数据
    static bool SerializeSkeleton(std::ofstream& Stream, const GASSkeleton* Skeleton);

    //辅助函数：写入 Animation 专有数据 
    static bool SerializeAnimation(std::ofstream& Stream, const GASAnimation* Animation);

    //辅助函数：写入 Mesh专有数据 
    static bool SerializeMesh(std::ofstream& Stream, const GASMesh* Mesh);

    //辅助函数：读取 Skeleton 专有数据 
    static bool DeserializeSkeleton(std::ifstream& Stream, GASSkeleton* Skeleton);

    // 辅助函数：读取 Animation 专有数据 
    static bool DeserializeAnimation(std::ifstream& Stream, GASAnimation* Animation);

    // 辅助函数：读取Mesh专有数据 
    static bool DeserializeMesh(std::ifstream& Stream, GASMesh* Mesh);
};