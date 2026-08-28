#ifndef UUID_H
#define UUID_H
#include "Serializable.h"
#include <stduuid/uuid.h>
namespace cereal
{
// Persist UUIDs as their canonical textual form instead of their 16-byte
// implementation representation. cereal minimal serialization makes the UUID
// appear as a JSON string while keeping the same behavior for binary archives.
template <class Archive>
inline std::string save_minimal(const Archive&, const uuids::uuid& value)
{
    return uuids::to_string(value);
}

template <class Archive>
inline void load_minimal(const Archive&, uuids::uuid& value, const std::string& serializedValue)
{
    const auto parsedValue = uuids::uuid::from_string(serializedValue);
    if (!parsedValue)
    {
        throw cereal::Exception("Invalid UUID string: " + serializedValue);
    }

    value = *parsedValue;
}
} // namespace cereal

using GUID = uuids::uuid;
#endif // UUID_H