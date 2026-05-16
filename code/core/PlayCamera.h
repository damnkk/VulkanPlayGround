#ifndef PLAYCAMERA_H
#define PLAYCAMERA_H
#include <memory>
#include <vulkan/vulkan.h>
#include <nvutils/camera_manipulator.hpp>

namespace Play::runtime
{
struct SdlInputState;
}

namespace Play
{

class PlayCamera
{
public:
    PlayCamera();
    ~PlayCamera();
    void                                        update(const runtime::SdlInputState& input, float deltaSeconds);
    void                                        onResize(const VkExtent2D& size);
    std::shared_ptr<nvutils::CameraManipulator> getCameraManipulator() const
    {
        return _cameraManip;
    }

private:
    std::shared_ptr<nvutils::CameraManipulator> _cameraManip;
};
} // namespace Play
#endif // PLAYCAMERA_H
