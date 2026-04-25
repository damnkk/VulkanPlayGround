#ifndef VOLUMESKYPASS_H
#define VOLUMESKYPASS_H
#include "RenderPass.h"
#include "core/RefCounted.h"
#include <memory.h>
#include "PlayProgram.h"
#include "controlComponent/controlComponent.h"
// Keep explicit padding so the CPU-side layout matches the shader constant-buffer packing.
struct AtmosphereParameters
{
    float BottomRadius            DEFAULT(6360.0f);
    float TopRadius               DEFAULT(6460.0f);
    float RayleighDensityExpScale DEFAULT(-0.125);

    float3 RayleighScattering DEFAULT(float3(0.005802f, 0.013558f, 0.033100f));
    float MieDensityExpScale  DEFAULT(-0.8333333);

    float3 MieScattering DEFAULT(float3(0.003996, 0.003996, 0.003996));

    float3 MieExtinction DEFAULT(float3(0.004440, 0.004440, 0.004440));

    float3 MieAbsorption DEFAULT(float3(0.000444, 0.000444, 0.000444));
    float MiePhaseG      DEFAULT(0.8f);

    float AbsorptionDensity0LayerWidth   DEFAULT(25.0f);
    float AbsorptionDensity0ConstantTerm DEFAULT(-2.0f / 3.0f);
    float AbsorptionDensity0LinearTerm   DEFAULT(1.0f / 15.0f);
    float AbsorptionDensity1ConstantTerm DEFAULT(8.0f / 3.0f);

    float AbsorptionDensity1LinearTerm DEFAULT(-1.0f / 15.0f);
    float3 AbsorptionExtinction        DEFAULT(float3(0.000650, 0.001881, 0.000085));

    float3 GroundAlbedo DEFAULT(float3(0.0f, 0.0f, 0.0f));

    float3 solar_irradiance  DEFAULT(float3(1.0f));
    float sun_angular_radius DEFAULT(0.004675);
    float mu_s_min           DEFAULT(-0.5);
};
namespace Play
{

class DeferRenderer;
class VolumeSkyPass : public BasePass
{
public:
    VolumeSkyPass() = default;
    VolumeSkyPass(DeferRenderer* ownedRender) : _ownedRender(ownedRender) {}
    virtual ~VolumeSkyPass() override;
    virtual void init() override;
    virtual void build(RDG::RDGBuilder* rdgBuilder) override;
    virtual void onGUI() override;

private:
    struct AtmosControler : public ControlComponent<AtmosphereParameters>
    {
    public:
        virtual void onGUI() override;
    } _skyAtmosControler;
    RefPtr<ComputeProgram> _transmittanceLutProgram;
    RefPtr<RenderProgram>  _skyBoxProgram;
    RefPtr<RenderProgram>  _atmosphereProgram;
    RefPtr<RenderProgram>  _volumetricCloudProgram;
    DeferRenderer*         _ownedRender = nullptr;

    // 常驻资源
    RefPtr<Texture> _transmittanceLut;
};

} // namespace Play

#endif // VOLUMESKYPASS_H
