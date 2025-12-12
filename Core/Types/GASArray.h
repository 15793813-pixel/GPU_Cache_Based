#pragma once

#include <vector>
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <utility>

template <typename T>
class GASArray
{
public:
    using ElementType = T;

    GASArray() = default;

    GASArray(std::initializer_list<T> InitList) : Data(InitList) {};

    GASArray(const GASArray& Other) = default;
    GASArray(GASArray&& Other) noexcept = default;
    GASArray& operator=(const GASArray& Other) = default;
    GASArray& operator=(GASArray&& Other) noexcept = default;

    int32_t Num() const
    {
        return static_cast<int32_t>(Data.size());
    }

    int32_t Add(const T& Item)
    {
        Data.push_back(Item);
        return Num() - 1;
    }

    int32_t Add(T&& Item)
    {
        Data.push_back(std::move(Item));
        return Num() - 1;
    }

    void Reserve(int32_t Number)
    {
        Data.reserve(Number);
    }

    void SetNum(int32_t NewNum)
    {
        Data.resize(NewNum);
    }

    void Resize(int32_t NewNum)
    {
        Data.resize(NewNum);
    }

    T* GetData()
    {
        return Data.empty() ? nullptr : Data.data();
    }

    const T* GetData() const
    {
        return Data.empty() ? nullptr : Data.data();
    }

    size_t GetTotalSizeInBytes() const
    {
        return Data.size() * sizeof(T);
    }

    T& operator[](int32_t Index)
    {
        return Data[Index];
    }

    const T& operator[](int32_t Index) const
    {
        // assert(IsValidIndex(Index));
        return Data[Index];
    }

    bool IsValidIndex(int32_t Index) const
    {
        return Index >= 0 && Index < Num();
    }

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

    void Empty(int32_t Slack = 0)
    {
        Data.clear();
        if (Slack > 0)
        {
            Data.reserve(Slack);
        }
    }

    void RemoveAt(int32_t Index)
    {
        if (IsValidIndex(Index))
        {
            Data.erase(Data.begin() + Index);
        }
    }

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

    // 标准库迭代器支持
    auto begin() { return Data.begin(); }
    auto end() { return Data.end(); }
    auto begin() const { return Data.begin(); }
    auto end() const { return Data.end(); }

private:
    std::vector<T> Data;
};