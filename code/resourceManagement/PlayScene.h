#ifndef RENDER_SCENE_H
#define RENDER_SCENE_H
#include <nvvkgltf/scene_vk.hpp>
#include <nvvkgltf/scene_rtx.hpp>
namespace Play
{
class RenderScene : public nvvkgltf::SceneVk
{
public:
    void     recordingCommandBuffer(VkCommandBuffer cmdBuffer);
    uint32_t getTextureOffset() const
    {
        return textureOffset;
    }
    void setTextureOffset(uint32_t offset)
    {
        textureOffset = offset;
    }

private:
    uint32_t textureOffset = 0;
}; // class RenderScene

class RTScene : public nvvkgltf::SceneRtx
{
}; // class RTScene
} // namespace Play
#endif // RENDER_SCENE_H