#ifndef RENDERER_H
#define RENDERER_H
#include "pch.h"
#include <glm/glm.hpp>
#include "newShaders/Hdevice.h"
#include <memory>
#include <vector>
#include <array>
#include "core/RefCounted.h"

namespace Play
{
class PlayCamera;
struct SceneManager;
class PlayElement;
class Texture;
class Buffer;
class BasePass;
namespace RDG { class RDGBuilder; }

struct PerFrameRootData
{
    uint64_t cameraBufferDeviceAddress;
    uint64_t sceneAnimationBufferDeviceAddress;
};
class Renderer
{
public:
    Renderer();
    virtual ~Renderer();
    virtual void     OnGUI()                         = 0;
    virtual void     OnPreRender();
    virtual void     OnPostRender();
    virtual void     RenderFrame();
    virtual void     SetScene(SceneManager* scene);
    virtual void     OnResize(int width, int height);
    virtual Texture* getOutputTexture();
    virtual void     addCamera();

    virtual void        setActiveCamera(size_t index);
    virtual PlayCamera* getActiveCamera() const
    {
        return _cameras[_activeCameraIdx].get();
    }

    Buffer*       getCurrentCameraBuffer() const;
    SceneManager* getSceneManager()
    {
        return _scene.get();
    }

protected:
    virtual void setupPasses() = 0;

    void                                     updateCameraBuffer();
    std::unique_ptr<SceneManager>            _scene;
    std::vector<std::unique_ptr<PlayCamera>> _cameras;
    uint32_t                                 _activeCameraIdx = 0;
    std::array<CameraData, 3>                _cameraDatas;
    std::array<RefPtr<Buffer>, 3>            _cameraUniformData;

    std::unique_ptr<RDG::RDGBuilder>         _rdgBuilder;
    std::vector<std::unique_ptr<BasePass>>   _passes;
    Texture*                                 _outputTexture = nullptr;
    PlayElement*                             _view          = nullptr;

private:
};

} // namespace Play

#endif // RENDERER_H