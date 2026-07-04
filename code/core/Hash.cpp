#include "Hash.h"

#include "crc32c/crc32c.h"

namespace Play
{
uint64_t memoryHash(void* data, size_t size)
{
    return crc32c::Crc32c(reinterpret_cast<char*>(data), size);
}
} // namespace Play
