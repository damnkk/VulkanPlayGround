#include "controlComponent.h"
#include "nvgui/tonemapper.hpp"
#include "nvgui/window.hpp"
#include <nvshaders/tonemap_functions.h.slang>
namespace Play
{

void ToneMappingControlComponent::onGUI()
{
    ImGui::Begin("ToneMapper Profile");
    // this->_cpuData.inputMatrix = shaderio::getColorCorrectionMatrix(1.0, 1.0, 1.0);
    nvgui::tonemapperWidget(this->_cpuData);
    ImGui::End();
    flushToGPU();
}

} // namespace Play