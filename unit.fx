// Unit cube shader (DX11-migration friendly)

float4x4 matWVP;
float4x4 matWorld;
float3   eyePos;

float4 tintColor = float4(1, 1, 1, 1);
float  hitFlash  = 0.0;   // 0 = normal, 1 = fully white

// Fog
float  fogStart  = 30.0;
float  fogEnd    = 90.0;
float  fogAmount = 1.0;
float3 fogColor  = float3(0.37, 0.47, 0.59);

struct VS_IN
{
    float4 Pos    : POSITION;
    float3 Normal : NORMAL;
    float4 Color  : COLOR0;   // ignored (kept for FVF compatibility)
};

struct VS_OUT
{
    float4 Pos     : POSITION;
    float3 Normal  : TEXCOORD0;
    float  FogDist : TEXCOORD1;
};

VS_OUT VS_Unit(VS_IN input)
{
    VS_OUT output;
    output.Pos     = mul(input.Pos, matWVP);
    float3 wp      = mul(input.Pos, matWorld).xyz;
    output.Normal  = mul(input.Normal, (float3x3)matWorld);
    output.FogDist = distance(wp, eyePos);
    return output;
}

float4 PS_Unit(VS_OUT input) : COLOR0
{
    // Simple directional light for shape definition
    float3 L = normalize(float3(0.5, 1.0, -0.3));
    float  ndotl = saturate(dot(normalize(input.Normal), L));
    float3 lit   = tintColor.rgb * (0.45 + 0.55 * ndotl);

    // Hit flash (blend toward white)
    lit = lerp(lit, float3(1, 1, 1), saturate(hitFlash));

    // Fog
    float f = saturate((input.FogDist - fogStart) / (fogEnd - fogStart)) * fogAmount;
    float3 finalColor = lerp(lit, fogColor, f);

    return float4(finalColor, 1.0);
}

technique Tech_Unit
{
    pass P0
    {
        VertexShader     = compile vs_3_0 VS_Unit();
        PixelShader      = compile ps_3_0 PS_Unit();
        ZEnable          = true;
        ZWriteEnable     = true;
        AlphaBlendEnable = false;
        CullMode         = CCW;
    }
}
