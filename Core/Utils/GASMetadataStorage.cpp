#include "GASMetadataStorage.h"
#include <sstream>
#include <iostream> // 替代 GASLogging.h 进行简单输出
// #include "GASLogging.h" // 如果你有自己的日志库，可以取消注释

// 数据库表定义
const char* SQL_CREATE_TABLE = R"(
    CREATE TABLE IF NOT EXISTS Assets (
        GUID INTEGER PRIMARY KEY NOT NULL,
        Name TEXT NOT NULL,
        Type INTEGER NOT NULL,
        BinaryFilePath TEXT NOT NULL,
        FrameCount INTEGER,
        Duration REAL,
        BoneCount INTEGER
    );
)";

GASMetadataStorage::GASMetadataStorage() : DB(nullptr) {}

GASMetadataStorage::~GASMetadataStorage()
{
    if (DB)
    {
        // 确保关闭数据库连接
        sqlite3_close(DB);
        DB = nullptr;
    }
}

/**
 * 初始化数据库，创建表结构
 */
bool GASMetadataStorage::Initialize(const std::string& DBPath)
{
    // 打开或创建数据库文件
    int rc = sqlite3_open(DBPath.c_str(), &DB);
    if (rc != SQLITE_OK)
    {
        std::cerr << "ERROR: Cannot open database: " << sqlite3_errmsg(DB) << std::endl;
        return false;
    }

    // 执行建表语句
    char* zErrMsg = 0;
    rc = sqlite3_exec(DB, SQL_CREATE_TABLE, 0, 0, &zErrMsg);
    if (rc != SQLITE_OK)
    {
        std::cerr << "ERROR: SQL error during table creation: " << zErrMsg << std::endl;
        sqlite3_free(zErrMsg);
        return false;
    }

    return true;
}

/**
 * 导入成功后，向数据库注册资产的元数据
 */
bool GASMetadataStorage::RegisterAsset(const FGASAssetMetadata& Metadata)
{
    if (!DB) return false;

    // 尝试执行 INSERT OR REPLACE，避免重复 GUID 导致的错误
    const char* sql = "INSERT OR REPLACE INTO Assets (GUID, Name, Type, BinaryFilePath, FrameCount, Duration, BoneCount) "
        "VALUES (?, ?, ?, ?, ?, ?, ?);";

    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(DB, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) return false;

    // 绑定参数 (使用 sqlite3_bind_xxx 防止 SQL 注入)
    sqlite3_bind_int64(stmt, 1, Metadata.GUID);
    sqlite3_bind_text(stmt, 2, Metadata.Name.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 3, (int)Metadata.Type);
    sqlite3_bind_text(stmt, 4, Metadata.BinaryFilePath.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 5, Metadata.FrameCount);
    sqlite3_bind_double(stmt, 6, Metadata.Duration);
    sqlite3_bind_int(stmt, 7, Metadata.BoneCount);

    // 执行插入
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return rc == SQLITE_DONE;
}


/**
 * 通过 GUID 查找元数据 (供运行时加载器查找文件路径)
 */
bool GASMetadataStorage::QueryAssetByGUID(uint64_t GUID, FGASAssetMetadata& OutMetadata) const
{
    if (!DB) return false;

    const char* sql = "SELECT Name, Type, BinaryFilePath, FrameCount, Duration, BoneCount FROM Assets WHERE GUID = ?;";
    sqlite3_stmt* stmt;

    // 准备 SQL 语句
    int rc = sqlite3_prepare_v2(DB, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) return false;

    // 绑定 GUID 参数
    sqlite3_bind_int64(stmt, 1, GUID);

    // 执行查询
    rc = sqlite3_step(stmt);

    if (rc == SQLITE_ROW)
    {
        // 读取结果并填充 OutMetadata 结构体
        OutMetadata.GUID = GUID;
        OutMetadata.Name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        OutMetadata.Type = static_cast<EGASAssetType>(sqlite3_column_int(stmt, 1));
        OutMetadata.BinaryFilePath = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        OutMetadata.FrameCount = sqlite3_column_int(stmt, 3);
        OutMetadata.Duration = (float)sqlite3_column_double(stmt, 4);
        OutMetadata.BoneCount = sqlite3_column_int(stmt, 5);

        sqlite3_finalize(stmt);
        return true;
    }

    // 清理资源
    sqlite3_finalize(stmt);
    return false; // 未找到或发生错误
}

/**
 * 查找所有资产元数据 (供编辑器资产列表显示)
 */
std::vector<FGASAssetMetadata> GASMetadataStorage::QueryAllAssets() const
{
    std::vector<FGASAssetMetadata> Results;
    if (!DB) return Results;

    const char* sql = "SELECT GUID, Name, Type, BinaryFilePath, FrameCount, Duration, BoneCount FROM Assets ORDER BY Name ASC;";
    sqlite3_stmt* stmt;

    int rc = sqlite3_prepare_v2(DB, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) return Results;

    // 循环读取所有行
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW)
    {
        FGASAssetMetadata Meta;
        Meta.GUID = sqlite3_column_int64(stmt, 0);
        Meta.Name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        Meta.Type = static_cast<EGASAssetType>(sqlite3_column_int(stmt, 2));
        Meta.BinaryFilePath = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        Meta.FrameCount = sqlite3_column_int(stmt, 4);
        Meta.Duration = (float)sqlite3_column_double(stmt, 5);
        Meta.BoneCount = sqlite3_column_int(stmt, 6);

        Results.push_back(Meta);
    }

    sqlite3_finalize(stmt);
    return Results;
}