#include "TransformComponent.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/matrix_decompose.hpp>
CEREAL_REGISTER_TYPE(Play::TransformComponent)
namespace Play
{

namespace
{
constexpr float kQuaternionEpsilon = 0.000001f;

glm::quat normalizeOrIdentity(const glm::quat& value)
{
    if (glm::dot(value, value) <= kQuaternionEpsilon * kQuaternionEpsilon)
    {
        return glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    }

    return glm::normalize(value);
}
} // namespace

void TransformComponent::onInit()
{
    // The matrix is the authoritative value when a component is first loaded.
    // This also repairs components deserialized by older versions that only
    // persisted the matrix.
    splitFromMat();
}

void TransformComponent::onUpdate(float deltaTime)
{
    (void) deltaTime;
}

void TransformComponent::setTransform(const glm::mat4& mat)
{
    _transform = mat;
    splitFromMat();
}

void TransformComponent::setPosition(const glm::vec3& pos)
{
    _position = pos;
    splitToMat();
}

void TransformComponent::setScale(const glm::vec3& scale)
{
    _scale = scale;
    splitToMat();
}

void TransformComponent::setRotate(const glm::vec3& angle)
{
    _rotate = normalizeOrIdentity(glm::quat(glm::radians(angle)));
    splitToMat();
}

void TransformComponent::setRotate(const glm::quat& rotate)
{
    _rotate = normalizeOrIdentity(rotate);
    splitToMat();
}

void TransformComponent::splitToMat()
{
    _rotate    = normalizeOrIdentity(_rotate);
    _transform = glm::translate(glm::mat4(1.0f), _position) * glm::mat4_cast(_rotate);
    _transform = glm::scale(_transform, _scale);
}

void TransformComponent::splitFromMat()
{
    glm::vec3 translation(0.0f);
    glm::vec3 scale(1.0f);
    glm::vec3 skew(0.0f);
    glm::vec4 perspective(0.0f);
    glm::quat rotation(1.0f, 0.0f, 0.0f, 0.0f);

    if (glm::decompose(_transform, scale, rotation, translation, skew, perspective))
    {
        _position = translation;
        _scale    = scale;
        _rotate   = normalizeOrIdentity(rotation);
        return;
    }

    // A matrix with a zero scale on one or more axes cannot always be
    // decomposed by glm::decompose. Translation is still unambiguous, and the
    // column lengths provide the best defined scale values in that case.
    _position = glm::vec3(_transform[3]);
    _scale.x  = glm::length(glm::vec3(_transform[0]));
    _scale.y  = glm::length(glm::vec3(_transform[1]));
    _scale.z  = glm::length(glm::vec3(_transform[2]));

    glm::mat3 rotationMatrix(1.0f);
    for (int axis = 0; axis < 3; ++axis)
    {
        const glm::vec3 column = glm::vec3(_transform[axis]);
        const float     length = glm::length(column);
        if (length > kQuaternionEpsilon)
        {
            rotationMatrix[axis] = column / length;
        }
    }

    if (glm::determinant(rotationMatrix) < 0.0f)
    {
        rotationMatrix[0] = -rotationMatrix[0];
        _scale.x          = -_scale.x;
    }

    _rotate = normalizeOrIdentity(glm::quat_cast(rotationMatrix));
}

} // namespace Play
