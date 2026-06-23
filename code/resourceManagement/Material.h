#ifndef MATERIAL_H
#define MATERIAL_H

#include "DescriptorManager.h"
#include "PipelineCacheManager.h"
#include "Resource.h"

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <rttr/rttr_enable.h>
#include <rttr/variant.h>

namespace Play
{

class MaterialInstance;

constexpr ShaderID       MATERIAL_INVALID_SHADER_ID  = ~0U;
constexpr uint32_t       MATERIAL_SHADER_STAGE_COUNT = static_cast<uint32_t>(ShaderStage::eCount);

enum class MaterialParameterKind : uint32_t
{
    eUnknown,
    eScalar,
    eVector,
    eMatrix,
    eTexture,
    eSampler,
    eBuffer
};

enum class MaterialRasterMode : uint32_t
{
    eTriangle,
    eTwoSidedTriangle,
    eWireframe,
    eCulledWireframe
};

enum class MaterialBlendMode : uint32_t
{
    eOpaque,
    eMasked,
    eAlphaBlend,
    ePremultipliedAlpha,
    eAdditive,
    eMultiply
};

enum class MaterialDepthCompareMode : uint32_t
{
    eLess,
    eLessEqual,
    eEqual,
    eGreater,
    eGreaterEqual,
    eAlways
};

struct MaterialRenderState
{
    MaterialRasterMode       rasterMode           = MaterialRasterMode::eTriangle;
    MaterialBlendMode        blendMode            = MaterialBlendMode::eOpaque;
    MaterialDepthCompareMode depthCompareMode     = MaterialDepthCompareMode::eLessEqual;
    bool                     depthTestEnable      = true;
    bool                     depthWriteEnable     = true;
    uint32_t                 colorAttachmentCount = 1;
    VkColorComponentFlags    colorWriteMask       = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT |
                                             VK_COLOR_COMPONENT_A_BIT;
};

using MaterialParamMap         = std::unordered_map<std::string, rttr::variant>;
using MaterialParamOverrideMap = MaterialParamMap;

struct MaterialParamValidationResult
{
    bool                     valid             = true;
    uint32_t                 checkedParamCount = 0;
    uint32_t                 ignoredParamCount = 0;
    std::vector<std::string> typeMismatchParams;
};

struct MaterialShaderSet
{
    MaterialShaderSet()
    {
        for (auto& shaderID : shaderIDs)
        {
            shaderID = MATERIAL_INVALID_SHADER_ID;
        }
    }

    MaterialShaderSet(ShaderID vertexShaderID, ShaderID fragmentShaderID) : MaterialShaderSet()
    {
        setShader(ShaderStage::eVertex, vertexShaderID);
        setShader(ShaderStage::eFragment, fragmentShaderID);
    }

    void setShader(ShaderStage stage, ShaderID shaderID)
    {
        const uint32_t index = static_cast<uint32_t>(stage);
        if (index >= MATERIAL_SHADER_STAGE_COUNT) return;
        shaderIDs[index] = shaderID;
    }

    ShaderID getShader(ShaderStage stage) const
    {
        const uint32_t index = static_cast<uint32_t>(stage);
        if (index >= MATERIAL_SHADER_STAGE_COUNT) return MATERIAL_INVALID_SHADER_ID;
        return shaderIDs[index];
    }

    bool hasShader(ShaderStage stage) const
    {
        return getShader(stage) != MATERIAL_INVALID_SHADER_ID;
    }

