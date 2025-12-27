#ifndef PLAYCAMERA_H
#define PLAYCAMERA_H
#include <memory>
#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>
#include <vulkan/vulkan.h>
#include <nvutils/camera_manipulator.hpp>

namespace Play
{

class PlayCamera
{
public:
    PlayCamera();
    ~PlayCamera();
    void                        update(ImGuiWindow* viewportWindow);
    void                        onResize(const VkExtent2D& size);
    nvutils::CameraManipulator* getCameraManipulator() const
    {
        return _cameraManip.get();
    }

private:
    std::unique_ptr<nvutils::CameraManipulator> _cameraManip;
};
} // namespace Play
#endif // PLAYCAMERA_H