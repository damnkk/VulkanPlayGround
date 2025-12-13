#ifndef DATAWRITER_H
#define DATAWRITER_H
#include "sqlite3.h"
#include <string>
#include <vector>
#include <mutex>
#include <cstring>
#include <filesystem>

namespace Play
{

class BufferStream
{
public:
    enum class SeekOrigin
    {
        Set,
        Cur,
        End
    };

    BufferStream() = default;
    explicit BufferStream(std::vector<uint8_t> data);

    // 基础读取：读取指定字节到缓冲区
    bool read(void* dst, size_t size);

    // 模板读取：读取 POD 类型
    template <typename T>
    bool read(T& outVal)
    {
        return read(&outVal, sizeof(T));
    }

    // 模板读取：读取 vector
    template <typename T>
    bool read(std::vector<T>& outVec, size_t count)
    {
        size_t bytes = count * sizeof(T);
        if (_cursor + bytes > _data.size()) return false;
        outVec.resize(count);
        std::memcpy(outVec.data(), _data.data() + _cursor, bytes);
        _cursor += bytes;
        return true;
    }

    // 基础写入：写入指定字节
    bool write(const void* src, size_t size);

    // 模板写入：写入 POD 类型
    template <typename T>
    bool write(const T& val)
    {
        return write(&val, sizeof(T));
    }

    // 模板写入：写入 vector
    template <typename T>
    bool write(const std::vector<T>& vec)
    {
        if (vec.empty()) return true;
        return write(vec.data(), vec.size() * sizeof(T));
    }

    // 移动光标
    bool seek(int64_t offset, SeekOrigin origin = SeekOrigin::Set);

    size_t tell() const;
    size_t size() const;
    bool   eof() const;

    // 允许外部填充数据
    std::vector<uint8_t>& getRawData();

private:
    std::vector<uint8_t> _data;
    size_t               _cursor = 0;
};

class DataWriter
{
public:
    DataWriter();
    ~DataWriter();

    // 连接到指定位置的数据库 (如果不存在则创建)
    // path: 相对于根目录（可执行文件所在目录）的路径
    bool open(const std::string& path);

    // 断开连接
    void close();

    // 写入数据：指针 + 大小
    // virtualFileName: 虚拟文件名，作为数据库中的键
    bool write(const std::string& virtualFileName, const void* data, size_t size);

    // 写入数据：从 BufferStream 写入
    bool write(const std::string& virtualFileName, BufferStream& outStream);

    // 写入数据：数组容器 (vector)
    template <typename T>
    bool write(const std::string& virtualFileName, const std::vector<T>& container)
    {
        if (container.empty()) return false;
        return write(virtualFileName, container.data(), container.size() * sizeof(T));
    }

    // 读取数据到 vector (输出二进制数据)
    bool read(const std::string& virtualFileName, std::vector<uint8_t>& outData);

    // 读取数据到 BufferStream
    bool read(const std::string& virtualFileName, BufferStream& outStream);

    // 检查文件是否存在
    bool exists(const std::string& virtualFileName);

    // 获取根目录路径
    std::filesystem::path getRootPath() const;

private:
    sqlite3*              _db = nullptr;
    std::filesystem::path _rootPath;
    std::mutex            _mutex;

    void        ensureTableExists();
    std::string formatPath(const std::string& path);
};

} // namespace Play

#endif // DATAWRITER_H