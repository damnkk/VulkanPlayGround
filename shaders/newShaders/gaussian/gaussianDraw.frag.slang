

struct VertexInput
{
    float4 position : SV_Position;
};
struct MeshPrimitiveInput
{
    [[vk::location(0)]]
    nointerpolation float4 inSplatCol : COLOR0;
};

float4 processFragment(float4 inSplatCol)
{
    return inSplatCol;
}
[shader("fragment")]
float4 main(VertexInput vInput, MeshPrimitiveInput pInput) : SV_Target0
{
    // return float4(1.0);
    return processFragment(pInput.inSplatCol);
}
