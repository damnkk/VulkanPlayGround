

struct VertexInput
{
    float4 position : SV_Position;
};
struct MeshPrimitiveInput
{
    [[vk::location(0)]]
    nointerpolation float4 inSplatCol : COLOR0;
};

float4 processFragment(float4 inSplatCol, float3 baryCorrd)
{
    const float  sqrt8     = sqrt(8.0);
    const float2 inFragPos = (baryCorrd.x * float2(-1, -1) + baryCorrd.y * float2(1, 1) + baryCorrd.z * float2(-1, 1)) * sqrt8;
    const float  A         = dot(inFragPos, inFragPos);
    // if (A > sqrt8)
    // {
    //     discard;
    // }
    const float opacity = exp(-0.5 * A) * inSplatCol.a;

    return float4(inSplatCol.xyz, opacity);
}
[shader("fragment")]
float4 main(VertexInput vInput, MeshPrimitiveInput pInput, float3 baryCoord: SV_BaryCentrics) : SV_Target0
{
    // return float4(1.0);
    return processFragment(pInput.inSplatCol, baryCoord);
}
