#ifndef RDGPASSES_HPP
#define RDGPASSES_HPP
#include <set>
#include <optional>
#include "PlayProgram.h"
#include "RDGResources.h"
#include "RDGPreDefine.h"
#include "utils.hpp"
namespace Play::RDG{
/*
当前实现了全新的RDG pass, RDGpass的构建交给上层逻辑pass的Build函数,pass本身只维护资源依赖逻辑,渲染逻辑以及相关的管线资源都交给上层逻辑pass管理,
由逻辑pass向底层RHI缓存进行申请
*/
using DescSetManagerRef = std::shared_ptr<Play::DescriptorSetManager>;
struct Scene;
class RenderDependencyGraph;

class RDGPass
{
public:
    // RDGPass只由graph本身分配
    RDGPass(std::optional<uint32_t> passID = std::nullopt,std::string name = "");
    RDGPass(const RDGPass&) = delete;
    virtual ~RDGPass() {};
    virtual void prepareDescriptors(){};
    virtual void preparePipeline(){};
    virtual void prepareRenderPass(){};
    void setDescSetManager(DescSetManagerRef descSetManager) {_passLayout = descSetManager;}
    void addRead( RDGTexture* texture, uint32_t setIdx, uint32_t bindIdx,uint32_t offset = 0);
    void addRead(RDGBuffer* buffer, uint32_t setIdx, uint32_t bindIdx, uint32_t offset = 0);
    void addReadWrite( RDGTexture* texture, uint32_t setIdx, uint32_t bindIdx, uint32_t offset = 0);
    void addReadWrite(RDGBuffer* buffer, uint32_t setIdx, uint32_t bindIdx, uint32_t offset = 0);
    struct DescriptorInfo{
        RDGResourceBase* resource = nullptr;
        uint32_t setIdx = 0;
        uint32_t bindIdx = 0;
        uint32_t offset = 0;
    };
    
protected:
    void updateDependency();
   
    friend class RenderDependencyGraph;
    bool                                 _isClipped = true;
    RenderDependencyGraph* _hostGraph = nullptr;
    DescSetManagerRef _passLayout;
    std::string                          _name;
    std::set<RDGPass*>                   _dependencies;
    std::optional<uint32_t>              _passID = std::nullopt;
    std::array<std::vector<DescriptorInfo>,static_cast<size_t>(AccessType::eCount)> _resourceMap;
};

/*
    So called _executeFunction is a lambda function that include the total render logic in this
   pass,is may include a scene or some geometry data, shader ref as parameters
*/

using FLambdaFunction = std::function<void()>;

class RDGRenderPass:public RDGPass{
public:
    RDGRenderPass(std::optional<uint32_t> paddID, std::string name = "");
    RDGRenderPass(std::optional<uint32_t> passID, std::string name="",FLambdaFunction&& executeFunction = nullptr)
        :RDGPass(passID, std::move(name)),_executeFunction(std::forward<FLambdaFunction>(executeFunction)) {
        _RTSlots.reserve(::Play::MAX_RT_NUM::value);
    }
    ~RDGRenderPass() override;
    //setting func
    void colorAttach(RDGRTState::LoadType loadType,
                    RDGRTState::StoreType storeType,
                    RDGTexture* texture,
                    RDGTexture* resolveTexture,
                    VkImageLayout initLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                    VkImageLayout finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    void colorAttach(RDGRTState& state);
    void depthStencilAttach(RDGRTState::LoadType loadType,
                            RDGRTState::StoreType storeType,
                            RDGTexture* texture,
                            VkImageLayout initLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                            VkImageLayout finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
    void executeFunction(FLambdaFunction&& func);
    void execute();
    virtual void prepareDescriptors()override;
    virtual void preparePipeline()override;
    virtual void prepareRenderPass()override;
    //RDGRenderPass中不必维护底层vkRenderPass/vkFrameBuffer,这些都在RHI层做池缓存,渲染时候直接get即可

protected:
    friend class RenderDependencyGraph;

private:
    FLambdaFunction _executeFunction;
    std::vector<RDGRTState> _RTSlots;
};

class RDGComputePass:public RDGPass{
public:
    RDGComputePass(std::optional<uint32_t>passID, std::string name = "");
    RDGComputePass(std::optional<uint32_t> passID, std::string name= "",FLambdaFunction&& executeFunction = nullptr)
        :RDGPass(passID, std::move(name)), _executeFunction(std::forward<FLambdaFunction>(executeFunction)) {
    }
    ~RDGComputePass() override;
    void execute();
    void executeFunction(FLambdaFunction&& func);
    virtual void prepareDescriptors()override;
    virtual void preparePipeline()override;
    virtual void prepareRenderPass()override;

private:
    friend class RenderDependencyGraph;
    FLambdaFunction _executeFunction;
};


} // namespace Play

#endif // RDGPASSES_HPP