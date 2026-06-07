#include "SceneAssets.h"

namespace Play
{

AABB transformAABB(const AABB& bounds, const glm::mat4& transform)
{
    const glm::vec3 corners[] = {
        {bounds.min.x, bounds.min.y, bounds.min.z},
        {bounds.max.x, bounds.min.y, bounds.min.z},
        {bounds.min.x, bounds.max.y, bounds.min.z},
        {bounds.max.x, bounds.max.y, bounds.min.z},
        {bounds.min.x, bounds.min.y, bounds.max.z},
        {bounds.max.x, bounds.min.y, bounds.max.z},
        {bounds.min.x, bounds.max.y, bounds.max.z},
        {bounds.max.x, bounds.max.y, bounds.max.z},
    };

    AABB result;
    glm::vec3 first = glm::vec3(transform * glm::vec4(corners[0], 1.0f));
    result.min      = first;
    result.max      = first;

    for (uint32_t i = 1; i < 8; ++i)
    {
        const glm::vec3 point = glm::vec3(transform * glm::vec4(corners[i], 1.0f));
        result.min           = glm::min(result.min, point);
        result.max           = glm::max(result.max, point);
    }

    return result;
}

void expandAABB(AABB& bounds, const AABB& other)
{
    bounds.min = glm::min(bounds.min, other.min);
    bounds.max = glm::max(bounds.max, other.max);
}

} // namespace Play
