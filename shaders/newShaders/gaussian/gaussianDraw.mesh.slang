#include "common.slang"
#include "gaussian/gaussianLib.h.slang"
#include "PConstantType.h.slang"
[[vk::binding(0, 3)]]
RWStructuredBuffer<IndrectBuffer> indirectBuffer;
[vk_binding(1, 3)]
RWStructuredBuffer<uint32_t> indicesBuffer;
[vk_binding(2, 3)]
ConstantBuffer<GaussianSceneUniform> sceneConstant;
[[vk::push_constant]]
ConstantBuffer<PerFrameConstant> perFrameConstant;
#define RASTER_MESH_WORKGROUP_SIZE 32
// 4 vertices per workgroup, is a quad (2 triangles)
static const int MAX_VERTICES   = 4 * RASTER_MESH_WORKGROUP_SIZE;
static const int MAX_PRIMITIVES = 2 * RASTER_MESH_WORKGROUP_SIZE;

float4 fetchColor(in const uint64_t constant, in uint splatIndex)
{
    float* color = (float*) constant;
    float4 res   = float4(color[splatIndex * 4 + 0], color[splatIndex * 4 + 1], color[splatIndex * 4 + 2], color[splatIndex * 4 + 3]);
    return res;
}

float3 fetchCenter(in const uint64_t constantAddress, in uint splatIndex)
{
    float* center = (float*) constantAddress;
    float3 res    = float3(center[splatIndex * 3 + 0], center[splatIndex * 3 + 1], center[splatIndex * 3 + 2]);
    return res;
}

float3x3 fetchCovariance(in const uint64_t covariancesBuffer, in uint splatIndex)
{
    float* covariancesBuffer = (float*) covariancesBuffer;
    // Use RGBA texture map to store sets of 3 elements requires some offset shifting depending on splatIndex
    const float3 cov3D_M11_M12_M13 =
        float3(covariancesBuffer[splatIndex * 6 + 0], covariancesBuffer[splatIndex * 6 + 1], covariancesBuffer[splatIndex * 6 + 2]);
    const float3 cov3D_M22_M23_M33 =
        float3(covariancesBuffer[splatIndex * 6 + 3], covariancesBuffer[splatIndex * 6 + 4], covariancesBuffer[splatIndex * 6 + 5]);

    return float3x3(cov3D_M11_M12_M13.x, cov3D_M11_M12_M13.y, cov3D_M11_M12_M13.z, cov3D_M11_M12_M13.y, cov3D_M22_M23_M33.x, cov3D_M22_M23_M33.y,
                    cov3D_M11_M12_M13.z, cov3D_M22_M23_M33.y, cov3D_M22_M23_M33.z);
}

float3 threedgsCovarianceProjection(float3x3 cov3Dm, float4 splatCenterView, float2 focal, float4x4 modelViewTransform)
{
    const float    s       = 1.0 / (splatCenterView.z * splatCenterView.z);
    const float3x3 J       = float3x3(focal.x / splatCenterView.z, 0.0, -(focal.x * splatCenterView.x) * s, 0.0, focal.y / splatCenterView.z,
                                      -(focal.y * splatCenterView.y) * s, 0.0, 0.0, 0.0);
    const float3x3 W       = transpose(float3x3(modelViewTransform));
    const float3x3 T       = mul(J, W);
    const float3x3 conv2Dm = mul(mul(T, cov3Dm), transpose(T));
    return float3(conv2Dm[0][0], conv2Dm[0][1], conv2Dm[1][1]);
}

