#include "PlayCamera.h"

#include "core/runtime/SdlWindow.h"

namespace Play
{

PlayCamera::PlayCamera()
{
    _cameraManip = std::make_shared<nvutils::CameraManipulator>();
    // _cameraManip->setFov(120.0f);
    _cameraManip->setClipPlanes({0.1F, 10000.0F});
    _cameraManip->setLookat({0.0F, 0.5F, -1.0F}, {0.0F, 0.0F, 0.0F}, {0.0F, 1.0F, 0.0F});
}
PlayCamera::~PlayCamera() {}
void PlayCamera::update(const runtime::SdlInputState& input, float deltaSeconds)
{
    nvutils::CameraManipulator::Inputs inputs; // Mouse and keyboard inputs

    _cameraManip->updateAnim(); // This makes the camera to transition smoothly to the new position

    // The SDL window is the viewport in the runtime path.
    if (!input.mouseInWindow) return;

    inputs.lmb   = input.lmb;
    inputs.rmb   = input.rmb;
    inputs.mmb   = input.mmb;
    inputs.ctrl  = input.ctrl;
    inputs.shift = input.shift;
    inputs.alt   = input.alt;

    // None of the modifiers should be pressed for the single key: WASD and arrows
    if (!inputs.alt)
    {
        // Speed of the camera movement when using WASD and arrows
        float keyMotionFactor = deltaSeconds;
        if (inputs.shift)
        {
            keyMotionFactor *= 5.0F; // Speed up the camera movement
        }
        if (inputs.ctrl)
        {
            keyMotionFactor *= 0.1F; // Slow down the camera movement
        }

        if (input.keyW)
        {
            _cameraManip->keyMotion({keyMotionFactor, 0}, nvutils::CameraManipulator::Actions::Dolly);
            inputs.shift = inputs.ctrl = false;
        }

        if (input.keyS)
        {
            _cameraManip->keyMotion({-keyMotionFactor, 0}, nvutils::CameraManipulator::Actions::Dolly);
            inputs.shift = inputs.ctrl = false;
        }

        if (input.keyD || input.keyRight)
        {
            _cameraManip->keyMotion({keyMotionFactor, 0}, nvutils::CameraManipulator::Actions::Pan);
            inputs.shift = inputs.ctrl = false;
        }

        if (input.keyA || input.keyLeft)
        {
            _cameraManip->keyMotion({-keyMotionFactor, 0}, nvutils::CameraManipulator::Actions::Pan);
            inputs.shift = inputs.ctrl = false;
        }

        if (input.keyUp)
        {
            _cameraManip->keyMotion({0, keyMotionFactor}, nvutils::CameraManipulator::Actions::Pan);
            inputs.shift = inputs.ctrl = false;
        }

        if (input.keyDown)
        {
            _cameraManip->keyMotion({0, -keyMotionFactor}, nvutils::CameraManipulator::Actions::Pan);
            inputs.shift = inputs.ctrl = false;
        }
    }

    if (input.lmbPressed || input.mmbPressed || input.rmbPressed)
    {
        _cameraManip->setMousePosition({input.mouseX, input.mouseY});
    }

    if (input.lmb || input.mmb || input.rmb)
    {
        _cameraManip->mouseMove({input.mouseX, input.mouseY}, inputs);
    }

    // Mouse Wheel
    if (input.wheelY != 0.0F)
    {
        _cameraManip->wheel(input.wheelY * -3.0F, inputs);
    }
}

void PlayCamera::onResize(const VkExtent2D& size)
{
    _cameraManip->setWindowSize({size.width, size.height});
}
} // namespace Play
