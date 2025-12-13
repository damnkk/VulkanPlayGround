#include "DataWriter.h"
#include <iostream>
#include <algorithm>
#include <filesystem>

#ifdef _WIN32
#include <windows.h>
#endif

namespace Play
{

BufferStream::BufferStream(std::vector<uint8_t> data) : _data(std::move(data)), _cursor(0) {}

bool BufferStream::read(void* dst, size_t size)
{
    if (_cursor + size > _data.size()) return false;
    std::memcpy(dst, _data.data() + _cursor, size);
    _cursor += size;
    return true;
}

bool BufferStream::write(const void* src, size_t size)
{
    if (size == 0) return true;
    size_t requiredSize = _cursor + size;
    if (requiredSize > _data.size())
    {
        _data.resize(requiredSize);
    }
    std::memcpy(_data.data() + _cursor, src, size);
    _cursor += size;
    return true;
}

bool BufferStream::seek(int64_t offset, SeekOrigin origin)
{
    int64_t newPos = 0;
    switch (origin)
    {
        case SeekOrigin::Set:
            newPos = offset;
            break;
        case SeekOrigin::Cur:
            newPos = static_cast<int64_t>(_cursor) + offset;
            break;
        case SeekOrigin::End:
            newPos = static_cast<int64_t>(_data.size()) + offset;
            break;
    }

    if (newPos < 0 || newPos > static_cast<int64_t>(_data.size())) return false;
    _cursor = static_cast<size_t>(newPos);
    return true;
}

size_t BufferStream::tell() const
{
    return _cursor;
}

size_t BufferStream::size() const
{
    return _data.size();
}

bool BufferStream::eof() const
{
    return _cursor >= _data.size();
}

std::vector<uint8_t>& BufferStream::getRawData()
{
    return _data;
}

std::filesystem::path GetExecutablePath()
{
    // 获取可执行文件所在目录
#ifdef _WIN32
    char exePath[MAX_PATH];
    GetModuleFileNameA(nullptr, exePath, MAX_PATH);
    return std::filesystem::path(exePath).parent_path();
#else
    // Linux/macOS
    return std::filesystem::canonical("/proc/self/exe").parent_path();
#endif
}

DataWriter::DataWriter()
{
    _rootPath = GetExecutablePath();
}

DataWriter::~DataWriter()
{
    close();
}

bool DataWriter::open(const std::string& path)
{
    std::lock_guard<std::mutex> lock(_mutex);
    close(); // 确保之前的连接已关闭

    // 将相对路径转换为相对于根目录的绝对路径
    std::filesystem::path fullPath = _rootPath / path;

    // 创建所需的目录
    std::filesystem::path dirPath = fullPath.parent_path();
    if (!dirPath.empty() && !std::filesystem::exists(dirPath))
    {
        std::filesystem::create_directories(dirPath);
    }

    int rc = sqlite3_open(fullPath.string().c_str(), &_db);
    if (rc)
    {
        std::cerr << "Can't open database: " << sqlite3_errmsg(_db) << std::endl;
        close();
        return false;
    }

    ensureTableExists();
    return true;
}

void DataWriter::close()
{
    if (_db)
    {
        sqlite3_close(_db);
        _db = nullptr;
    }
}

void DataWriter::ensureTableExists()
{
    // 创建一个简单的表：文件名 (主键) | 数据 (Blob)
    // 这里的 SQL 对用户隐藏
    const char* sql =
        "CREATE TABLE IF NOT EXISTS FileSystem ("
        "FileName TEXT PRIMARY KEY NOT NULL,"
        "Data BLOB"
        ");";
    char* errMsg = 0;
    int   rc     = sqlite3_exec(_db, sql, 0, 0, &errMsg);
    if (rc != SQLITE_OK)
    {
        std::cerr << "SQL error: " << errMsg << std::endl;
        sqlite3_free(errMsg);
    }
}

bool DataWriter::write(const std::string& virtualFileName, const void* data, size_t size)
{
    std::lock_guard<std::mutex> lock(_mutex);
    if (!_db) return false;

    std::string formattedName = formatPath(virtualFileName);

    // 使用 INSERT OR REPLACE 来处理新建或覆盖，无需用户关心 UPDATE 还是 INSERT
    const char*   sql = "INSERT OR REPLACE INTO FileSystem (FileName, Data) VALUES (?, ?);";
    sqlite3_stmt* stmt;

    if (sqlite3_prepare_v2(_db, sql, -1, &stmt, 0) != SQLITE_OK)
    {
        std::cerr << "Failed to prepare statement: " << sqlite3_errmsg(_db) << std::endl;
        return false;
    }

    // 绑定参数: ?1 -> FileName, ?2 -> Data
    sqlite3_bind_text(stmt, 1, formattedName.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_blob(stmt, 2, data, (int) size, SQLITE_STATIC);

    int  rc      = sqlite3_step(stmt);
    bool success = (rc == SQLITE_DONE);

    if (!success)
    {
        std::cerr << "Execution failed: " << sqlite3_errmsg(_db) << std::endl;
    }

    sqlite3_finalize(stmt);
    return success;
}

bool DataWriter::read(const std::string& virtualFileName, std::vector<uint8_t>& outData)
{
    std::lock_guard<std::mutex> lock(_mutex);
    if (!_db) return false;

    std::string formattedName = formatPath(virtualFileName);

    const char*   sql = "SELECT Data FROM FileSystem WHERE FileName = ?;";
    sqlite3_stmt* stmt;

    if (sqlite3_prepare_v2(_db, sql, -1, &stmt, 0) != SQLITE_OK)
    {
        return false;
    }

    sqlite3_bind_text(stmt, 1, formattedName.c_str(), -1, SQLITE_STATIC);

    bool success = false;
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        const void* blob  = sqlite3_column_blob(stmt, 0);
        int         bytes = sqlite3_column_bytes(stmt, 0);

        outData.resize(bytes);
        if (bytes > 0)
        {
            memcpy(outData.data(), blob, bytes);
        }
        success = true;
    }

    sqlite3_finalize(stmt);
    return success;
}

bool DataWriter::write(const std::string& virtualFileName, BufferStream& outStream)
{
    // 将 BufferStream 内部的数据写入数据库
    const auto& data = outStream.getRawData();
    return write(virtualFileName, data.data(), data.size());
}

bool DataWriter::read(const std::string& virtualFileName, BufferStream& outStream)
{
    // 直接读取到 BufferStream 内部的 vector 中
    bool success = read(virtualFileName, outStream.getRawData());
    if (success)
    {
        // 读取成功后重置光标
        outStream.seek(0, BufferStream::SeekOrigin::Set);
    }
    return success;
}

bool DataWriter::exists(const std::string& virtualFileName)
{
    std::lock_guard<std::mutex> lock(_mutex);
    if (!_db) return false;

    std::string formattedName = formatPath(virtualFileName);

    const char*   sql = "SELECT 1 FROM FileSystem WHERE FileName = ?;";
    sqlite3_stmt* stmt;

    if (sqlite3_prepare_v2(_db, sql, -1, &stmt, 0) != SQLITE_OK) return false;

    sqlite3_bind_text(stmt, 1, formattedName.c_str(), -1, SQLITE_STATIC);

    bool found = (sqlite3_step(stmt) == SQLITE_ROW);
    sqlite3_finalize(stmt);
    return found;
}

std::string DataWriter::formatPath(const std::string& path)
{
    std::string formatted = path;
    std::replace(formatted.begin(), formatted.end(), '\\', '/');
    return formatted;
}

std::filesystem::path DataWriter::getRootPath() const
{
    return _rootPath;
}

} // namespace Play
