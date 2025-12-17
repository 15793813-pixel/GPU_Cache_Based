#include "GASMetadataStorage.h"
#include <sstream>
#include <iostream>
#include "GASLogging.h"

// 数据库表定义 - 增加了 FileHash 字段并指定为 INTEGER
const char* SQL_CREATE_TABLE = R"(
    CREATE TABLE IF NOT EXISTS Assets (
        GUID INTEGER PRIMARY KEY NOT NULL,
        Name TEXT NOT NULL,
        Type INTEGER NOT NULL,
        BinaryFilePath TEXT NOT NULL,
        FileHash INTEGER,
        FrameCount INTEGER,
        Duration REAL,
        BoneCount INTEGER,
        VerticeCount INTEGER,
        MeshCount INTEGER
    );
)";

GASMetadataStorage::GASMetadataStorage() : DB(nullptr) {}

GASMetadataStorage::~GASMetadataStorage()
{
    if (DB)
    {
        sqlite3_close(DB);
        DB = nullptr;
    }
}

// 初始化数据库，创建表结构
bool GASMetadataStorage::Initialize(const std::string& DBPath)
{
    //打开或创建数据库文件
    int rc = sqlite3_open(DBPath.c_str(), &DB);
    if (rc != SQLITE_OK)
    {
        GAS_LOG_ERROR("Cannot open database: %s (Path: %s)", sqlite3_errmsg(DB), DBPath.c_str());
        return false;
    }
    //执行建表语句 (SQL_CREATE_TABLE)
    char* zErrMsg = 0;
    rc = sqlite3_exec(DB, SQL_CREATE_TABLE, 0, 0, &zErrMsg);
    if (rc != SQLITE_OK)
    {
        GAS_LOG_ERROR("SQL error during table creation: %s", zErrMsg);
        sqlite3_free(zErrMsg);
        return false;
    }
    return true;
}

// 注册资产元数据 - 增加了 FileHash 的绑定
bool GASMetadataStorage::RegisterAsset(const FGASAssetMetadata& Metadata)
{
    if (!DB) return false;

    // 增加了 FileHash 字段的插入逻辑
    const char* sql = "INSERT OR REPLACE INTO Assets (GUID, Name, Type, BinaryFilePath, FileHash, FrameCount, Duration, BoneCount, VerticeCount, MeshCount) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?);";

    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(DB, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) return false;

    // 绑定参数
    sqlite3_bind_int64(stmt, 1, Metadata.GUID);
    sqlite3_bind_text(stmt, 2, Metadata.Name.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 3, (int)Metadata.Type);
    sqlite3_bind_text(stmt, 4, Metadata.BinaryFilePath.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int64(stmt, 5, (sqlite3_int64)Metadata.FileHash); // 绑定哈希值
    sqlite3_bind_int(stmt, 6, Metadata.FrameCount);
    sqlite3_bind_double(stmt, 7, Metadata.Duration);
    sqlite3_bind_int(stmt, 8, Metadata.BoneCount);
    sqlite3_bind_int(stmt, 9, Metadata.VerticeCount);
    sqlite3_bind_int(stmt, 10, Metadata.MeshCount);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return rc == SQLITE_DONE;
}

// 通过 GUID 查找元数据
bool GASMetadataStorage::QueryAssetByGUID(uint64_t GUID, FGASAssetMetadata& OutMetadata) const
{
    if (!DB) return false;

    const char* sql = "SELECT Name, Type, BinaryFilePath, FileHash, FrameCount, Duration, BoneCount, VerticeCount, MeshCount FROM Assets WHERE GUID = ?;";
    sqlite3_stmt* stmt;

    int rc = sqlite3_prepare_v2(DB, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) return false;

    sqlite3_bind_int64(stmt, 1, GUID);

    rc = sqlite3_step(stmt);

    if (rc == SQLITE_ROW)
    {
        OutMetadata.GUID = GUID;
        OutMetadata.Name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        OutMetadata.Type = static_cast<EGASAssetType>(sqlite3_column_int(stmt, 1));
        OutMetadata.BinaryFilePath = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        OutMetadata.FileHash = (uint64_t)sqlite3_column_int64(stmt, 3); // 读取哈希值
        OutMetadata.FrameCount = sqlite3_column_int(stmt, 4);
        OutMetadata.Duration = (float)sqlite3_column_double(stmt, 5);
        OutMetadata.BoneCount = sqlite3_column_int(stmt, 6);
        OutMetadata.VerticeCount = sqlite3_column_int(stmt, 7);
        OutMetadata.MeshCount = sqlite3_column_int(stmt, 8);

        sqlite3_finalize(stmt);
        return true;
    }

    sqlite3_finalize(stmt);
    return false;
}

// 查找所有资产元数据
std::vector<FGASAssetMetadata> GASMetadataStorage::QueryAllAssets() const
{
    std::vector<FGASAssetMetadata> Results;
    if (!DB) return Results;

    const char* sql = "SELECT GUID, Name, Type, BinaryFilePath, FileHash, FrameCount, Duration, BoneCount, VerticeCount, MeshCount FROM Assets ORDER BY Name ASC;";
    sqlite3_stmt* stmt;

    int rc = sqlite3_prepare_v2(DB, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) return Results;

    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW)
    {
        FGASAssetMetadata Meta;
        Meta.GUID = sqlite3_column_int64(stmt, 0);
        Meta.Name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        Meta.Type = static_cast<EGASAssetType>(sqlite3_column_int(stmt, 2));
        Meta.BinaryFilePath = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        Meta.FileHash = (uint64_t)sqlite3_column_int64(stmt, 4); // 读取哈希值
        Meta.FrameCount = sqlite3_column_int(stmt, 5);
        Meta.Duration = (float)sqlite3_column_double(stmt, 6);
        Meta.BoneCount = sqlite3_column_int(stmt, 7);
        Meta.VerticeCount = sqlite3_column_int(stmt, 8);
        Meta.MeshCount = sqlite3_column_int(stmt, 9);

        Results.push_back(Meta);
    }

    sqlite3_finalize(stmt);
    return Results;
}