    std::array<ShaderID, MATERIAL_SHADER_STAGE_COUNT> shaderIDs;
};

struct MaterialDescriptorDeclaration
{
    std::string        name;
    uint32_t           bindingIdx       = 0;
    uint32_t           descriptorCount  = 1;
    VkDescriptorType   descriptorType   = VK_DESCRIPTOR_TYPE_MAX_ENUM;
    VkShaderStageFlags shaderStageFlags = VK_SHADER_STAGE_ALL;
};

struct MaterialParameterDeclaration
{
    std::string           name;
    MaterialParameterKind kind = MaterialParameterKind::eUnknown;
    rttr::variant         defaultValue;
    std::string           descriptorName;
    uint32_t              byteOffset = 0;
    uint32_t              byteSize   = 0;
};

struct MaterialDescriptorBinding
{
    MaterialDescriptorDeclaration declaration;
    rttr::variant                 resource;
    bool                          resourceOverridden = false;

    bool hasResourceOverride() const
    {
        return resourceOverridden;
    }

    bool isBound() const
    {
        return resourceOverridden && resource.is_valid();
    }

    void clearResource()
    {
        resource           = rttr::variant();
        resourceOverridden = false;
    }
};

using MaterialDescriptorDeclarationMap = std::unordered_map<std::string, MaterialDescriptorDeclaration>;
using MaterialParameterDeclarationMap  = std::unordered_map<std::string, MaterialParameterDeclaration>;
using MaterialDescriptorBindingMap     = std::unordered_map<std::string, MaterialDescriptorBinding>;

class Material
{
public:
    Material() = default;
    explicit Material(const std::string& name) : _name(name) {}
    Material(ShaderID vertexShaderID, ShaderID fragmentShaderID, const std::string& name = "Material")
        : _name(name), _shaderSet(vertexShaderID, fragmentShaderID)
    {
        refreshShaderReflection();
    }

    Material(const MaterialShaderSet& shaderSet, const std::string& name = "Material") : _name(name), _shaderSet(shaderSet)
    {
        refreshShaderReflection();
    }
    virtual ~Material();

    const std::string& getName() const
    {
        return _name;
    }

    Material& setName(const std::string& name)
    {
        _name = name;
        return *this;
    }

    Material& setShader(ShaderStage stage, ShaderID shaderID)
    {
        _shaderSet.setShader(stage, shaderID);
        refreshShaderReflection();
        return *this;
    }

    Material& setShader(ShaderID vertexShaderID, ShaderID fragmentShaderID)
    {
        _shaderSet = MaterialShaderSet(vertexShaderID, fragmentShaderID);
        refreshShaderReflection();
        return *this;
    }

    Material& setShaders(const MaterialShaderSet& shaderSet)
    {
        _shaderSet = shaderSet;
        refreshShaderReflection();
        return *this;
    }

    ShaderID getShader(ShaderStage stage) const
    {
        return _shaderSet.getShader(stage);
    }

    const MaterialShaderSet& getShaderSet() const
    {
        return _shaderSet;
    }

    uint32_t getVersion() const
    {
        return _version;
    }

    const rttr::variant* getParam(const std::string& name) const
    {
        auto iter = _paramMap.find(name);
        if (iter == _paramMap.end()) return nullptr;
        return &iter->second;
    }

    const MaterialParamMap& getParamMap() const
    {
        return _paramMap;
    }

    const MaterialParameterDeclaration* getParameterDeclaration(const std::string& name) const
    {
        auto iter = _parameterDeclarations.find(name);
        if (iter == _parameterDeclarations.end()) return nullptr;
        return &iter->second;
    }

    const MaterialParameterDeclarationMap& getParameterDeclarations() const
    {
        return _parameterDeclarations;
    }

    std::shared_ptr<MaterialInstance> createMaterialInstance();

    bool refreshShaderReflection();

    RTTR_ENABLE()

private:
    friend class MaterialInstance;

    void registerInstance(MaterialInstance* instance);
    void unregisterInstance(MaterialInstance* instance);
    void syncMaterialInstances();
    bool reflectDrawObjectDescriptorSet();

    std::string                      _name = "Default Material";
    MaterialShaderSet                _shaderSet;
    MaterialParamMap                 _paramMap;
    MaterialDescriptorDeclarationMap _descriptorDeclarations;
    MaterialParameterDeclarationMap  _parameterDeclarations;
    std::vector<MaterialInstance*>   _instances;
    uint32_t                         _version = 0;
};

class MaterialInstance
{
public:
    MaterialInstance() = default;
    explicit MaterialInstance(Material* material);
    ~MaterialInstance();

