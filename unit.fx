#pragma pack_matrix(row_major)

cbuffer UnitCB : register(b0)
{
    float4x4 matWVP;
    float4x4 matWorld;
    float4 color;
    float4 data; // time, hitFlash, mode, unused
};

struct VS_IN
{
    float3 Pos    : POSITION;
    float3 Normal : NORMAL;
};

struct VS_OUT
{
    float4 Pos    : SV_POSITION;
    float3 Normal : TEXCOORD0;
};

VS_OUT VS_Unit(VS_IN input)
{
    VS_OUT output;
    output.Pos = mul(float4(input.Pos, 1.0), matWVP);
    output.Normal = normalize(mul(float4(input.Normal, 0.0), matWorld).xyz);
    return output;
}

float4 PS_Unit(VS_OUT input) : SV_TARGET
{
    float3 lightDir = normalize(float3(-0.35, 0.85, -0.35));
    float ndl = saturate(dot(normalize(input.Normal), lightDir));
    float3 lit = color.rgb * (0.35 + ndl * 0.75);
    lit = lerp(lit, float3(1.0, 1.0, 1.0), saturate(data.y));
    return float4(lit, color.a);
}
