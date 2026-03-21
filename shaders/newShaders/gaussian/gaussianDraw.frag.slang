

struct VertexInput
{
    float4 position : SV_Position;
};
struct PrimitiveInput
{
    [[vk::location(0)]]
    float4 outSplatCol : COLOR0;
};
[shader("fragment")]
float4 main(VertexInput vInput, PrimitiveInput pInput) : SV_Target0
{
    return float4(1.0, 1.0, 1.0, 1.0);
}