bool threedgsProjectedExtentBasis(float3 cov2Dv, float stdDev, float splatScale, inout float opacity, out float2 basisVector1,
                                  out float2 basisVector2)
{
    cov2Dv[0] += 0.3;
    cov2Dv[2] += 0.3;

    const float a           = cov2Dv.x;
    const float d           = cov2Dv.z;
    const float b           = cov2Dv.y;
    const float D           = a * d - b * b;
    const float trace       = a + d;
    const float traceOver2  = 0.5 * trace;
    const float term2       = sqrt(max(0.1f, traceOver2 * traceOver2 - D));
    float       eigenValue1 = traceOver2 + term2;
    float       eigenValue2 = traceOver2 - term2;

    if (eigenValue2 <= 0.0f)
    {
        return false;
    }
    const float2 eigenVector1 = normalize(float2(b, eigenValue1 - a));
    // since the eigen vectors are orthogonal, we derive the second one from the first
    const float2 eigenVector2 = float2(eigenVector1.y, -eigenVector1.x);

    basisVector1 = eigenVector1 * splatScale * min(stdDev * sqrt(eigenValue1), 2048.0);
    basisVector2 = eigenVector2 * splatScale * min(stdDev * sqrt(eigenValue2), 2048.0);

    return true;
}

void emitDegeneratedQuad(uint localIndex, out OutputVertices<VertexOutput, MAX_VERTICES> verts)
{
    [unroll]
    for (uint i = 0; i < 4; ++i)
    {
        verts[localIndex * 4 + i].position = float4(0.0, 0.0, 3.0, 1.0);
    }
}

uint getShDegree(uint shRestCoefficientCount)
{
    if (shRestCoefficientCount == 0)
    {
        return 0;
    }

    const uint shRestCoefficientCountPerChannel = shRestCoefficientCount / 3;
    uint       shDegree                         = 0;
    uint       accumulatedCoefficientCount      = 0;
    while (accumulatedCoefficientCount < shRestCoefficientCountPerChannel)
    {
        ++shDegree;
        accumulatedCoefficientCount += 2 * shDegree + 1;
    }

    return shDegree;
}

void fetchSh(in uint splatIndex, in uint shDegree, out float3 shd[15])
{
    float* shArray = (float*) sceneConstant.shBufferDeviceAddress;

    const uint shStride = sceneConstant.shStride;
    const bool hasShD1  = shDegree >= 1;
    const bool hasShD2  = shDegree >= 2;
    const bool hasShD3  = shDegree >= 3;
    if (hasShD1)
    {
        shd[0] = float3(shArray[splatIndex * shStride + 3 * 0 + 0], shArray[splatIndex * shStride + 3 * 0 + 1],
                        shArray[splatIndex * shStride + 3 * 0 + 2]);
        shd[1] = float3(shArray[splatIndex * shStride + 3 * 1 + 0], shArray[splatIndex * shStride + 3 * 1 + 1],
                        shArray[splatIndex * shStride + 3 * 1 + 2]);
        shd[2] = float3(shArray[splatIndex * shStride + 3 * 2 + 0], shArray[splatIndex * shStride + 3 * 2 + 1],
                        shArray[splatIndex * shStride + 3 * 2 + 2]);
    }
    if (hasShD2)
    {
        shd[3] = float3(shArray[splatIndex * shStride + 3 * 3 + 0], shArray[splatIndex * shStride + 3 * 3 + 1],
                        shArray[splatIndex * shStride + 3 * 3 + 2]);
        shd[4] = float3(shArray[splatIndex * shStride + 3 * 4 + 0], shArray[splatIndex * shStride + 3 * 4 + 1],
                        shArray[splatIndex * shStride + 3 * 4 + 2]);
        shd[5] = float3(shArray[splatIndex * shStride + 3 * 5 + 0], shArray[splatIndex * shStride + 3 * 5 + 1],
                        shArray[splatIndex * shStride + 3 * 5 + 2]);
        shd[6] = float3(shArray[splatIndex * shStride + 3 * 6 + 0], shArray[splatIndex * shStride + 3 * 6 + 1],
                        shArray[splatIndex * shStride + 3 * 6 + 2]);
        shd[7] = float3(shArray[splatIndex * shStride + 3 * 7 + 0], shArray[splatIndex * shStride + 3 * 7 + 1],
                        shArray[splatIndex * shStride + 3 * 7 + 2]);
    }
    if (hasShD3)
    {
        shd[8]  = float3(shArray[splatIndex * shStride + 3 * 8 + 0], shArray[splatIndex * shStride + 3 * 8 + 1],
                         shArray[splatIndex * shStride + 3 * 8 + 2]);
        shd[9]  = float3(shArray[splatIndex * shStride + 3 * 9 + 0], shArray[splatIndex * shStride + 3 * 9 + 1],
                         shArray[splatIndex * shStride + 3 * 9 + 2]);
        shd[10] = float3(shArray[splatIndex * shStride + 3 * 10 + 0], shArray[splatIndex * shStride + 3 * 10 + 1],
                         shArray[splatIndex * shStride + 3 * 10 + 2]);
        shd[11] = float3(shArray[splatIndex * shStride + 3 * 11 + 0], shArray[splatIndex * shStride + 3 * 11 + 1],
                         shArray[splatIndex * shStride + 3 * 11 + 2]);
        shd[12] = float3(shArray[splatIndex * shStride + 3 * 12 + 0], shArray[splatIndex * shStride + 3 * 12 + 1],
                         shArray[splatIndex * shStride + 3 * 12 + 2]);
        shd[13] = float3(shArray[splatIndex * shStride + 3 * 13 + 0], shArray[splatIndex * shStride + 3 * 13 + 1],
                         shArray[splatIndex * shStride + 3 * 13 + 2]);
        shd[14] = float3(shArray[splatIndex * shStride + 3 * 14 + 0], shArray[splatIndex * shStride + 3 * 14 + 1],
                         shArray[splatIndex * shStride + 3 * 14 + 2]);
    }
}

