#pragma once

#include <string>
#include <memory>
#include "../Types/GASAsset.h"
#include "../Types/GASCoreTypes.h" // 包含 FGASFileHeader 等定义

/**
 * @class GASBinarySerializer
 * @brief 负责将 GASAsset 及其子类对象序列化和反序列化为紧凑的自定义二进制文件（.gas）。
 * 目标是实现零拷贝式加载。
 */
class GASBinarySerializer
{
public:
    /**
     * 将 GASAsset 对象序列化到磁盘文件。
     * 写入顺序：Header -> 骨骼/动画数据数组。
     * @param Asset 待写入的资产对象
     * @param FilePath 目标文件路径
     * @return 是否成功
     */
    static bool SaveAssetToDisk(const GASAsset* Asset, const std::string& FilePath);

    /**
     * 从磁盘文件反序列化资产。
     * 注意：这里返回的是基类指针，由调用方（如 GASAssetManager）负责进行动态转换。
     * @param FilePath 目标文件路径
     * @return 成功则返回新创建的资产指针，失败则返回 nullptr
     */
    static std::shared_ptr<GASAsset> LoadAssetFromDisk(const std::string& FilePath);

private:
    /** 辅助函数：将内存块写入文件 */
    static bool WriteData(std::ofstream& Stream, const void* Data, size_t Size);

    /** 辅助函数：从文件读取内存块 */
    static bool ReadData(std::ifstream& Stream, void* Data, size_t Size);

    /** 辅助函数：写入 Skeleton 专有数据 */
    static bool SerializeSkeleton(std::ofstream& Stream, const GASSkeleton* Skeleton);

    /** 辅助函数：写入 Animation 专有数据 */
    static bool SerializeAnimation(std::ofstream& Stream, const GASAnimation* Animation);

    /** 辅助函数：读取 Skeleton 专有数据 */
    static bool DeserializeSkeleton(std::ifstream& Stream, GASSkeleton* Skeleton);

    /** 辅助函数：读取 Animation 专有数据 */
    static bool DeserializeAnimation(std::ifstream& Stream, GASAnimation* Animation);
};