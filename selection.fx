#pragma pack_matrix(row_major)

cbuffer UnitCB : register(b0)
{
    float4x4 matWVP;
    float4x4 matWorld;
    float4 color;
    float4 data; // time, progress, mode, unused
};

struct VS_OUT
{
    float4 Pos : SV_POSITION;
    float2 UV  : TEXCOORD0;
};

float4 PS_Selection(VS_OUT input) : SV_TARGET
{
    float2 c = float2(0.5, 0.5);
    float d = length(input.UV - c) * 2.0;

    const float ringOuter = 0.92;
    const float ringInner = 0.76;
    const float edgeSoft = 0.05;

    float inner = smoothstep(ringInner - edgeSoft, ringInner, d);
    float outer = 1.0 - smoothstep(ringOuter, ringOuter + edgeSoft, d);
    float mask = inner * outer;

    float pulse = 0.75 + 0.25 * sin(data.x * 3.5);
    float alpha = mask * color.a * pulse;
    clip(alpha - 0.01);
    return float4(color.rgb, alpha);
}