    Material* getMaterial()
    {
        return _material;
    }

    const Material* getMaterial() const
    {
        return _material;
    }

    uint32_t getSourceMaterialVersion() const
    {
        return _sourceMaterialVersion;
    }

    bool isOutOfDate() const;

    MaterialInstance& syncFromMaterial(bool preserveOverrides = true);

    MaterialInstance& setRenderState(const MaterialRenderState& renderState)
    {
        _renderState = renderState;
        return *this;
    }

    MaterialRenderState& getRenderState()
    {
        return _renderState;
    }

    const MaterialRenderState& getRenderState() const
    {
        return _renderState;
    }

    MaterialParamOverrideMap& getOverrideParamMap()
    {
        return _overrideParamMap;
    }

    const MaterialParamOverrideMap& getOverrideParamMap() const
    {
        return _overrideParamMap;
    }

    MaterialParamOverrideMap& getParamMap()
    {
        return _overrideParamMap;
    }

    const MaterialParamOverrideMap& getParamMap() const
    {
        return _overrideParamMap;
    }

    MaterialInstance& setParam(const std::string& name, const rttr::variant& value)
    {
        _overrideParamMap[name] = value;
        return *this;
    }

    template <typename T>
    MaterialInstance& setParam(const std::string& name, const T& value)
    {
        return setParam(name, rttr::variant(value));
    }

    MaterialInstance& setTexture(const std::string& name, Texture* texture)
    {
        _overrideParamMap[name] = texture;
        return *this;
    }

    MaterialInstance& setTexture(const std::string& name, Texture& texture)
    {
        return setTexture(name, &texture);
    }

    MaterialInstance& setBuffer(const std::string& name, Buffer* buffer)
    {
        _overrideParamMap[name] = buffer;
        return *this;
    }

    MaterialInstance& setBuffer(const std::string& name, Buffer& buffer)
    {
        return setBuffer(name, &buffer);
    }

    MaterialInstance& setParam(const std::string& name, Texture* texture)
    {
        return setTexture(name, texture);
    }

    MaterialInstance& setParam(const std::string& name, Texture& texture)
    {
        return setTexture(name, texture);
    }

    MaterialInstance& setParam(const std::string& name, Buffer* buffer)
    {
        return setBuffer(name, buffer);
    }

    MaterialInstance& setParam(const std::string& name, Buffer& buffer)
    {
        return setBuffer(name, buffer);
    }

    rttr::variant* getOverrideParam(const std::string& name)
    {
        auto iter = _overrideParamMap.find(name);
        if (iter == _overrideParamMap.end()) return nullptr;
        return &iter->second;
    }

    const rttr::variant* getOverrideParam(const std::string& name) const
    {
        auto iter = _overrideParamMap.find(name);
        if (iter == _overrideParamMap.end()) return nullptr;
        return &iter->second;
    }

    rttr::variant* getParam(const std::string& name)
    {
        return getOverrideParam(name);
    }

    const rttr::variant* getParam(const std::string& name) const;

    bool hasParam(const std::string& name) const;

    bool hasParamOverride(const std::string& name) const;

    bool clearParamOverride(const std::string& name);

    bool resetParamToDefault(const std::string& name);

    bool removeParam(const std::string& name);

    const MaterialParameterDeclaration* getParameterDeclaration(const std::string& name) const
    {
        auto iter = _parameterDeclarations.find(name);
        if (iter == _parameterDeclarations.end()) return nullptr;
        return &iter->second;
    }

    MaterialParamValidationResult validateOverrideParamMap() const;

    MaterialInstance& setRasterMode(MaterialRasterMode mode)
    {
        _renderState.rasterMode = mode;
        return *this;
    }

