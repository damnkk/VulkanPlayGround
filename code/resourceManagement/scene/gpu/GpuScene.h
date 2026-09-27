#ifndef GPU_SCENE_H
#define GPU_SCENE_H

#include "resourceManagement/Model.h"
#include "renderer/Drawable.h"

namespace Play
{
struct GpuSceneRange
{
    uint32_t offset   = 0;
    uint32_t capacity = 0;
};

class GpuScene
{
public:
    static constexpr uint32_t InvalidID       = ~0U;
    static constexpr uint32_t TextureBinding  = 3;
    static constexpr uint32_t TextureCapacity = 10240;

    struct DrawBucket
    {
        MaterialInstance*  materialInstanceRef = nullptr;
        GpuSceneDrawBucket gpu{};
    };

    // CPU-side mutations occur on the scene/render thread. Models stay registered
    // for this scene's lifetime; repeated registration returns the same ID.
    uint32_t                 registerModel(const ModelRef& model);
    const GpuSceneModelData& getModelData(uint32_t modelID) const;

    GpuSceneRange allocateInstances(uint32_t capacity);
    GpuSceneRange allocateDraws(uint32_t count);
    void          releaseInstances(GpuSceneRange range);
    void          releaseDraws(GpuSceneRange range);
    void          updateInstances(GpuSceneRange range, uint32_t first, uint32_t count, const GpuSceneInstanceData* data);
    void          updateDraws(GpuSceneRange range, uint32_t first, uint32_t count, const GpuSceneDrawData* data);

    // Persistent draw registration. Call only when membership or material changes.
    void updateDrawCommands(const std::vector<DrawCommand>& commands);
    // Call only after VulkanRuntime has waited for the current frame slot.
    void prepareFrame();
    void updateTextureDescriptors(VkDescriptorSet descriptorSet);
    // Upload dirty inputs and reset visibility counts on the GPU. Record on the
    // graphics queue through RDG, outside a render pass, before culling and scene consumers.
    // Finish scene mutations before prepareFrame; cmdUpload consumes those dirty inputs.
    void cmdUpload(VkCommandBuffer cmd);

    VkDeviceAddress                getRootAddress() const;
    const GpuSceneData&            getRootData() const;
    Buffer*                        getFrameBuffer() const;
    const std::vector<DrawBucket>& getDrawBuckets() const;
    VkDeviceSize                   getIndirectOffset(uint32_t bucketID) const;
    VkDeviceSize                   getCountOffset(uint32_t bucketID) const;
    // Caller binds the bucket's pipeline/material and pushes rootAddress + bucketID.
    void cmdDrawBucket(VkCommandBuffer cmd, uint32_t bucketID) const;
    // cmdUpload resets counts for the first culling pass. Reset again only when
    // starting another culling result; publish compute writes before drawing.
    void cmdResetDrawCounts(VkCommandBuffer cmd) const;
    void cmdBarrierAfterCulling(VkCommandBuffer cmd) const;

private:
    struct DirtyRange
    {
        uint32_t first = InvalidID;
        uint32_t end   = 0;
        void     include(uint32_t offset, uint32_t count);
    };
    struct FrameResources
    {
        RefPtr<Buffer>          buffer;
        GpuSceneData            root{};
        uint32_t                modelCapacity       = 0;
        uint32_t                drawCapacity        = 0;
        uint32_t                instanceCapacity    = 0;
        uint32_t                commandCapacity     = 0;
        uint32_t                bucketCapacity      = 0;
        VkDeviceSize            modelOffset         = 0;
        VkDeviceSize            drawOffset          = 0;
        VkDeviceSize            instanceOffset      = 0;
        VkDeviceSize            candidateOffset     = 0;
        VkDeviceSize            bucketOffset        = 0;
        VkDeviceSize            indirectOffset      = 0;
        VkDeviceSize            countOffset         = 0;
        VkDeviceSize            visibleDrawIDOffset = 0;
        DirtyRange              modelsDirty;
        DirtyRange              drawsDirty;
        DirtyRange              instancesDirty;
        bool                    snapshotDirty        = true;
        bool                    rootDirty            = true;
        uint32_t                uploadedTextureCount = 0;
        std::vector<DrawBucket> buckets;
    };

    static GpuSceneRange  allocateRange(std::vector<GpuSceneRange>& freeRanges, uint32_t count, uint32_t size);
    static void           releaseRange(std::vector<GpuSceneRange>& freeRanges, GpuSceneRange range);
    void                  ensureFrameCapacity(FrameResources& frame, uint32_t commandCount, uint32_t bucketCount);
    void                  rebuildDrawSnapshot();
    FrameResources&       currentFrame();
    const FrameResources& currentFrame() const;

    std::vector<ModelRef>              _models;
    std::vector<GpuSceneModelData>     _modelData;
    std::vector<RefPtr<Texture>>       _textures;
    std::vector<GpuSceneDrawData>      _drawData;
    std::vector<GpuSceneInstanceData>  _instanceData;
    std::vector<GpuSceneRange>         _freeDrawRanges;
    std::vector<GpuSceneRange>         _freeInstanceRanges;
    std::vector<DrawCommand>           _drawCommands; // Indexed by drawID; released entries have InvalidID.
    std::vector<DrawBucket>            _buckets;
    std::vector<GpuSceneDrawBucket>    _gpuBuckets;
    std::vector<GpuSceneDrawCandidate> _candidates;
    bool                               _drawSnapshotDirty = true;
    std::vector<FrameResources>        _frames;
};
} // namespace Play

#endif // GPU_SCENE_H
