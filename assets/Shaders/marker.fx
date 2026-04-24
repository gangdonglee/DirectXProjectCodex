#pragma pack_matrix(row_major)

cbuffer UnitCB : register(b0)
{
    float4x4 matWVP;
    float4x4 matWorld;
    float4 color;
    float4 data; // time, progress, mode, unused
};

struct VS_IN
{
    float3 Pos : POSITION;
    float2 UV  : TEXCOORD0;
};

struct VS_OUT
{
    float4 Pos : SV_POSITION;
    float2 UV  : TEXCOORD0;
};

VS_OUT VS_Marker(VS_IN input)
{
    VS_OUT output;
    output.Pos = mul(float4(input.Pos, 1.0), matWVP);
    output.UV = input.UV;
    return output;
}

float4 PS_Marker(VS_OUT input) : SV_TARGET
{
    float2 c = float2(0.5, 0.5);
    float d = length(input.UV - c) * 2.0;
    float progress = saturate(data.y);

    float ringPos = 1.0 - progress;
    float ringEdge = 0.10;
    float ring = 1.0 - saturate(abs(d - ringPos) / ringEdge);
    ring = ring * ring;
    ring *= smoothstep(0.0, 0.3, progress);

    float dotCore = 1.0 - smoothstep(0.12, 0.28, d);
    float pulse = 0.75 + 0.25 * sin(progress * 30.0);
    float centerDot = dotCore * pulse;

    float mask = saturate(ring + centerDot);
    float alpha = mask * progress * color.a;
    clip(alpha - 0.01);
    return float4(color.rgb, alpha);
}
