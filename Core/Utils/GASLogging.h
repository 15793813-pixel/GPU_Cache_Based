#pragma once

#include <string>
#include <fstream>
#include <iostream>
#include <sstream>
#include <cstdarg>
#include <ctime>
#include <mutex>
#include <filesystem>

// 日志等级枚举
enum class EGASLogLevel
{
    Info,
    Warning,
    Error
};

// GASLogger (单例模式)简单的线程安全日志记录器，支持输出到文件和控制台

class GASLogger
{
public:
    static GASLogger& Get()
    {
        static GASLogger Instance;
        return Instance;
    }

    // 初始化日志系统
    void Initialize(const std::string& LogFilePath = "GAS_Log.txt")
    {
        std::lock_guard<std::mutex> Lock(LogMutex);

        LogFile.open(LogFilePath, std::ios::out | std::ios::trunc);
        if (LogFile.is_open())
        {
            std::cout << "[GASLogger] Log file created at: " << LogFilePath << std::endl;
        }
        else
        {
            std::cerr << "[GASLogger] Failed to create log file: " << LogFilePath << std::endl;
        }
    }

    // 关闭日志文件 
    void Shutdown()
    {
        std::lock_guard<std::mutex> Lock(LogMutex);
        if (LogFile.is_open())
        {
            LogFile.close();
        }
    }

    /** 核心记录函数 */
    void Log(EGASLogLevel Level, const char* File, int Line, const char* Format, ...)
    {
        // 1. 获取时间戳
        std::time_t Now = std::time(nullptr);
        char TimeStr[20];
        struct tm TimeInfo;
        localtime_s(&TimeInfo, &Now); // MSVC 版本参数顺序：(&结果容器, &源时间)
        std::strftime(TimeStr, sizeof(TimeStr), "%H:%M:%S", &TimeInfo);

        // 2. 格式化日志内容
        char Buffer[1024];
        va_list Args;
        va_start(Args, Format);
        vsnprintf(Buffer, sizeof(Buffer), Format, Args);
        va_end(Args);

        // 3. 组装完整消息
        std::stringstream FullMessage;
        FullMessage << "[" << TimeStr << "]";

        switch (Level)
        {
        case EGASLogLevel::Info:    FullMessage << " [INFO] "; break;
        case EGASLogLevel::Warning: FullMessage << " [WARN] "; break;
        case EGASLogLevel::Error:   FullMessage << " [ERR ] "; break;
        }

        FullMessage << Buffer;

        // 附加文件和行号 (仅 Warning 和 Error 显示，保持 Info 简洁)
        if (Level != EGASLogLevel::Info)
        {
            // 只显示文件名，不显示完整路径
            std::string FileName = std::filesystem::path(File).filename().string();
            FullMessage << " (" << FileName << ":" << Line << ")";
        }

        std::string FinalStr = FullMessage.str();

        // 4. 线程安全写入
        {
            std::lock_guard<std::mutex> Lock(LogMutex);

            if (Level == EGASLogLevel::Error)
            {
                std::cerr << FinalStr << std::endl;
            }
            else
            {
                std::cout << FinalStr << std::endl;
            }

            // 输出到文件
            if (LogFile.is_open())
            {
                LogFile << FinalStr << std::endl;
                LogFile.flush(); // 确保崩溃时日志已写入磁盘
            }
        }
    }

private:
    GASLogger() = default;
    ~GASLogger() { Shutdown(); }

    // 禁止拷贝
    GASLogger(const GASLogger&) = delete;
    GASLogger& operator=(const GASLogger&) = delete;

    std::ofstream LogFile;
    std::mutex LogMutex;
};


// 普通信息
#define GAS_LOG(Format, ...) \
    GASLogger::Get().Log(EGASLogLevel::Info, __FILE__, __LINE__, Format, ##__VA_ARGS__)

// 警告
#define GAS_LOG_WARN(Format, ...) \
    GASLogger::Get().Log(EGASLogLevel::Warning, __FILE__, __LINE__, Format, ##__VA_ARGS__)

// 错误
#define GAS_LOG_ERROR(Format, ...) \
    GASLogger::Get().Log(EGASLogLevel::Error, __FILE__, __LINE__, Format, ##__VA_ARGS__)

// 断言检查 (如果条件为假，打印错误并(可选)中断程序)
#define GAS_CHECK(Condition, Format, ...) \
    do { \
        if (!(Condition)) { \
            GAS_LOG_ERROR("Assertion Failed: %s. " Format, #Condition, ##__VA_ARGS__); \
            /* 如果需要调试中断，可以解开下面这行 */ \
            /* std::abort(); */ \
        } \
    } while(0)