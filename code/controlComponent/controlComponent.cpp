#include "controlComponent.h"
#include "nvgui/tonemapper.hpp"
#include "nvgui/window.hpp"
namespace Play
{

void ToneMappingControlComponent::onGUI()
{
    ImGui::Begin("ToneMapper Profile");
    nvgui::tonemapperWidget(this->_cpuData);
    ImGui::End();
    flushToGPU();
}

} // namespace Play