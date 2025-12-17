#pragma once
#include <string>
#include <vector>
#include <fstream>
#include <iostream>
#include <sys/stat.h>


#if __cplusplus >= 201703L
#include <filesystem>
namespace fs = std::filesystem;
#endif

namespace GASFileHelper
{
	inline bool FileExists(const std::string& FilePath)
	{
#if __cplusplus >= 201703L
		return fs::exists(FilePath);
#else 
		struct stat buffer;
		return (stat(FilePath.c_str(), &buffer) == 0);
#endif
	}

    inline bool LoadFileToBuffer(const std::string& FilePath, std::vector<uint8_t>& OutBuffer)
    {
        std::ifstream File(FilePath, std::ios::binary | std::ios::ate); // ate: 打开后定位到文件末尾以便获取大小
        if (!File.is_open())
        {
            std::cerr << "[GASFileHelper] Failed to open file for reading: " << FilePath << std::endl;
            return false;
        }

        // 获取文件大小
        std::streamsize FileSize = File.tellg();
        if (FileSize <= 0)
        {
            return false; // 空文件
        }

        File.seekg(0, std::ios::beg); // 回到文件开头

        // 分配内存
        OutBuffer.resize(FileSize);

        // 读取数据
        if (File.read(reinterpret_cast<char*>(OutBuffer.data()), FileSize))
        {
            return true;
        }

        return false;
    }

    // 读取原始文件的二进制数据 (用于计算 XXHash64)
    inline std::vector<uint8_t> ReadRawFile(const std::string& FilePath)
    {
        std::vector<uint8_t> Buffer;
        if (!LoadFileToBuffer(FilePath, Buffer))
        {
            // 如果读取失败，返回一个空的 vector
            return {};
        }
        return Buffer;
    }


     // 将内存数据写入二进制文件
    inline bool SaveBufferToFile(const std::string& FilePath, const void* Data, size_t Size)
    {
        if (!Data || Size == 0) return false;

        std::ofstream File(FilePath, std::ios::binary | std::ios::trunc);
        if (!File.is_open())
        {
            std::cerr << "[GASFileHelper] Failed to open file for writing: " << FilePath << std::endl;
            return false;
        }

        File.write(reinterpret_cast<const char*>(Data), Size);
        return File.good();
    }

    
     //获取文件扩展名 (如 ".fbx", ".gas")  
    inline std::string GetFileExtension(const std::string& FilePath)
    {
#if __cplusplus >= 201703L
        return fs::path(FilePath).extension().string();
#else
        size_t DotPos = FilePath.rfind('.');
        if (DotPos != std::string::npos)
        {
            return FilePath.substr(DotPos);
        }
        return "";
#endif
    }

    
     // 获取不带路径的文件名
    inline std::string GetFileName(const std::string& FilePath)
    {
#if __cplusplus >= 201703L
        return fs::path(FilePath).filename().string();
#else
        size_t SlashPos = FilePath.find_last_of("/\\");
        if (SlashPos != std::string::npos)
        {
            return FilePath.substr(SlashPos + 1);
        }
        return FilePath;
#endif
    }

    //路径拼接 helper
    inline std::string CombinePath(const std::string& Folder, const std::string& Filename)
    {
        if (Folder.empty()) return Filename;

        char LastChar = Folder.back();
        if (LastChar == '/' || LastChar == '\\')
        {
            return Folder + Filename;
        }
        return Folder + "/" + Filename;
    }



}