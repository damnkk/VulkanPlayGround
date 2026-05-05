#ifndef ATMOSPHERE_H
#define ATMOSPHERE_H
#include "constants.h.slang"
struct AtmosphereParameters
{
    // Radius of the planet (center to ground)
    float BottomRadius;
    // Maximum considered atmosphere height (center to atmosphere top)
    float TopRadius;

    // Rayleigh scattering exponential distribution scale in the atmosphere
    float RayleighDensityExpScale;
    // Rayleigh scattering coefficients
    float3 RayleighScattering;

    // Mie scattering exponential distribution scale in the atmosphere
    float MieDensityExpScale;
    // Mie scattering coefficients
    float3 MieScattering;
    // Mie extinction coefficients
    float3 MieExtinction;
    // Mie absorption coefficients
    float3 MieAbsorption;
    // Mie phase function excentricity
    float MiePhaseG;

    // Another medium type in the atmosphere
    float AbsorptionDensity0LayerWidth;
    float AbsorptionDensity0ConstantTerm;
    float AbsorptionDensity0LinearTerm;
    float AbsorptionDensity1ConstantTerm;
    float AbsorptionDensity1LinearTerm;
    // This other medium only absorb light, e.g. useful to represent ozone in the earth atmosphere
    float3 AbsorptionExtinction;

    // The albedo of the ground.
    float3 GroundAlbedo;
    // x: sun pitch, y: sun yaw.
    float2 sun_dir;
};

float fromSubUvsToUnit(float u, float resolution)
{
    return (u - 0.5f / resolution) * (resolution / (resolution - 1.0f));
}

void UvToLutTransmittanceParams(AtmosphereParameters Atmosphere, out float viewHeight, out float viewZenithCosAngle, in float2 uv)
{
    float x_mu = uv.x;
    float x_r  = uv.y;

    float H    = sqrt(Atmosphere.TopRadius * Atmosphere.TopRadius - Atmosphere.BottomRadius * Atmosphere.BottomRadius);
    float rho  = H * x_r;
    viewHeight = sqrt(rho * rho + Atmosphere.BottomRadius * Atmosphere.BottomRadius);

    float d_min        = Atmosphere.TopRadius - viewHeight;
    float d_max        = rho + H;
    float d            = d_min + x_mu * (d_max - d_min);
    viewZenithCosAngle = d == 0.0 ? 1.0f : (H * H - rho * rho - d * d) / (2.0 * viewHeight * d);
    viewZenithCosAngle = clamp(viewZenithCosAngle, -1.0, 1.0);
}

void LutTransmittanceParamsToUv(AtmosphereParameters Atmosphere, in float viewHeight, in float viewZenithCosAngle, out float2 uv)
{
    float H   = sqrt(max(0.0f, Atmosphere.TopRadius * Atmosphere.TopRadius - Atmosphere.BottomRadius * Atmosphere.BottomRadius));
    float rho = sqrt(max(0.0f, viewHeight * viewHeight - Atmosphere.BottomRadius * Atmosphere.BottomRadius));

    float discriminant = viewHeight * viewHeight * (viewZenithCosAngle * viewZenithCosAngle - 1.0) + Atmosphere.TopRadius * Atmosphere.TopRadius;
    float d            = max(0.0, (-viewHeight * viewZenithCosAngle + sqrt(discriminant))); // Distance to atmosphere boundary

    float d_min = Atmosphere.TopRadius - viewHeight;
    float d_max = rho + H;
    float x_mu  = (d - d_min) / (d_max - d_min);
    float x_r   = rho / H;

    uv = float2(x_mu, x_r);
    // uv = float2(fromUnitToSubUvs(uv.x, TRANSMITTANCE_TEXTURE_WIDTH), fromUnitToSubUvs(uv.y, TRANSMITTANCE_TEXTURE_HEIGHT)); // No real impact so
    // off
}