float3 fetchViewDependentRadiance(in uint splatIndex, in float3 worldViewDir)
{
    float3     rgb      = float3(0.0);
    const uint shDegree = getShDegree(sceneConstant.shStride);
    float3     shd[15];
    fetchSh(splatIndex, shDegree, shd);
    const float x   = worldViewDir.x;
    const float y   = worldViewDir.y;
    const float z   = worldViewDir.z;
    const float xx  = x * x;
    const float yy  = y * y;
    const float zz  = z * z;
    const float xy  = x * y;
    const float yz  = y * z;
    const float xz  = x * z;
    const float xyy = x * yy;
    const float yzz = y * zz;
    const float zxx = z * xx;
    const float xyz = x * y * z;
    if (shDegree >= 1)
        rgb += SH_C1 * (-shd[0] * y + shd[1] * z - shd[2] * x);
    if (shDegree >= 2)
    {
        rgb += (SH_C2[0] * xy) * shd[3] + (SH_C2[1] * yz) * shd[4] + (SH_C2[2] * (2.0 * zz - xx - yy)) * shd[5] + (SH_C2[3] * xz) * shd[6] +
               (SH_C2[4] * (xx - yy)) * shd[7];
    }
    if (shDegree >= 3)
    {
        rgb += SH_C3[0] * shd[8] * (3.0 * x * x - y * y) * y + SH_C3[1] * shd[9] * x * y * z +
               SH_C3[2] * shd[10] * (4.0 * z * z - x * x - y * y) * y + SH_C3[3] * shd[11] * z * (2.0 * z * z - 3.0 * x * x - 3.0 * y * y) +
               SH_C3[4] * shd[12] * x * (4.0 * z * z - x * x - y * y) + SH_C3[5] * shd[13] * (x * x - y * y) * z +
               SH_C3[6] * shd[14] * x * (x * x - 3.0 * y * y);
    }

    return rgb;
}

struct PrimitiveOutput
{
    [[vk::location(0)]]
    nointerpolation uint splatId : SPLATID;
    [[vk::location(1)]]
    nointerpolation float4 splatCol : COLOR0;
    [[vk::location(2)]]
    nointerpolation float3 splatPosition : POSITION0;
    [[vk::location(3)]]
    nointerpolation float3 splatScale : SCALE0;
    [[vk::location(4)]]
    nointerpolation float4 splatRotation : ROTATION0;
};

