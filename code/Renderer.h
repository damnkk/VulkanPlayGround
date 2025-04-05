#ifndef RENDERER_H
#define RENDERER_H
namespace Play
{

struct Scene;
struct PlayApp;
class Renderer
{
   public:
    Renderer(){};
    Renderer(PlayApp& app);
    virtual ~Renderer() = default;

    virtual void OnPreRender()=0;
    virtual void OnPostRender()=0;
    virtual void RenderFrame()=0;
    virtual void SetScene(Scene* scene) = 0;
    virtual void OnResize(int width, int height)=0 ;
    virtual void OnDestroy() =0;
    protected:
     Scene* _scene;
     PlayApp* _app;
   private:
};

}

#endif // RENDERER_H