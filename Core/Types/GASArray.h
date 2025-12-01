#pragma once

#include <vector>
#include <algorithm>
#include <cassert>
#include<cstdint>
#include <utility>


template <typename T>
class GASArray
{
public:
	using ElementType = T;
	
	GASArray() = default;

	GASArray(std::initializer_list<T> InitList) :Data(InitList) {};

	TGASArray(const TGASArray& Other) = default;
	TGASArray(TGASArray&& Other) noexcept = default;
	TGASArray& operator=(const TGASArray& Other) = default;
	TGASArray& operator=(TGASArray&& Other) noexcept = default;


    int32_t Num() const
    {
        return static_cast<int32_t>(Data.size());
    }

    /** 添加元素 (UE: Add())，返回新元素的索引 */
    int32_t Add(const T& Item)
    {
        Data.push_back(Item);
        return Num() - 1;
    }

    /** 添加元素 (移动语义) */
    int32_t Add(T&& Item)
    {
        Data.push_back(std::move(Item));
        return Num() - 1;
    }

    /** 预留空间 (UE: Reserve())，避免频繁内存重分配 */
    void Reserve(int32_t Number)
    {
        Data.reserve(Number);
    }
    void SetNum(int32_t NewNum)
    {
        Data.resize(NewNum);
    }

    //获取原始数据指针 (UE: GetData())

    T* GetData()
    {
        return Data.empty() ? nullptr : Data.data();
    }

    const T* GetData() const
    {
        return Data.empty() ? nullptr : Data.data();
    }

    /** 获取整个数据块的字节大小 (用于序列化) */
    size_t GetTotalSizeInBytes() const
    {
        return Data.size() * sizeof(T);
    }
    T& operator[](int32_t Index)
    {
        assert(IsValidIndex(Index));
        return Data[Index];
    }

    const T& operator[](int32_t Index) const
    {
        assert(IsValidIndex(Index));
        return Data[Index];
    }

    /** 检查索引是否有效 */
    bool IsValidIndex(int32_t Index) const
    {
        return Index >= 0 && Index < Num();
    }

    /** 查找元素，返回 true 如果找到，OutIndex 存储索引 */
    bool Find(const T& Item, int32_t& OutIndex) const
    {
        auto It = std::find(Data.begin(), Data.end(), Item);
        if (It != Data.end())
        {
            OutIndex = static_cast<int32_t>(std::distance(Data.begin(), It));
            return true;
        }
        OutIndex = -1;
        return false;
    }

    bool Contains(const T& Item) const
    {
        return std::find(Data.begin(), Data.end(), Item) != Data.end();
    }

    // =========================================================
    // 移除与清理 (Remove & Empty)
    // =========================================================

    /** 清空数组 (UE: Empty()) */
    void Empty(int32_t Slack = 0)
    {
        Data.clear();
        if (Slack > 0)
        {
            Data.reserve(Slack);
        }
    }

    /** 移除指定索引的元素，后续元素前移 (保持顺序，较慢) */
    void RemoveAt(int32_t Index)
    {
        if (IsValidIndex(Index))
        {
            Data.erase(Data.begin() + Index);
        }
    }

    /** * 移除指定索引的元素 (UE: RemoveAtSwap)
     * 将最后一个元素移到被删除位置，速度快但不保持顺序
     */
    void RemoveAtSwap(int32_t Index)
    {
        if (IsValidIndex(Index))
        {
            if (Index < Num() - 1)
            {
                std::swap(Data[Index], Data.back());
            }
            Data.pop_back();
        }
    }
    auto begin() { return Data.begin(); }
    auto end() { return Data.end(); }
    auto begin() const { return Data.begin(); }
    auto end() const { return Data.end(); }

private:
    std::vector<T> Data;
};
