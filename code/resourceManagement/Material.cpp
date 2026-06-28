#include "Material.h"
#include "PlayProgram.h"

namespace Play
{
namespace
{
constexpr DescriptorEnum MATERIAL_DESCRIPTOR_SET = DescriptorEnum::eDrawObjectDescriptorSet;
uint32_t getDescriptorCount(const SpvReflectDescriptorBinding& binding)
{
    if (binding.array.dims_count == 0)
    {
        return binding.count == 0 ? 1 : binding.count;
    }

    uint32_t count = 1;
    for (uint32_t index = 0; index < binding.array.dims_count; ++index)
    {
        if (binding.array.dims[index] == 0) continue;
        count *= binding.array.dims[index];
    }
    return count;
}

std::string getDescriptorName(const SpvReflectDescriptorBinding& binding)
{
    if (binding.name && binding.name[0] != '\0')
    {
        return binding.name;
    }

    return "set" + std::to_string(binding.set) + "_binding" + std::to_string(binding.binding);
}

std::string findDescriptorDeclarationName(const MaterialDescriptorDeclarationMap& declarations, uint32_t bindingIdx)
{
    for (const auto& pair : declarations)
    {
        if (pair.second.bindingIdx == bindingIdx)
        {
            return pair.first;
        }
    }

    return {};
}

MaterialParameterKind getParameterKind(const SpvReflectBlockVariable& variable)
{
    if (!variable.type_description)
    {
        return MaterialParameterKind::eUnknown;
    }

    const SpvReflectTypeFlags typeFlags = variable.type_description->type_flags;
    if (typeFlags & SPV_REFLECT_TYPE_FLAG_MATRIX)
    {
        return MaterialParameterKind::eMatrix;
    }
    if (typeFlags & SPV_REFLECT_TYPE_FLAG_VECTOR)
    {
        return MaterialParameterKind::eVector;
    }
    if (typeFlags & SPV_REFLECT_TYPE_FLAG_BOOL || typeFlags & SPV_REFLECT_TYPE_FLAG_INT || typeFlags & SPV_REFLECT_TYPE_FLAG_FLOAT)
    {
        return MaterialParameterKind::eScalar;
    }

    return MaterialParameterKind::eUnknown;
}

void collectBlockParameters(MaterialParameterDeclarationMap& declarations, MaterialParamMap& defaultParams,
                            const MaterialDescriptorDeclaration& descriptor, const SpvReflectBlockVariable& variable,
                            const std::string& prefix)
{
    for (uint32_t index = 0; index < variable.member_count; ++index)
    {
        const SpvReflectBlockVariable& member     = variable.members[index];
        const std::string              memberName = member.name && member.name[0] != '\0' ? member.name : "member" + std::to_string(index);
        const std::string              paramName  = prefix.empty() ? memberName : prefix + "." + memberName;

        if (member.member_count > 0)
        {
            collectBlockParameters(declarations, defaultParams, descriptor, member, paramName);
            continue;
        }

        MaterialParameterDeclaration declaration;
        declaration.name           = paramName;
        declaration.kind           = getParameterKind(member);
        declaration.descriptorName = descriptor.name;
        declaration.byteOffset     = member.absolute_offset;
        declaration.byteSize       = member.size;
        declarations[declaration.name] = declaration;

        if (defaultParams.find(declaration.name) == defaultParams.end())
        {
            defaultParams[declaration.name] = rttr::variant();
        }
    }
}

MaterialParameterKind getDescriptorParameterKind(VkDescriptorType descriptorType)
{
    switch (descriptorType)
    {
        case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
        case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
        case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
        case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
            return MaterialParameterKind::eBuffer;
        case VK_DESCRIPTOR_TYPE_SAMPLER:
            return MaterialParameterKind::eSampler;
        case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
        case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
        case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
        case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
            return MaterialParameterKind::eTexture;
        default:
            return MaterialParameterKind::eUnknown;
    }
}

bool isScalarValue(const rttr::variant& value)
{
    return value.is_type<bool>() || value.is_type<int>() || value.is_type<uint32_t>() || value.is_type<int64_t>() ||
           value.is_type<uint64_t>() || value.is_type<float>() || value.is_type<double>();
}

bool isBufferResourceValue(const rttr::variant& value)
{
    return value.is_type<Buffer*>() || value.is_type<std::vector<Buffer*>>();
}

bool isImageResourceValue(const rttr::variant& value)
{
    return value.is_type<Texture*>() || value.is_type<std::vector<Texture*>>();
}

bool isParameterValueCompatible(const MaterialParameterDeclaration& declaration, const rttr::variant& value,
                                const rttr::variant* materialValue)
{
    if (!value.is_valid()) return true;

    if (materialValue && materialValue->is_valid())
    {
        return value.get_type() == materialValue->get_type();
    }

    switch (declaration.kind)
    {
        case MaterialParameterKind::eScalar:
            return isScalarValue(value);
        case MaterialParameterKind::eTexture:
        case MaterialParameterKind::eSampler:
            return isImageResourceValue(value);
        case MaterialParameterKind::eBuffer:
            return isBufferResourceValue(value);
        case MaterialParameterKind::eVector:
        case MaterialParameterKind::eMatrix:
            return true;
        default:
            return false;
    }
}

bool applyBufferResource(DescriptorSetBindings& descriptorBindings, const MaterialDescriptorDeclaration& declaration, const rttr::variant& resource)
{
    if (resource.is_type<Buffer*>())
    {
        Buffer* buffer = resource.get_value<Buffer*>();
        if (!buffer) return false;
        descriptorBindings.setDescInfo(declaration.bindingIdx, *buffer);
        return true;
    }
    if (resource.is_type<std::vector<Buffer*>>())
    {
        const std::vector<Buffer*> values = resource.get_value<std::vector<Buffer*>>();
        if (values.size() < declaration.descriptorCount) return false;

        std::vector<VkDescriptorBufferInfo> bufferInfos(declaration.descriptorCount);
        for (uint32_t index = 0; index < declaration.descriptorCount; ++index)
        {
            if (!values[index]) return false;
            bufferInfos[index].buffer = values[index]->buffer;
            bufferInfos[index].offset = 0;
            bufferInfos[index].range  = values[index]->BufferRange();
        }
        descriptorBindings.setDescInfo(declaration.bindingIdx, bufferInfos.data(), declaration.descriptorCount);
        return true;
    }

    return false;
}

bool applyImageResource(DescriptorSetBindings& descriptorBindings, const MaterialDescriptorDeclaration& declaration, const rttr::variant& resource)
{
    if (resource.is_type<Texture*>())
    {
        Texture* texture = resource.get_value<Texture*>();
        if (!texture) return false;
        descriptorBindings.setDescInfo(declaration.bindingIdx, *texture);
        return true;
    }
    if (resource.is_type<std::vector<Texture*>>())
    {
        const std::vector<Texture*> values = resource.get_value<std::vector<Texture*>>();
        if (values.size() < declaration.descriptorCount) return false;

        std::vector<VkDescriptorImageInfo> imageInfos(declaration.descriptorCount);
        for (uint32_t index = 0; index < declaration.descriptorCount; ++index)
        {
            if (!values[index]) return false;
            imageInfos[index] = values[index]->descriptor;
        }
        descriptorBindings.setDescInfo(declaration.bindingIdx, imageInfos.data(), declaration.descriptorCount);
        return true;
    }

    return false;
}

bool applyDescriptorResource(DescriptorSetBindings& descriptorBindings, const MaterialDescriptorDeclaration& declaration,
                             const rttr::variant& resource)
{
    switch (declaration.descriptorType)
    {
        case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
        case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
            return applyBufferResource(descriptorBindings, declaration, resource);
        case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
        case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
        case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
        case VK_DESCRIPTOR_TYPE_SAMPLER:
            return applyImageResource(descriptorBindings, declaration, resource);
        default:
            return false;
    }
}
} // namespace

MaterialInstance::MaterialInstance(Material* material) : _material(material)
{
    if (_material)
    {
        _material->registerInstance(this);
    }
    syncFromMaterial(false);
}

MaterialInstance::~MaterialInstance()
{
    if (_material)
    {
        _material->unregisterInstance(this);
    }
}

const rttr::variant* MaterialInstance::getParam(const std::string& name) const
{
    auto overrideIter = _overrideParamMap.find(name);
    if (overrideIter != _overrideParamMap.end()) return &overrideIter->second;
    if (!_material) return nullptr;
    return _material->getParam(name);
}

bool MaterialInstance::hasParam(const std::string& name) const
{
    return getParam(name) != nullptr;
}

bool MaterialInstance::hasParamOverride(const std::string& name) const
{
    return _overrideParamMap.find(name) != _overrideParamMap.end();
}

bool MaterialInstance::clearParamOverride(const std::string& name)
{
    return _overrideParamMap.erase(name) > 0;
}

bool MaterialInstance::resetParamToDefault(const std::string& name)
{
    if (!_material || !_material->getParam(name)) return false;
    _overrideParamMap.erase(name);
    return true;
}

bool MaterialInstance::removeParam(const std::string& name)
{
    return clearParamOverride(name);
}

bool MaterialInstance::isOutOfDate() const
{
    return _material && _sourceMaterialVersion != _material->getVersion();
}

MaterialInstance& MaterialInstance::syncFromMaterial(bool preserveOverrides)
{
    if (!_material) return *this;

    const MaterialDescriptorBindingMap oldDescriptorBindings = _descriptorBindings;

    _sourceMaterialVersion = _material->getVersion();
    if (!preserveOverrides)
    {
        _overrideParamMap.clear();
    }
    _parameterDeclarations = _material->_parameterDeclarations;
    _descriptorBindings.clear();

    for (const auto& pair : _material->_descriptorDeclarations)
    {
        MaterialDescriptorBinding binding;
        binding.declaration = pair.second;

        if (preserveOverrides)
        {
            auto oldBinding = oldDescriptorBindings.find(pair.first);
            if (oldBinding != oldDescriptorBindings.end())
            {
                binding.resource           = oldBinding->second.resource;
                binding.resourceOverridden = oldBinding->second.resourceOverridden;
            }
        }

        _descriptorBindings[pair.first] = binding;
    }

    return *this;
}

MaterialParamValidationResult MaterialInstance::validateOverrideParamMap() const
{
    MaterialParamValidationResult result;

    for (const auto& pair : _overrideParamMap)
    {
        const MaterialParameterDeclaration* declaration = getParameterDeclaration(pair.first);
        if (!declaration)
        {
            ++result.ignoredParamCount;
            continue;
        }

        ++result.checkedParamCount;
        const rttr::variant* materialValue = _material ? _material->getParam(pair.first) : nullptr;
        if (!isParameterValueCompatible(*declaration, pair.second, materialValue))
        {
            result.valid = false;
            result.typeMismatchParams.push_back(pair.first);
        }
    }

    return result;
}

DescriptorSetBindings& MaterialInstance::getDescriptorSetState(bool requireBoundResources)
{
    if (isOutOfDate())
    {
        syncFromMaterial(true);
    }
    if (_descriptorSetStateVersion != _sourceMaterialVersion)
    {
        _descriptorSetState.reset(MATERIAL_DESCRIPTOR_SET);
        _descriptorSetStateVersion = _sourceMaterialVersion;
    }
    buildDescriptorSetState(_descriptorSetState, requireBoundResources);
    return _descriptorSetState;
}

bool MaterialInstance::buildDescriptorSetState(DescriptorSetBindings& descriptorBindings, bool requireBoundResources) const
{
    descriptorBindings.setDescriptorSetSlot(MATERIAL_DESCRIPTOR_SET);
    return buildDrawObjectDescriptorBindings(descriptorBindings, requireBoundResources);
}

bool MaterialInstance::buildDrawObjectDescriptorBindings(DescriptorSetBindings& descriptorBindings, bool requireBoundResources) const
{
    bool supportedLayout     = true;
    bool resourcesComplete   = true;
    bool boundResourcesValid = true;

    for (const auto& pair : _descriptorBindings)
    {
        const MaterialDescriptorDeclaration& declaration = pair.second.declaration;
        if (declaration.descriptorType == VK_DESCRIPTOR_TYPE_MAX_ENUM)
        {
            supportedLayout = false;
            continue;
        }

        descriptorBindings.addBinding(declaration.bindingIdx, declaration.descriptorCount, declaration.descriptorType,
                                      declaration.shaderStageFlags);
    }

    descriptorBindings.finalizeLayout();

    for (const auto& pair : _descriptorBindings)
    {
        const MaterialDescriptorBinding& binding     = pair.second;
        const MaterialDescriptorDeclaration& declaration = binding.declaration;
        if (declaration.descriptorType == VK_DESCRIPTOR_TYPE_MAX_ENUM) continue;

        const rttr::variant* resource = getParam(declaration.name);
        if ((!resource || !resource->is_valid()) && binding.isBound())
        {
            resource = &binding.resource;
        }

        if (!resource || !resource->is_valid())
        {
            resourcesComplete = false;
            continue;
        }

        if (!applyDescriptorResource(descriptorBindings, declaration, *resource))
        {
            boundResourcesValid = false;
        }
    }

    return supportedLayout && boundResourcesValid && (!requireBoundResources || resourcesComplete);
}

Material::~Material()
{
    for (MaterialInstance* instance : _instances)
    {
        if (instance && instance->_material == this)
        {
            instance->_material              = nullptr;
            instance->_sourceMaterialVersion = 0;
        }
    }
    _instances.clear();
}

void Material::registerInstance(MaterialInstance* instance)
{
    if (!instance) return;

    for (MaterialInstance* registeredInstance : _instances)
    {
        if (registeredInstance == instance) return;
    }

    _instances.push_back(instance);
}

void Material::unregisterInstance(MaterialInstance* instance)
{
    for (auto iter = _instances.begin(); iter != _instances.end();)
    {
        if (*iter == instance)
        {
            iter = _instances.erase(iter);
            continue;
        }
        ++iter;
    }
}

void Material::syncMaterialInstances()
{
    for (auto iter = _instances.begin(); iter != _instances.end();)
    {
        MaterialInstance* instance = *iter;
        if (!instance || instance->_material != this)
        {
            iter = _instances.erase(iter);
            continue;
        }

        instance->syncFromMaterial(true);
        ++iter;
    }
}

bool Material::refreshShaderReflection()
{
    const bool reflectedAnyBinding = reflectDrawObjectDescriptorSet();
    ++_version;
    syncMaterialInstances();
    return reflectedAnyBinding;
}

bool Material::reflectDrawObjectDescriptorSet()
{
    const uint32_t targetSet = static_cast<uint32_t>(MATERIAL_DESCRIPTOR_SET);
    _descriptorDeclarations.clear();
    _parameterDeclarations.clear();
    _paramMap.clear();

    bool reflectedAnyBinding = false;

    for (uint32_t shaderIndex = 0; shaderIndex < MATERIAL_SHADER_STAGE_COUNT; ++shaderIndex)
    {
        const ShaderID shaderID = _shaderSet.shaderIDs[shaderIndex];
        if (shaderID == MATERIAL_INVALID_SHADER_ID) continue;

        const ShaderModule* shaderModule = ShaderManager::Instance().getShaderById(shaderID);
        if (!shaderModule || shaderModule->_spvCode.empty()) continue;

        SpvReflectShaderModule reflectModule = {};
        const size_t spirvSize = shaderModule->_spvCode.size() * sizeof(uint32_t);
        SpvReflectResult result = spvReflectCreateShaderModule(spirvSize, shaderModule->_spvCode.data(), &reflectModule);
        if (result != SPV_REFLECT_RESULT_SUCCESS) continue;

        uint32_t bindingCount = 0;
        result = spvReflectEnumerateDescriptorBindings(&reflectModule, &bindingCount, nullptr);
        if (result == SPV_REFLECT_RESULT_SUCCESS && bindingCount > 0)
        {
            std::vector<SpvReflectDescriptorBinding*> bindings(bindingCount);
            result = spvReflectEnumerateDescriptorBindings(&reflectModule, &bindingCount, bindings.data());
            if (result == SPV_REFLECT_RESULT_SUCCESS)
            {
                for (uint32_t bindingIndex = 0; bindingIndex < bindingCount; ++bindingIndex)
                {
                    const SpvReflectDescriptorBinding* reflectedBinding = bindings[bindingIndex];
                    if (!reflectedBinding || reflectedBinding->set != targetSet) continue;
                    // Reflect the declared material interface, independent of whether entry-point code currently reads it.

                    const std::string reflectedName = getDescriptorName(*reflectedBinding);
                    std::string declarationName =
                        findDescriptorDeclarationName(_descriptorDeclarations, reflectedBinding->binding);
                    if (declarationName.empty())
                    {
                        declarationName = reflectedName;
                    }

                    const VkDescriptorType descriptorType = spvToDescriptorType(reflectedBinding->descriptor_type);
                    if (descriptorType == VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR) continue;

                    MaterialDescriptorDeclaration& declaration = _descriptorDeclarations[declarationName];
                    if (declaration.name.empty())
                    {
                        declaration.name             = declarationName;
                        declaration.bindingIdx       = reflectedBinding->binding;
                        declaration.descriptorCount  = getDescriptorCount(*reflectedBinding);
                        declaration.descriptorType   = descriptorType;
                        declaration.shaderStageFlags = 0;
                    }
                    else if (declaration.descriptorCount < getDescriptorCount(*reflectedBinding))
                    {
                        declaration.descriptorCount = getDescriptorCount(*reflectedBinding);
                    }

                    declaration.shaderStageFlags |= static_cast<VkShaderStageFlags>(reflectModule.shader_stage);

                    const MaterialParameterKind parameterKind = getDescriptorParameterKind(declaration.descriptorType);
                    if (parameterKind != MaterialParameterKind::eUnknown)
                    {
                        MaterialParameterDeclaration parameterDeclaration;
                        parameterDeclaration.name           = declaration.name;
                        parameterDeclaration.kind           = parameterKind;
                        parameterDeclaration.descriptorName = declaration.name;
                        _parameterDeclarations[parameterDeclaration.name] = parameterDeclaration;
                        if (_paramMap.find(parameterDeclaration.name) == _paramMap.end())
                        {
                            _paramMap[parameterDeclaration.name] = rttr::variant();
                        }
                    }

                    if (reflectedBinding->block.member_count > 0)
                    {
                        collectBlockParameters(_parameterDeclarations, _paramMap, declaration, reflectedBinding->block, declaration.name);
                    }

                    reflectedAnyBinding = true;
                }
            }
        }

        spvReflectDestroyShaderModule(&reflectModule);
    }

    return reflectedAnyBinding;
}
} // namespace Play
