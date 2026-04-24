// SDF Font Shader
// stb_truetype SDF: onedge_value=128 -> threshold=0.5

float4 textColor;
float4 outlineColor;
float4 glowColor;
float  outlineWidth;   // SDF units (0.0 ~ 0.5)
float  glowWidth;      // SDF units (0.0 ~ 0.5)
float  glowIntensity;

texture SDFTexture;
sampler SDFSampler = sampler_state
{
    Texture   = <SDFTexture>;
    MinFilter = Linear;
    MagFilter = Linear;
    MipFilter = None;
    AddressU  = Clamp;
    AddressV  = Clamp;
};

struct PS_IN
{
    float2 UV : TEXCOORD0;
};

// ===================== Simple =====================
float4 PS_SDFSimple(PS_IN input) : COLOR0
{
    float dist = tex2D(SDFSampler, input.UV).r;
    float edge = fwidth(dist) * 0.75;
    edge = max(edge, 0.001);
    float alpha = smoothstep(0.5 - edge, 0.5 + edge, dist);
    return float4(textColor.rgb, alpha * textColor.a);
}

// ===================== Outline =====================
float4 PS_SDFOutline(PS_IN input) : COLOR0
{
    float dist = tex2D(SDFSampler, input.UV).r;
    float edge = fwidth(dist) * 0.75;
    edge = max(edge, 0.001);

    float textAlpha = smoothstep(0.5 - edge, 0.5 + edge, dist);

    float outThresh = 0.5 - outlineWidth;
    float outerAlpha = smoothstep(outThresh - edge, outThresh + edge, dist);

    float3 color = lerp(outlineColor.rgb, textColor.rgb, textAlpha);
    float  alpha = outerAlpha * lerp(outlineColor.a, textColor.a, textAlpha);

    return float4(color, alpha);
}

// ===================== Glow =====================
float4 PS_SDFGlow(PS_IN input) : COLOR0
{
    float dist = tex2D(SDFSampler, input.UV).r;
    float edge = fwidth(dist) * 0.75;
    edge = max(edge, 0.001);

    float textAlpha = smoothstep(0.5 - edge, 0.5 + edge, dist);

    float glowThresh = 0.5 - glowWidth;
    float glowAlpha  = smoothstep(glowThresh, 0.5, dist) * glowIntensity;
    glowAlpha = saturate(glowAlpha);

    float3 color = lerp(glowColor.rgb, textColor.rgb, textAlpha);
    float  alpha = max(glowAlpha * glowColor.a, textAlpha * textColor.a);

    return float4(color, alpha);
}

// ===================== Outline + Glow =====================
float4 PS_SDFOutlineGlow(PS_IN input) : COLOR0
{
    float dist = tex2D(SDFSampler, input.UV).r;
    float edge = fwidth(dist) * 0.75;
    edge = max(edge, 0.001);

    // Text
    float textAlpha = smoothstep(0.5 - edge, 0.5 + edge, dist);

    // Outline
    float outThresh  = 0.5 - outlineWidth;
    float outerAlpha = smoothstep(outThresh - edge, outThresh + edge, dist);

    // Glow
    float glowThresh = outThresh - glowWidth;
    float glowAlpha  = smoothstep(glowThresh, outThresh, dist) * glowIntensity;
    glowAlpha = saturate(glowAlpha);

    // Composite: glow -> outline -> text
    float3 color = glowColor.rgb;
    float  alpha = glowAlpha * glowColor.a;

    color = lerp(color, outlineColor.rgb, outerAlpha);
    alpha = max(alpha, outerAlpha * outlineColor.a);

    color = lerp(color, textColor.rgb, textAlpha);
    alpha = max(alpha, textAlpha * textColor.a);

    return float4(color, alpha);
}

// ===================== Technique =====================
technique Tech_SDF
{
    pass P0_Simple
    {
        VertexShader     = NULL;
        PixelShader      = compile ps_3_0 PS_SDFSimple();
        AlphaBlendEnable = true;
        SrcBlend         = SrcAlpha;
        DestBlend        = InvSrcAlpha;
        ZEnable          = false;
        ZWriteEnable     = false;
    }

    pass P1_Outline
    {
        VertexShader     = NULL;
        PixelShader      = compile ps_3_0 PS_SDFOutline();
        AlphaBlendEnable = true;
        SrcBlend         = SrcAlpha;
        DestBlend        = InvSrcAlpha;
        ZEnable          = false;
        ZWriteEnable     = false;
    }

    pass P2_Glow
    {
        VertexShader     = NULL;
        PixelShader      = compile ps_3_0 PS_SDFGlow();
        AlphaBlendEnable = true;
        SrcBlend         = SrcAlpha;
        DestBlend        = InvSrcAlpha;
        ZEnable          = false;
        ZWriteEnable     = false;
    }

    pass P3_OutlineGlow
    {
        VertexShader     = NULL;
        PixelShader      = compile ps_3_0 PS_SDFOutlineGlow();
        AlphaBlendEnable = true;
        SrcBlend         = SrcAlpha;
        DestBlend        = InvSrcAlpha;
        ZEnable          = false;
        ZWriteEnable     = false;
    }
}
