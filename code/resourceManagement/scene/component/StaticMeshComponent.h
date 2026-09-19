#ifndef STATICMESHCOMPONENT_H
#define STATICMESHCOMPONENT_H
#include "renderer/MeshCollector.h"
#include "Component.h"
#include "resourceManagement/Model.h"
#include "core/assets/Asset.h"

namespace Play
{
class StaticMeshComponent : public Play::Component, public AssetBinder, public Drawable
{
public:
    StaticMeshComponent() = default;
    virtual ~StaticMeshComponent();
    virtual void onLoad() override;
    virtual void onSave() override;
    virtual void onInit() override;
    virtual void onUpdate(float deltaTime) override;

    virtual std::string getTypeName() override
    {
        return "Static Mesh";
    }

    virtual ComponentType getType() override
    {
        return STATIC_MESH_COMPONENT;
    }

    virtual void collectDrawBatch(std::vector<DrawBatch>& batcheds) override;
    void         setModel(ModelRef model)
    {
        _model = model;
    }
    ModelRef getModel() const { return _model; }
    bool getCastShadow() const { return _castShadow; }
    void setCastShadow(bool value) { _castShadow = value; }

    RTTR_ENABLE(Component)

private:
    ModelRef _model;

    bool _castShadow = true;

private:
    // clang-format off
    BeginSerailize() 
    SerailizeBaseClass(Component) 
    SerailizeBaseClass(AssetBinder) 
    SerailizeEntry(_castShadow) 
    EndSerailize
    // clang-format on
};

} // namespace Play

#endif // STATICMESHCOMPONENT_H
