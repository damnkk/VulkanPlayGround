#ifndef TRANSFORMCOMPONENT_H
#define TRANSFORMCOMPONENT_H
#include "Component.h"

namespace Play
{

class TransformComponent : public Component
{
public:
    TransformComponent() = default;
    ~TransformComponent() override = default;

    void onInit() override;
    void onUpdate(float deltaTime) override;

    void setTransform(const glm::mat4& mat);
    void setPosition(const glm::vec3& pos);
    void setScale(const glm::vec3& scale);

    void setRotate(const glm::vec3& angle);
    void setRotate(const glm::quat& rotate);

    inline glm::mat4 getTransform() const
    {
        return _transform;
    }
    inline glm::vec3 getPosition() const
    {
        return _position;
    }
    inline glm::vec3 getScale() const
    {
        return _scale;
    }
    inline glm::quat getRotate() const
    {
        return _rotate;
    }

    virtual std::string getTypeName() override
    {
        return "Transform Component";
    }
    virtual ComponentType getType() override
    {
        return TRANSFORM_COMPONENT;
    }

    RTTR_ENABLE(Component)

private:
    void splitToMat();
    void splitFromMat();

    glm::mat4 _transform = glm::mat4(1.0f);
    glm::vec3 _position  = glm::vec3(0.0f);
    glm::quat _rotate    = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    glm::vec3 _scale     = glm::vec3(1.0f);

private:
    // clang-format off
    BeginSerailize()
    SerailizeBaseClass(Component)
    SerailizeEntry(_transform)
    SerailizeEntry(_position)
    SerailizeEntry(_rotate)
    SerailizeEntry(_scale)
    EndSerailize
    // clang-format on
};

} // namespace Play

#endif // TRANSFORMCOMPONENT_H
