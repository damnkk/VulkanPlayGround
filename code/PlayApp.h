
#include "nvvkhl/appbase_vk.hpp"
#include "ModelLoader.h"
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
    ModelLoader _modelLoader;
};

} //    namespace Play