    MaterialRasterMode getRasterMode() const
    {
        return _renderState.rasterMode;
    }

    MaterialInstance& setBlendMode(MaterialBlendMode mode)
    {
        _renderState.blendMode = mode;
        return *this;
    }

    MaterialBlendMode getBlendMode() const
    {
        return _renderState.blendMode;
    }

    MaterialInstance& setDepthTestEnable(bool enable)
    {
        _renderState.depthTestEnable = enable;
        return *this;
    }

    bool isDepthTestEnabled() const
    {
        return _renderState.depthTestEnable;
    }

    MaterialInstance& setDepthWriteEnable(bool enable)
    {
        _renderState.depthWriteEnable = enable;
        return *this;
    }

    bool isDepthWriteEnabled() const
    {
        return _renderState.depthWriteEnable;
    }

    MaterialInstance& setDepthCompareMode(MaterialDepthCompareMode mode)
    {
        _renderState.depthCompareMode = mode;
        return *this;
    }

    MaterialDepthCompareMode getDepthCompareMode() const
    {
        return _renderState.depthCompareMode;
    }

    MaterialInstance& setColorAttachmentCount(uint32_t count)
    {
        _renderState.colorAttachmentCount = count;
        return *this;
    }

    uint32_t getColorAttachmentCount() const
    {
        return _renderState.colorAttachmentCount;
    }

    MaterialInstance& setColorWriteMask(VkColorComponentFlags mask)
    {
        _renderState.colorWriteMask = mask;
        return *this;
    }

    VkColorComponentFlags getColorWriteMask() const
    {
        return _renderState.colorWriteMask;
    }

    void applyToPSOState(PSOState& psoState) const
    {
        applyRasterState(psoState);
        applyDepthState(psoState);
        applyBlendState(psoState);
        psoState.dirtyFlag = true;
    }

    bool buildDrawObjectDescriptorBindings(DescriptorSetBindings& descriptorBindings, bool requireBoundResources = false) const;

    RTTR_ENABLE()

private:
    friend class Material;

    static VkCompareOp toVkCompareOp(MaterialDepthCompareMode mode)
    {
        switch (mode)
        {
            case MaterialDepthCompareMode::eLess:
                return VK_COMPARE_OP_LESS;
            case MaterialDepthCompareMode::eLessEqual:
                return VK_COMPARE_OP_LESS_OR_EQUAL;
            case MaterialDepthCompareMode::eEqual:
                return VK_COMPARE_OP_EQUAL;
            case MaterialDepthCompareMode::eGreater:
                return VK_COMPARE_OP_GREATER;
            case MaterialDepthCompareMode::eGreaterEqual:
                return VK_COMPARE_OP_GREATER_OR_EQUAL;
            case MaterialDepthCompareMode::eAlways:
                return VK_COMPARE_OP_ALWAYS;
            default:
                return VK_COMPARE_OP_LESS_OR_EQUAL;
        }
    }

    static VkColorBlendEquationEXT makeBlendEquation(MaterialBlendMode mode)
    {
        VkColorBlendEquationEXT equation{
            .srcColorBlendFactor = VK_BLEND_FACTOR_ONE,
            .dstColorBlendFactor = VK_BLEND_FACTOR_ZERO,
            .colorBlendOp        = VK_BLEND_OP_ADD,
            .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
            .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
            .alphaBlendOp        = VK_BLEND_OP_ADD,
        };

        switch (mode)
        {
            case MaterialBlendMode::eAlphaBlend:
                equation.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
                equation.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
                equation.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
                equation.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
                break;
            case MaterialBlendMode::ePremultipliedAlpha:
                equation.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
                equation.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
                equation.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
                equation.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
                break;
            case MaterialBlendMode::eAdditive:
                equation.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
                equation.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
                equation.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
                equation.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
                break;
            case MaterialBlendMode::eMultiply:
                equation.srcColorBlendFactor = VK_BLEND_FACTOR_DST_COLOR;
                equation.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
                equation.srcAlphaBlendFactor = VK_BLEND_FACTOR_DST_ALPHA;
                equation.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
                break;
            default:
                break;
        }

        return equation;
    }

