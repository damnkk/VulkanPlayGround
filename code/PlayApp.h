#ifndef PLAYAPP_H
#define PLAYAPP_H
#include "nvvkhl/appbase_vk.hpp"
#include "ModelLoader.h" // Ensure ModelLoader class is defined in this header
#include "nvvk/memallocator_vma_vk.hpp"
#include "Resource.h"
namespace Play
{

class PlayApp : public nvvkhl::AppBaseVk
{
   public:
    void onResize(int width, int height) override;
    void OnInit();
    void Run();
    void RenderFrame();
    void OnPreRender();
    void OnPostRender();

   private:
    friend class ModelLoader;
    ModelLoader _modelLoader;
    nvvk::ResourceAllocatorVma _alloc;
    TexturePool                _texturePool;
    BufferPool                 _bufferPool;
};

} //    namespace Play

#endif // PLAYAPP_H