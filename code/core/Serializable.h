#ifndef SERIALIZABLE_H
#define SERIALIZABLE_H

#include <cereal/access.hpp>
#include <cereal/types/utility.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/polymorphic.hpp>
#include <cereal/types/vector.hpp>
#include <cereal/types/map.hpp>
#include <cereal/types/list.hpp>
#include <cereal/types/array.hpp>
#include <cereal/types/queue.hpp>
#include <cereal/archives/json.hpp>
#include <cereal/archives/binary.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace cereal
{

// GLM vector aliases (vec2, vec3, ivec4, dvec2, u8vec4, etc.) all resolve to
// glm::vec<L, T, Q>, so one overload covers the complete vector family.
template <class Archive, glm::length_t L, typename T, glm::qualifier Q>
inline void serialize(Archive& archive, glm::vec<L, T, Q>& value)
{
    constexpr const char* componentNames[] = {"x", "y", "z", "w"};
    for (glm::length_t index = 0; index < L; ++index)
    {
        archive(cereal::make_nvp(componentNames[index], value[index]));
    }
}

// GLM matrices are column-major and expose each column through operator[].
// Iterating columns first preserves GLM's native layout and works for every
// matCxR alias, including non-square matrices.
template <class Archive, glm::length_t C, glm::length_t R, typename T, glm::qualifier Q>
inline void serialize(Archive& archive, glm::mat<C, R, T, Q>& value)
{
    constexpr const char* columnNames[] = {"column0", "column1", "column2", "column3"};
    for (glm::length_t column = 0; column < C; ++column)
    {
        archive(cereal::make_nvp(columnNames[column], value[column]));
    }
}

// Keep the component order consistent with GLM's component access and the
// project's existing model binary format: x, y, z, w.
template <class Archive, typename T, glm::qualifier Q>
inline void serialize(Archive& archive, glm::qua<T, Q>& value)
{
    archive(cereal::make_nvp("x", value.x), cereal::make_nvp("y", value.y), cereal::make_nvp("z", value.z), cereal::make_nvp("w", value.w));
}

} // namespace cereal

#define BeginSerailize()         \
    friend class cereal::access; \
    template <class Archive>     \
    void serialize(Archive& ar)  \
    {
#define SerailizeBaseClass(className) ar(cereal::make_nvp(#className, cereal::base_class<className>(this)));

#define SerailizeEntry(entry) ar(cereal::make_nvp(#entry, entry));

#define SerailizeFilePath(entry, path) \
    if (entry)                         \
    {                                  \
        path = entry->GetFilePath();   \
    }                                  \
    ar(cereal::make_nvp(#path, path));

#define IfSerailizeInput()                                                                      \
    {                                                                                           \
        cereal::JSONInputArchive*   jsonPtr   = dynamic_cast<cereal::JSONInputArchive*>(&ar);   \
        cereal::BinaryInputArchive* binaryPtr = dynamic_cast<cereal::BinaryInputArchive*>(&ar); \
        if (jsonPtr || binaryPtr)                                                               \
        {
#define IfSerailizeOutput()                                                                       \
    {                                                                                             \
        cereal::JSONOutputArchive*   jsonPtr   = dynamic_cast<cereal::JSONOutputArchive*>(&ar);   \
        cereal::BinaryOutputArchive* binaryPtr = dynamic_cast<cereal::BinaryOutputArchive*>(&ar); \
        if (jsonPtr || binaryPtr)                                                                 \
        {
#define EndIfSerailize() \
    }                    \
    }

#define EndSerailize }
#endif // SERIALIZABLE_H
