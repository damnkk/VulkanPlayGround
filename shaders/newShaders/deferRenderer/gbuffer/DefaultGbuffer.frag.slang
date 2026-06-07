struct FragmentInput
{
    float4 position : SV_Position;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD0;
};

struct FragmentOutput
{
    float4 albedo : SV_Target0;
    float4 worldNormal : SV_Target1;
    float4 pbr : SV_Target2;
    float4 emissive : SV_Target3;
    float4 custom : SV_Target4;
    float4 velocity : SV_Target5;
};

[shader("fragment")]
FragmentOutput main(FragmentInput fragIn)
{
    FragmentOutput output;
    output.albedo = float4(1.0, 0.0, 0.0, 1.0);
    output.worldNormal = float4(0.0, 1.0, 0.0, 1.0);
    output.pbr = float4(0.0, 0.0, 1.0, 1.0);
    output.emissive = float4(1.0, 1.0, 0.0, 1.0);
    output.custom = float4(1.0, 0.0, 1.0, 1.0);
    output.velocity = float4(0.0, 1.0, 1.0, 1.0);
    return output;
}
