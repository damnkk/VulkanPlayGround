#include "PlayCamera.h"
#include <nvgui/window.hpp>
namespace Play
{

PlayCamera::PlayCamera()
{
    _cameraManip = std::make_unique<nvutils::CameraManipulator>();
    _cameraManip->setFov(120.0f);
}
PlayCamera::~PlayCamera() {}
void PlayCamera::update(ImGuiWindow* viewportWindow)
{
    nvutils::CameraManipulator::Inputs inputs; // Mouse and keyboard inputs

    _cameraManip->updateAnim(); // This makes the camera to transition smoothly to the new position

    // Check if the mouse cursor is over the "Viewport", check for all inputs that can manipulate the camera.
    if (!nvgui::isWindowHovered(viewportWindow)) return;

    inputs.lmb      = ImGui::IsMouseDown(ImGuiMouseButton_Left);
    inputs.rmb      = ImGui::IsMouseDown(ImGuiMouseButton_Right);
    inputs.mmb      = ImGui::IsMouseDown(ImGuiMouseButton_Middle);
    inputs.ctrl     = ImGui::IsKeyDown(ImGuiKey_LeftCtrl) || ImGui::IsKeyDown(ImGuiKey_RightCtrl);
    inputs.shift    = ImGui::IsKeyDown(ImGuiKey_LeftShift) || ImGui::IsKeyDown(ImGuiKey_RightShift);
    inputs.alt      = ImGui::IsKeyDown(ImGuiKey_LeftAlt) || ImGui::IsKeyDown(ImGuiKey_RightAlt);
    ImVec2 mousePos = ImGui::GetMousePos();

    // None of the modifiers should be pressed for the single key: WASD and arrows
    if (!inputs.alt)
    {
        // Speed of the camera movement when using WASD and arrows
        float keyMotionFactor = ImGui::GetIO().DeltaTime;
        if (inputs.shift)
        {
            keyMotionFactor *= 5.0F; // Speed up the camera movement
        }
        if (inputs.ctrl)
        {
            keyMotionFactor *= 0.1F; // Slow down the camera movement
        }

        if (ImGui::IsKeyDown(ImGuiKey_W))
        {
            _cameraManip->keyMotion({keyMotionFactor, 0}, nvutils::CameraManipulator::Dolly);
            inputs.shift = inputs.ctrl = false;
        }

        if (ImGui::IsKeyDown(ImGuiKey_S))
        {
            _cameraManip->keyMotion({-keyMotionFactor, 0}, nvutils::CameraManipulator::Dolly);
            inputs.shift = inputs.ctrl = false;
        }

        if (ImGui::IsKeyDown(ImGuiKey_D) || ImGui::IsKeyDown(ImGuiKey_RightArrow))
        {
            _cameraManip->keyMotion({keyMotionFactor, 0}, nvutils::CameraManipulator::Pan);
            inputs.shift = inputs.ctrl = false;
        }

        if (ImGui::IsKeyDown(ImGuiKey_A) || ImGui::IsKeyDown(ImGuiKey_LeftArrow))
        {
            _cameraManip->keyMotion({-keyMotionFactor, 0}, nvutils::CameraManipulator::Pan);
            inputs.shift = inputs.ctrl = false;
        }

        if (ImGui::IsKeyDown(ImGuiKey_UpArrow))
        {
            _cameraManip->keyMotion({0, keyMotionFactor}, nvutils::CameraManipulator::Pan);
            inputs.shift = inputs.ctrl = false;
        }

        if (ImGui::IsKeyDown(ImGuiKey_DownArrow))
        {
            _cameraManip->keyMotion({0, -keyMotionFactor}, nvutils::CameraManipulator::Pan);
            inputs.shift = inputs.ctrl = false;
        }
    }

    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) || ImGui::IsMouseClicked(ImGuiMouseButton_Middle) ||
        ImGui::IsMouseClicked(ImGuiMouseButton_Right))
    {
        _cameraManip->setMousePosition({mousePos.x, mousePos.y});
    }

    if (ImGui::IsMouseDragging(ImGuiMouseButton_Left, 1.0F) || ImGui::IsMouseDragging(ImGuiMouseButton_Middle, 1.0F) ||
        ImGui::IsMouseDragging(ImGuiMouseButton_Right, 1.0F))
    {
        _cameraManip->mouseMove({mousePos.x, mousePos.y}, inputs);
    }

    // Mouse Wheel
    if (ImGui::GetIO().MouseWheel != 0.0F)
    {
        _cameraManip->wheel(ImGui::GetIO().MouseWheel * -3.f, inputs);
    }
}

void PlayCamera::onResize(const VkExtent2D& size)
{
    _cameraManip->setWindowSize({size.width, size.height});
}
} // namespace Play