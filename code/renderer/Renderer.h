#ifndef RENDERER_H
#define RENDERER_H
#include "pch.h"
namespace Play
{

struct Scene;
class PlayElement;
class Texture;
class Renderer
{
public:
    Renderer() {};
    Renderer(PlayElement& app) : _app(&app) {};
    virtual ~Renderer() = default;

    virtual void     OnPreRender()                   = 0;
    virtual void     OnPostRender()                  = 0;
    virtual void     RenderFrame()                   = 0;
    virtual void     SetScene(Scene* scene)          = 0;
    virtual void     OnResize(int width, int height) = 0;
    virtual Texture* getOutputTexture()              = 0;

protected:
    Scene*       _scene;
    PlayElement* _app;

private:
};

} // namespace Play

#endif // RENDERER_H