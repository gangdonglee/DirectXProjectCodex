#pragma pack_matrix(row_major)

cbuffer TerrainCB : register(b0)
{
    float4x4 matWVP;
    float4x4 matWorld;
    float4 eyePos;
    float4 fogColor;
    float4 fogParams; // start, end, amount, unused
};

Texture2D SourceTex : register(t0);
SamplerState Samp : register(s0);

struct VS_IN
{
    float3 Pos    : POSITION;
    float3 Normal : NORMAL;
    float2 UV     : TEXCOORD0;
};

struct VS_OUT
{
    float4 Pos      : SV_POSITION;
    float3 WorldPos : TEXCOORD0;
    float3 Normal   : TEXCOORD1;
    float2 UV       : TEXCOORD2;
};

VS_OUT VS_Terrain(VS_IN input)
{
    VS_OUT output;
    output.Pos = mul(float4(input.Pos, 1.0), matWVP);
    output.WorldPos = mul(float4(input.Pos, 1.0), matWorld).xyz;
    output.Normal = normalize(mul(float4(input.Normal, 0.0), matWorld).xyz);
    output.UV = input.UV;
    return output;
}

float4 PS_Terrain(VS_OUT input) : SV_TARGET
{
    float3 base = SourceTex.Sample(Samp, input.UV).rgb;
    float3 lightDir = normalize(float3(-0.35, 0.9, -0.25));
    float ndl = saturate(dot(normalize(input.Normal), lightDir));
    float3 color = base * (0.45 + ndl * 0.65);

    float dist = distance(eyePos.xyz, input.WorldPos);
    float fog = saturate((dist - fogParams.x) / max(0.001, fogParams.y - fogParams.x)) * fogParams.z;
    color = lerp(color, fogColor.rgb, fog);

    return float4(color, 1.0);
}
