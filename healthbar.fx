#pragma pack_matrix(row_major)

cbuffer HealthBarCB : register(b0)
{
    float4 screenParams; // width, height, unused, unused
    float4 data;         // progress, unused...
};

struct VS_IN
{
    float2 Pos : POSITION;
    float2 UV  : TEXCOORD0;
};

struct VS_OUT
{
    float4 Pos : SV_POSITION;
    float2 UV  : TEXCOORD0;
};

VS_OUT VS_HealthBar(VS_IN input)
{
    VS_OUT output;
    float2 ndc;
    ndc.x = (input.Pos.x / screenParams.x) * 2.0 - 1.0;
    ndc.y = 1.0 - (input.Pos.y / screenParams.y) * 2.0;
    output.Pos = float4(ndc, 0.0, 1.0);
    output.UV = input.UV;
    return output;
}

float3 GetFillColor(float hp)
{
    float3 red = float3(0.90, 0.20, 0.20);
    float3 yellow = float3(0.95, 0.82, 0.20);
    float3 green = float3(0.25, 0.85, 0.25);
    return (hp > 0.5)
        ? lerp(yellow, green, saturate((hp - 0.5) * 2.0))
        : lerp(red, yellow, saturate(hp * 2.0));
}

float4 PS_HealthBar(VS_OUT input) : SV_TARGET
{
    float2 uv = input.UV;
    float progress = saturate(data.x);

    const float borderX = 0.03;
    const float borderY = 0.15;
    bool inBorder = (uv.x < borderX) || (uv.x > 1.0 - borderX) ||
                    (uv.y < borderY) || (uv.y > 1.0 - borderY);
    if (inBorder)
        return float4(0.0, 0.0, 0.0, 0.9);

    if (uv.x < progress)
    {
        float shade = 1.0 - abs(uv.y - 0.5) * 0.5;
        return float4(GetFillColor(progress) * shade, 0.95);
    }
    return float4(0.08, 0.08, 0.08, 0.85);
}
