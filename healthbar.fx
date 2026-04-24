// Screen-space health bar shader
// progress: 0.0 (empty) ~ 1.0 (full)

float progress;

struct PS_IN
{
    float2 UV : TEXCOORD0;
};

float3 GetFillColor(float hp)
{
    float3 red    = float3(0.90, 0.20, 0.20);
    float3 yellow = float3(0.95, 0.82, 0.20);
    float3 green  = float3(0.25, 0.85, 0.25);

    if (hp > 0.5)
        return lerp(yellow, green, saturate((hp - 0.5) * 2.0));
    else
        return lerp(red, yellow, saturate(hp * 2.0));
}

float4 PS_HealthBar(PS_IN input) : COLOR0
{
    float2 uv = input.UV;

    // Border frame
    const float borderX = 0.03;
    const float borderY = 0.15;
    bool inBorder = (uv.x < borderX) || (uv.x > 1.0 - borderX) ||
                    (uv.y < borderY) || (uv.y > 1.0 - borderY);
    if (inBorder)
        return float4(0, 0, 0, 0.9);

    // Fill vs empty
    if (uv.x < progress)
    {
        // Add a subtle vertical shading for 3D-ish feel
        float shade = 1.0 - abs(uv.y - 0.5) * 0.5;
        return float4(GetFillColor(progress) * shade, 0.95);
    }
    else
    {
        return float4(0.08, 0.08, 0.08, 0.85);
    }
}

technique Tech_HealthBar
{
    pass P0
    {
        VertexShader     = NULL;
        PixelShader      = compile ps_3_0 PS_HealthBar();
        AlphaBlendEnable = true;
        SrcBlend         = SrcAlpha;
        DestBlend        = InvSrcAlpha;
        ZEnable          = false;
        ZWriteEnable     = false;
        CullMode         = None;
    }
}
