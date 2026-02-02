
#include "gltf_scene_io.h.slang"
#include "gltf_vertex_access.h.slang"
struct VertexInput
{
    uint vertexId : SV_VertexID;
}

struct VertexOutput
{
    float4 position : SV_Position;
    float3 normal : NORMAL;
    float3 tangent : TANGENT;
    float2 uv : TEXCOORD0;
};

[shader("vertex")]
VertexOutput main(VertexInput vin)
{
    VertexOutput vout;
    vout.uv       = float2((vin.vertexId << 1) & 2, vin.vertexId & 2);
    vout.position = float4(vout.uv * 2.0f - 1.0f, 0.0f, 1.0f);
    return vout;
}