struct VertexOutput
{
    float4 position : SV_Position;
};

[numthreads(RASTER_MESH_WORKGROUP_SIZE, 1, 1)]
[outputtopology("triangle")]
[shader("mesh")]
void main(uint3 groupThreadID: SV_GroupThreadID, uint3 groupID: SV_GroupID, uint3 dispatchThreadID: SV_DispatchThreadID,
          out OutputVertices<VertexOutput, MAX_VERTICES> outVerts, out OutputPrimitives<PrimitiveOutput, MAX_PRIMITIVES> outPrims,
          out OutputIndices<uint3, MAX_PRIMITIVES> triangles)
{
    const uint     localIndex      = groupThreadID.x;
    const uint32_t baseIndex       = dispatchThreadID.x;
    const uint     splatCount      = indirectBuffer[0].instanceCount;
    const uint     outputQuadCount = min(RASTER_MESH_WORKGROUP_SIZE, splatCount - groupID.x * RASTER_MESH_WORKGROUP_SIZE);
    if (localIndex == 0)
    {
        SetMeshOutputCounts(outputQuadCount * 4, outputQuadCount * 2);
    }
    // if (baseIndex >= splatCount)
    // {
    //     return;
    // }

    // const int splatIndex           = indicesBuffer[baseIndex];
    // triangles[localIndex * 2 + 0]  = uint3(0, 2, 1) + localIndex * 4;
    // triangles[localIndex * 2 + 1]  = uint3(2, 0, 3) + localIndex * 4;
    // CameraData*    cameraPtr       = (CameraData*) perFrameConstant.cameraBufferDeviceAddress;
    // const float4x4 modelViewMatrix = cameraPtr->viewMatrix;
    // float4         splatColor      = fetchColor(sceneConstant.colorBufferDeviceAddress, splatIndex);

    // const float3 splatCenter = fetchCenter(sceneConstant.positionBufferDeviceAddress, splatIndex);
    // const float4 viewCenter  = mul(float4(splatCenter, 1.0), modelViewMatrix);
    // const float4 clipCenter  = mul(viewCenter, cameraPtr->projMatrix);
    // // // for clip
    // // {
    // // }

    const float2 positions[4] = { float2(-1.0f, -1.0f), float2(1.0f, -1.0f), float2(1.0f, 1.0f), float2(-1.0f, 1.0f) };
    // const float3x3 covariance   = fetchCovariance(sceneConstant.covarianceBufferDeviceAddress, splatIndex);
    // const float3   conv2Dv      = threedgsCovarianceProjection(covariance, viewCenter, float2(1.0), modelViewMatrix);
    // float2         basisVector1, basisVector2;
    // if (!threedgsProjectedExtentBasis(conv2Dv, sqrt8, 1.0, splatColor.a, basisVector1, basisVector2))
    // {
    //     emitDegeneratedQuad(localIndex, outVerts);
    //     return;
    // }

    // const float3 worldViewDirection = normalize(splatCenter - mul(float4(cameraPtr->cameraPosition, 1.0), float4x4(1.0)).xyz);
    // splatColor.xyz += fetchViewDependentRadiance(splatIndex, worldViewDirection);
    // const float3 ndcCenter = clipCenter.xyz / clipCenter.w;
    [unroll]
    for (uint i = 0; i < 4; ++i)
    {
        // const float2 fragPos = positions[i].xy;

        // const float2 ndcOffset                = float2(fragPos.x * basisVector1 + fragPos.y * basisVector2);
        // const float4 quadPos                  = float4(ndcCenter.xy + ndcOffset, ndcCenter.z, 1.0);
        // outVerts[localIndex * 4 + i].position = quadPos;
        outVerts[localIndex * 4 + i].position = float4(positions[i] * 0.003, 0.0, 1.0);
    }
}