// - r0: ray origin
// - rd: normalized ray direction
// - s0: sphere center
// - sR: sphere radius
// - Returns distance from r0 to first intersecion with sphere,
//   or -1.0 if no intersection.
float raySphereIntersectNearest(float3 r0, float3 rd, float3 s0, float sR)
{
    float  a     = dot(rd, rd);
    float3 s0_r0 = r0 - s0;
    float  b     = 2.0 * dot(rd, s0_r0);
    float  c     = dot(s0_r0, s0_r0) - (sR * sR);
    float  delta = b * b - 4.0 * a * c;
    if (delta < 0.0 || a == 0.0)
    {
        return -1.0;
    }

    float sol0 = (-b - sqrt(delta)) / (2.0 * a);
    float sol1 = (-b + sqrt(delta)) / (2.0 * a);
    if (sol0 < 0.0 && sol1 < 0.0)
    {
        return -1.0;
    }
    if (sol0 < 0.0)
    {
        return max(0.0, sol1);
    }
    else if (sol1 < 0.0)
    {
        return max(0.0, sol0);
    }
    return max(0.0, min(sol0, sol1));
}

float3 SampleMediumExtinction(float3 WorldPos, in AtmosphereParameters Atmosphere)
{
    const float viewHeight = length(WorldPos) - Atmosphere.BottomRadius;

    const float densityMie = exp(Atmosphere.MieDensityExpScale * viewHeight);
    const float densityRay = exp(Atmosphere.RayleighDensityExpScale * viewHeight);
    const float densityOzo = saturate(viewHeight < Atmosphere.AbsorptionDensity0LayerWidth
                                          ? Atmosphere.AbsorptionDensity0LinearTerm * viewHeight + Atmosphere.AbsorptionDensity0ConstantTerm
                                          : Atmosphere.AbsorptionDensity1LinearTerm * viewHeight + Atmosphere.AbsorptionDensity1ConstantTerm);

    const float3 extinctionMie = densityMie * Atmosphere.MieExtinction;
    const float3 extinctionRay = densityRay * Atmosphere.RayleighScattering;
    const float3 extinctionOzo = densityOzo * Atmosphere.AbsorptionExtinction;

    return extinctionMie + extinctionRay + extinctionOzo;
}

float3 SampleMediumScattering(float3 WorldPos, in AtmosphereParameters Atmosphere)
{
    const float viewHeight = length(WorldPos) - Atmosphere.BottomRadius;

    const float densityMie = exp(Atmosphere.MieDensityExpScale * viewHeight);
    const float densityRay = exp(Atmosphere.RayleighDensityExpScale * viewHeight);
    const float densityOzo = saturate(viewHeight < Atmosphere.AbsorptionDensity0LayerWidth
                                          ? Atmosphere.AbsorptionDensity0LinearTerm * viewHeight + Atmosphere.AbsorptionDensity0ConstantTerm
                                          : Atmosphere.AbsorptionDensity1LinearTerm * viewHeight + Atmosphere.AbsorptionDensity1ConstantTerm);

    const float3 scatteringMie = densityMie * Atmosphere.MieScattering;
    const float3 scatteringRay = densityRay * Atmosphere.RayleighScattering;
    const float3 scatteringOzo = 0.0;

    return scatteringMie + scatteringRay + scatteringOzo;
     
}

void UvToSkyViewLutParams(AtmosphereParameters Atmosphere, out float viewZenithCosAngle, out float lightViewCosAngle, in float viewHeight, in float2 uv)
{
    // Constrain uvs to valid sub texel range (avoid zenith derivative issue making LUT usage visible)
    uv = float2(fromSubUvsToUnit(uv.x, 192.0f), fromSubUvsToUnit(uv.y, 108.0f));

    float Vhorizon           = sqrt(viewHeight * viewHeight - Atmosphere.BottomRadius * Atmosphere.BottomRadius);
    float CosBeta            = Vhorizon / viewHeight; // GroundToHorizonCos
    float Beta               = acos(CosBeta);
    float ZenithHorizonAngle = M_PI - Beta;

    if (uv.y < 0.5f)
    {
        float coord = 2.0 * uv.y;
        coord       = 1.0 - coord;
        coord *= coord;
        coord              = 1.0 - coord;
        viewZenithCosAngle = cos(ZenithHorizonAngle * coord);
    }
    else
    {
        float coord = uv.y * 2.0 - 1.0;
        coord *= coord;
        viewZenithCosAngle = cos(ZenithHorizonAngle + Beta * coord);
    }

    float coord = uv.x;
    coord *= coord;
    lightViewCosAngle = -(coord * 2.0 - 1.0);
}

