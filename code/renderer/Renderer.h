#ifndef RENDERER_H
#define RENDERER_H
#include "pch.h"
#include <glm/glm.hpp>
#include "newShaders/Hdevice.h"
#include <memory>
#include <vector>
#include <array>

namespace Play
{
class PlayCamera;
struct SceneManager;
class PlayElement;
class Texture;
class Buffer;

struct PerFrameRootData
{
    uint64_t cameraBufferDeviceAddress;
    uint64_t sceneAnimationBufferDeviceAddress;
};
class Renderer
{
public:
    Renderer();
    virtual ~Renderer()                              = default;
    virtual void     OnGUI()                         = 0;
    virtual void     OnPreRender()                   = 0;
    virtual void     OnPostRender()                  = 0;
    virtual void     RenderFrame()                   = 0;
    virtual void     SetScene(SceneManager* scene)   = 0;
    virtual void     OnResize(int width, int height) = 0;
    virtual Texture* getOutputTexture()              = 0;
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
    std::unique_ptr<SceneManager>            _scene;
    std::vector<std::unique_ptr<PlayCamera>> _cameras;
    uint32_t                                 _activeCameraIdx = 0;
    std::array<CameraData, 3>                _cameraDatas;
    std::array<Buffer*, 3>                   _cameraUniformData;

private:
};

} // namespace Play

#endif // RENDERER_H