#ifndef HASH_H
#define HASH_H

#include <cstddef>
#include <cstdint>
#include <vector>

namespace Play
{
uint64_t memoryHash(void* data, size_t size);

template <typename T>
uint64_t memoryHash(const std::vector<T>& data)
{
    return memoryHash((void*) data.data(), data.size() * sizeof(T));
}
} // namespace Play

#endif // HASH_H
