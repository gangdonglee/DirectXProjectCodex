// Terrain shader (DX11-migration friendly, no fixed function)

float4x4 matWVP;
float4x4 matWorld;
float3   eyePos;

// Fog (always computed, use fogAmount=0 to disable)
float  fogStart  = 30.0;
float  fogEnd    = 90.0;
float  fogAmount = 1.0;
float3 fogColor  = float3(0.37, 0.47, 0.59);

texture SourceTex;
sampler SourceSamp = sampler_state
{
    Texture   = <SourceTex>;
    MinFilter = Linear;
    MagFilter = Linear;
    MipFilter = None;
    AddressU  = Wrap;
    AddressV  = Wrap;
};

struct VS_IN
{
    float4 Pos    : POSITION;
    float3 Normal : NORMAL;
    float2 UV     : TEXCOORD0;
};

struct VS_OUT
{
    float4 Pos     : POSITION;
    float2 UV      : TEXCOORD0;
    float  FogDist : TEXCOORD1;
};

VS_OUT VS_Terrain(VS_IN input)
{
    VS_OUT output;
    output.Pos     = mul(input.Pos, matWVP);
    float3 wp      = mul(input.Pos, matWorld).xyz;
    output.UV      = input.UV;
    output.FogDist = distance(wp, eyePos);
    return output;
}

float4 PS_Terrain(VS_OUT input) : COLOR0
{
    float4 color = tex2D(SourceSamp, input.UV);
    float f = saturate((input.FogDist - fogStart) / (fogEnd - fogStart)) * fogAmount;
    color.rgb = lerp(color.rgb, fogColor, f);
    return color;
}

technique Tech_Terrain_Solid
{
    pass P0
    {
        VertexShader     = compile vs_3_0 VS_Terrain();
        PixelShader      = compile ps_3_0 PS_Terrain();
        ZEnable          = true;
        ZWriteEnable     = true;
        AlphaBlendEnable = false;
        CullMode         = CCW;
        FillMode         = Solid;
    }
}

technique Tech_Terrain_Wireframe
{
    pass P0
    {
        VertexShader     = compile vs_3_0 VS_Terrain();
        PixelShader      = compile ps_3_0 PS_Terrain();
        ZEnable          = true;
        ZWriteEnable     = true;
        AlphaBlendEnable = false;
        CullMode         = CCW;
        FillMode         = Wireframe;
    }
}
