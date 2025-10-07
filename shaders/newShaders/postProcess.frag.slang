
struct FragmentInput
{
    float2 uv : TEXCOORD0;
};

struct FragmentOutput
{
    float4 color : SV_Target0;
};

[shader("fragment")]
void main(FragmentInput fragIn, out FragmentOutput fragOut)
{
    fragOut.color = float4(fragIn.uv, 0.0f, 1.0f);
}
