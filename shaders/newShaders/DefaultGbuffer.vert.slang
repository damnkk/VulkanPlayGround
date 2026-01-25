
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
VertexOutput main()
{
    VertexOutput vout;
    return vout;
}