float3 GetSunDirection(float uiSunPitch, float uiSunYaw)
{
    float cp = cos(uiSunPitch);
    float sp = sin(uiSunPitch);
    float cy = cos(uiSunYaw);
    float sy = sin(uiSunYaw);

    return float3(
        cp * sy,
        cp * cy,
        sp
    );
}

bool MoveToTopAtmosphere(inout float3 WorldPos, in float3 WorldDir, in float AtmosphereTopRadius)
{
    float viewHeight = length(WorldPos);
    if (viewHeight > AtmosphereTopRadius)
    {
        float tTop = raySphereIntersectNearest(WorldPos, WorldDir, float3(0.0f, 0.0f, 0.0f), AtmosphereTopRadius);
        if (tTop >= 0.0f)
        {
            float3 UpVector = WorldPos / viewHeight;
            float3 UpOffset = UpVector * - 0.01;
            WorldPos        = WorldPos + WorldDir * tTop + UpOffset;
        }
        else
        {
            // Ray is not intersecting the atmosphere
            return false;
        }
    }
    return true; // ok to start tracing
}

float hgPhase(float g, float cosTheta)
{
    float k = 3.0 / (8.0 * M_PI) * (1.0 - g * g) / (2.0 + g * g);
    return k * (1.0 + cosTheta * cosTheta) / pow(1.0 + g * g - 2.0 * g * -cosTheta, 1.5);
}

float RayleighPhase(float cosTheta)
{
    float factor = 3.0f / (16.0f * M_PI);
    return factor * (1.0f + cosTheta * cosTheta);
}

struct MediumSampleRGB
{
    float3 scattering;
    float3 absorption;
    float3 extinction;

    float3 scatteringMie;
    float3 absorptionMie;
    float3 extinctionMie;

    float3 scatteringRay;
    float3 absorptionRay;
    float3 extinctionRay;

    float3 scatteringOzo;
    float3 absorptionOzo;
    float3 extinctionOzo;

    float3 albedo;
};

float getAlbedo(float scattering, float extinction)
{
    return scattering / max(0.001, extinction);
}
float3 getAlbedo(float3 scattering, float3 extinction)
{
    return scattering / max(0.001, extinction);
}

MediumSampleRGB sampleMediumRGB(in float3 WorldPos, in AtmosphereParameters Atmosphere)
{
    const float viewHeight = length(WorldPos) - Atmosphere.BottomRadius;

    const float densityMie = exp(Atmosphere.MieDensityExpScale * viewHeight);
    const float densityRay = exp(Atmosphere.RayleighDensityExpScale * viewHeight);
    const float densityOzo = saturate(viewHeight < Atmosphere.AbsorptionDensity0LayerWidth ?
		Atmosphere.AbsorptionDensity0LinearTerm * viewHeight + Atmosphere.AbsorptionDensity0ConstantTerm :
		Atmosphere.AbsorptionDensity1LinearTerm * viewHeight + Atmosphere.AbsorptionDensity1ConstantTerm);

    MediumSampleRGB s;

    s.scatteringMie = densityMie * Atmosphere.MieScattering;
    s.absorptionMie = densityMie * Atmosphere.MieAbsorption;
    s.extinctionMie = densityMie * Atmosphere.MieExtinction;

    s.scatteringRay = densityRay * Atmosphere.RayleighScattering;
    s.absorptionRay = 0.0f;
    s.extinctionRay = s.scatteringRay + s.absorptionRay;

    s.scatteringOzo = 0.0;
    s.absorptionOzo = densityOzo * Atmosphere.AbsorptionExtinction;
    s.extinctionOzo = s.scatteringOzo + s.absorptionOzo;

    s.scattering = s.scatteringMie + s.scatteringRay + s.scatteringOzo;
    s.absorption = s.absorptionMie + s.absorptionRay + s.absorptionOzo;
    s.extinction = s.extinctionMie + s.extinctionRay + s.extinctionOzo;
    s.albedo     = getAlbedo(s.scattering, s.extinction);

    return s;
}

#endif // ATMOSPHERE_H