    static bool isBlendEnabled(MaterialBlendMode mode)
    {
        return mode != MaterialBlendMode::eOpaque && mode != MaterialBlendMode::eMasked;
    }

    void applyRasterState(PSOState& psoState) const
    {
        psoState.inputAssemblyState.topology               = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        psoState.inputAssemblyState.primitiveRestartEnable = VK_FALSE;
        psoState.rasterizationState.frontFace              = VK_FRONT_FACE_COUNTER_CLOCKWISE;

        switch (_renderState.rasterMode)
        {
            case MaterialRasterMode::eTwoSidedTriangle:
                psoState.rasterizationState.polygonMode = VK_POLYGON_MODE_FILL;
                psoState.rasterizationState.cullMode    = VK_CULL_MODE_NONE;
                psoState.rasterizationState.lineWidth   = 1.0F;
                break;
            case MaterialRasterMode::eWireframe:
                psoState.rasterizationState.polygonMode = VK_POLYGON_MODE_LINE;
                psoState.rasterizationState.cullMode    = VK_CULL_MODE_NONE;
                psoState.rasterizationState.lineWidth   = 2.0F;
                break;
            case MaterialRasterMode::eCulledWireframe:
                psoState.rasterizationState.polygonMode = VK_POLYGON_MODE_LINE;
                psoState.rasterizationState.cullMode    = VK_CULL_MODE_BACK_BIT;
                psoState.rasterizationState.lineWidth   = 2.0F;
                break;
            case MaterialRasterMode::eTriangle:
            default:
                psoState.rasterizationState.polygonMode = VK_POLYGON_MODE_FILL;
                psoState.rasterizationState.cullMode    = VK_CULL_MODE_BACK_BIT;
                psoState.rasterizationState.lineWidth   = 1.0F;
                break;
        }
    }

    void applyDepthState(PSOState& psoState) const
    {
        psoState.depthStencilState.depthTestEnable  = _renderState.depthTestEnable ? VK_TRUE : VK_FALSE;
        psoState.depthStencilState.depthWriteEnable = _renderState.depthWriteEnable ? VK_TRUE : VK_FALSE;
        psoState.depthStencilState.depthCompareOp   = toVkCompareOp(_renderState.depthCompareMode);
    }

    void applyBlendState(PSOState& psoState) const
    {
        uint32_t attachmentCount = _renderState.colorAttachmentCount;
        if (attachmentCount == 0) attachmentCount = 1;

        psoState.colorBlendEnables.resize(attachmentCount);
        psoState.colorWriteMasks.resize(attachmentCount);
        psoState.colorBlendEquations.resize(attachmentCount);

        const VkBool32                blendEnable = isBlendEnabled(_renderState.blendMode) ? VK_TRUE : VK_FALSE;
        const VkColorBlendEquationEXT equation    = makeBlendEquation(_renderState.blendMode);

        for (uint32_t index = 0; index < attachmentCount; ++index)
        {
            psoState.colorBlendEnables[index]   = blendEnable;
            psoState.colorWriteMasks[index]     = _renderState.colorWriteMask;
            psoState.colorBlendEquations[index] = equation;
        }
    }

private:
    Material*                       _material = nullptr;
    uint32_t                        _sourceMaterialVersion = 0;
    MaterialParamOverrideMap        _overrideParamMap;
    MaterialDescriptorBindingMap    _descriptorBindings;
    MaterialParameterDeclarationMap _parameterDeclarations;
    MaterialRenderState             _renderState;
};

inline std::shared_ptr<MaterialInstance> Material::createMaterialInstance()
{
    return std::make_shared<MaterialInstance>(this);
}

} // namespace Play

#endif // MATERIAL_H
