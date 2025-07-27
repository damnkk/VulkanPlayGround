#ifndef RDG_SHADER_PARAMETERS_HPP
#define RDG_SHADER_PARAMETERS_HPP

#include <vector>
#include "RDGResources.h"
namespace Play::RDG {
class RDGResourceState{
 public:
    enum class AccessType{
        eReadOnly, //uniform buffer, sampled texture
        eWriteOnly, //most used for color attachments, depth attachments, msaa attachments, shading rate resolve attachments
        eReadWrite, //storage buffer, storage texture
        eCount
    };
    enum class AccessStage{
        eVertexShader = 1 << 0,
        eFragmentShader = 1 << 1,
        eComputeShader = 1 << 2,
        eRayTracingShader = 1 << 3,
        eAllGraphics = eVertexShader | eFragmentShader | eComputeShader,
        eAllCompute = eComputeShader | eRayTracingShader,
        eAll = eAllGraphics | eAllCompute
    };

    enum class ResourceType{
        eTexture,
        eBuffer,
        eCount
    };

    ResourceType _resourceType = ResourceType::eCount;
    AccessType _accessType;
    AccessStage _accessStage;
    std::vector<void*> _resources;
};

struct RDGShaderParameters{
    template<typename T>
    bool addUAVResource(T* resource, RDGResourceState::AccessStage accessStage = RDGResourceState::AccessStage::eAll);
    template<typename T>
    bool addUAVBindlessArray(std::vector<T*> resources, RDGResourceState::AccessStage accessStage = RDGResourceState::AccessStage::eAll);
    template<typename T>
    bool addSRVResource(T* resource, RDGResourceState::AccessStage accessStage = RDGResourceState::AccessStage::eAll);
    template<typename T>
    bool addSRVBindlessArray(std::vector<T*> resources, RDGResourceState::AccessStage accessStage = RDGResourceState::AccessStage::eAll);
    template<typename T>
    bool addResource(T* resource, RDGResourceState::AccessType accessType, RDGResourceState::AccessStage accessStage = RDGResourceState::AccessStage::eAll);
    template<typename T>
    bool addResourceBindlessArray(std::vector<T*> resources, RDGResourceState::AccessType accessType, RDGResourceState::AccessStage accessStage = RDGResourceState::AccessStage::eAll);
    std::array<std::vector<RDGResourceState>, static_cast<size_t>(RDGResourceState::AccessType::eCount)> _resources;
};

struct RDGRTState{
    enum class LoadType{
        eLoad,
        eDontCare,
        eClear
    } loadType = LoadType::eLoad;

    enum class StoreType{
        eStore,
        eDontCare,
    } storeType = StoreType::eStore;
    RDGTexture* rtTexture = nullptr;
    RDGTexture* resolveTexture = nullptr;
};

template<typename T>
bool RDGShaderParameters::addResource(T* resource, RDGResourceState::AccessType accessType, RDGResourceState::AccessStage accessStage) {
    if (!resource) return false;
    RDGResourceState state;
    state._accessType = accessType;
    state._accessStage = accessStage;
    if constexpr(std::is_same<T,RDGTexture>::value) {
        state._resourceType = RDGResourceState::ResourceType::eTexture;
    } else if constexpr(std::is_same<T,RDGBuffer>::value) {
        state._resourceType = RDGResourceState::ResourceType::eBuffer;
    } else {
        return false; // Unsupported resource type
    }
    state._resources.push_back(resource);
    _resources[static_cast<size_t>(accessType)].push_back(state);
    return true;
}

template<typename T>
bool RDGShaderParameters::addResourceBindlessArray(std::vector<T*> resources, RDGResourceState::AccessType accessType, RDGResourceState::AccessStage accessStage) {
    if (resources.empty()) return false;
    RDGResourceState state;
    state._accessType = accessType;
    state._accessStage = accessStage;
    if constexpr(std::is_same<T,RDGTexture>::value) {
        state._resourceType = RDGResourceState::ResourceType::eTexture;
    } else if constexpr(std::is_same<T,RDGBuffer>::value) {
        state._resourceType = RDGResourceState::ResourceType::eBuffer;
    } else {
        return false; // Unsupported resource type
    }
    state._resources = std::move(resources);
    _resources[static_cast<size_t>(accessType)].push_back(state);
    return true;
}

template<typename T>
bool RDGShaderParameters::addUAVResource(T* resource, RDGResourceState::AccessStage accessStage) {
    return addResource(resource, RDGResourceState::AccessType::eReadWrite, accessStage);
}

template<typename T>
bool RDGShaderParameters::addUAVBindlessArray(std::vector<T*> resources, RDGResourceState::AccessStage accessStage) {
    return addResourceBindlessArray(resources, RDGResourceState::AccessType::eReadWrite, accessStage);
}

template<typename T>
bool RDGShaderParameters::addSRVResource(T* resource, RDGResourceState::AccessStage accessStage) {
    return addResource(resource, RDGResourceState::AccessType::eReadOnly, accessStage);
}

template<typename T>
bool RDGShaderParameters::addSRVBindlessArray(std::vector<T*> resources, RDGResourceState::AccessStage accessStage) {
    return addResourceBindlessArray(resources, RDGResourceState::AccessType::eReadOnly, accessStage);   
}




}// namespace Play::RDG

#endif // RDG_SHADER_PARAMETERS_HPP