
#include "PlayProgram.h"
#include "nvh/nvprint.hpp"
namespace Play{
DescriptorSetManager::DescriptorSetManager(VkDevice device) :
nvvk::TDescriptorSetContainer<MAX_DESCRIPTOR_SETS::value,1>(device){

}

DescriptorSetManager::~DescriptorSetManager(){
    deinit();
}
DescriptorSetManager& DescriptorSetManager::addBinding(const BindInfo& bindingInfo){
    _recordState = false;
    for(int i = 0;i<_bindingInfos.size();i++){
        if(_bindingInfos[i].setIdx == bindingInfo.setIdx && _bindingInfos[i].bindingIdx == bindingInfo.bindingIdx){
            if(_bindingInfos[i].descriptorType!= bindingInfo.descriptorType){
                LOGE("Descriptor type mismatch");
                return *this;
            }
            _bindingInfos[i].descriptorCount += bindingInfo.descriptorCount;
            _bindingInfos[i].pipelineStageFlags |= bindingInfo.pipelineStageFlags;
            return *this;
        }
    }
    _bindingInfos.push_back(bindingInfo);
    return *this;
}

DescriptorSetManager& DescriptorSetManager::addBinding(uint32_t setIdx, uint32_t bindingIdx, uint32_t descriptorCount, VkDescriptorType descriptorType, VkPipelineStageFlags2 pipelineStageFlags)
{
    _recordState = false;
    BindInfo bindingInfo = { setIdx, bindingIdx, descriptorCount, descriptorType, pipelineStageFlags };
    return addBinding(bindingInfo);
}

DescriptorSetManager& DescriptorSetManager::initLayout(uint32_t setIdx){
    _recordState = false;
    if(setIdx < MAX_DESCRIPTOR_SETS::value){
        at(setIdx).initLayout();
    }
    return *this;
}

DescriptorSetManager& DescriptorSetManager::addConstantRange(uint32_t size,uint32_t offset, VkPipelineStageFlags2 stage)
{
    _recordState = false;
    _constantRanges.emplace_back(stage, offset, size);
    return *this;
}

bool DescriptorSetManager::finish(){
    _recordState = true;
    deinitLayouts();
    for(size_t i = 0;i<this->_bindingInfos.size();++i){
        NV_ASSERT(_bindingInfos[i].setIdx < MAX_DESCRIPTOR_SETS::value);
        at(_bindingInfos[i].setIdx).addBinding(_bindingInfos[i].bindingIdx, _bindingInfos[i].descriptorType, _bindingInfos[i].descriptorCount, _bindingInfos[i].pipelineStageFlags);
    }
    initPipeLayout(0,_constantRanges.size(),_constantRanges.data());
    return _recordState;
}

VkPipelineLayout DescriptorSetManager::getPipelineLayout()const{
    if(!_recordState){
        LOGE("Pipeline layout is not created, check finish() is called");
        return VK_NULL_HANDLE;
    }
    return m_pipelayouts[0];
}


}// namespace Play