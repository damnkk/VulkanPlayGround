#ifndef VIEW_CONTROL_H
#define VIEW_CONTROL_H

#include <glm/glm.hpp>

namespace Play
{
struct TAASettings
{
    bool  enabled       = true;
    float currentWeight = 0.15f;
    float jitterScale   = 1.0f;
};

struct ViewSettings
{
    TAASettings taa;
};

// One instance per view. Configuration is editable; temporal state advances
// once per frame and is shared by camera preparation and the TAA pass.
class ViewControl
{
public:
    ViewSettings& settings()
    {
        return _settings;
    }
    const ViewSettings& settings() const
    {
        return _settings;
    }
    const glm::vec2& jitterPixels() const
    {
        return _jitterPixels;
    }
    const glm::vec2& previousJitterPixels() const
    {
        return _previousJitterPixels;
    }
    bool historyReset() const
    {
        return _historyReset;
    }

    void beginFrame(bool cameraReset)
    {
        const auto& taa = _settings.taa;
        _historyReset   = cameraReset || taa.enabled != _wasEnabled || taa.jitterScale != _previousJitterScale;
        if (_historyReset)
        {
            _sampleIndex  = 0;
            _jitterPixels = glm::vec2(0.0f);
        }
        _previousJitterPixels = _jitterPixels;
        _jitterPixels         = glm::vec2(0.0f);
        if (taa.enabled)
        {
            // Halton(2, 3), eight samples inside one pixel. This index is
            // independent of the frames-in-flight slot index.
            _sampleIndex  = _sampleIndex % 8 + 1;
            _jitterPixels = (glm::vec2(halton(_sampleIndex, 2), halton(_sampleIndex, 3)) - glm::vec2(0.5f)) * taa.jitterScale;
        }
        if (_historyReset) _previousJitterPixels = _jitterPixels;
        _wasEnabled          = taa.enabled;
        _previousJitterScale = taa.jitterScale;
    }

private:
    static float halton(unsigned int index, unsigned int base)
    {
        float value  = 0.0f;
        float weight = 1.0f;
        while (index > 0)
        {
            weight /= static_cast<float>(base);
            value += weight * static_cast<float>(index % base);
            index /= base;
        }
        return value;
    }

    ViewSettings _settings;
    glm::vec2    _jitterPixels{0.0f};
    glm::vec2    _previousJitterPixels{0.0f};
    unsigned int _sampleIndex         = 0;
    bool         _wasEnabled          = false;
    bool         _historyReset        = true;
    float        _previousJitterScale = 1.0f;
};
} // namespace Play
#endif
