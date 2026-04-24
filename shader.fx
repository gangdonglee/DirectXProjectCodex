float4   textColor      = float4(1, 1, 1, 1);
float4   outlineColor   = float4(0, 0, 0, 1);
float    texWidth       = 0.0f;
float    texHeight      = 0.0f;
float    outlineWidth   = 0;

// Glow params
float4   glowColor      = float4(0.4, 0.7, 1.0, 1.0);
float    glowWidth      = 5.0;
float    glowIntensity  = 0.8;

// Dissolve params
float    progress       = 0.0;
float    edgeWidth      = 0.1;
float2   noiseScale     = float2(1.0, 1.0);
float4   edgeColor      = float4(1.0, 0.8, 0.2, 1.0);
float    edgeIntensity  = 1.5;
float2   quadMin        = float2(0, 0);
float2   quadMax        = float2(1, 1);

texture SourceTex;
texture NoiseTex;

sampler SourceSamp = sampler_state
{
    Texture   = <SourceTex>;
    MinFilter = Linear;
    MagFilter = Linear;
    AddressU  = Clamp;
    AddressV  = Clamp;
};

sampler NoiseSamp = sampler_state
{
    Texture   = <NoiseTex>;
    MinFilter = Linear;
    MagFilter = Linear;
    AddressU  = Wrap;
    AddressV  = Wrap;
};

struct PS_IN
{
    float2 UV : TEXCOORD0;
};

// ===================== PS: Simple =====================
float4 PS_Simple(PS_IN input) : COLOR0
{
    float4 tex = tex2D(SourceSamp, input.UV);
    return tex * textColor;
}

// ===================== PS: Outline =====================
float4 PS_Outline(PS_IN input) : COLOR0
{
    float2 uv = input.UV;
    float4 center = tex2D(SourceSamp, uv);
    float sz = outlineWidth;
    float2 tx = float2(1.0 / texWidth, 1.0 / texHeight);

    float hR = max(tex2D(SourceSamp, uv + float2( sz * 0.5 * tx.x, 0)).a,
                   tex2D(SourceSamp, uv + float2( sz       * tx.x, 0)).a);
    float hL = max(tex2D(SourceSamp, uv + float2(-sz * 0.5 * tx.x, 0)).a,
                   tex2D(SourceSamp, uv + float2(-sz       * tx.x, 0)).a);
    float hD = max(tex2D(SourceSamp, uv + float2(0,  sz * 0.5 * tx.y)).a,
                   tex2D(SourceSamp, uv + float2(0,  sz       * tx.y)).a);
    float hU = max(tex2D(SourceSamp, uv + float2(0, -sz * 0.5 * tx.y)).a,
                   tex2D(SourceSamp, uv + float2(0, -sz       * tx.y)).a);

    float encMin = min(min(hR, hL), min(hD, hU));
    float encMask = 1.0 - step(0.5, encMin) * step(center.a, 0.5);

    float ma = 0;
    float d1 = sz * 0.33;
    float d2 = sz * 0.66;
    float d3 = sz;

    ma = max(ma, tex2D(SourceSamp, uv + float2( d1 * tx.x, 0)).a);
    ma = max(ma, tex2D(SourceSamp, uv + float2( d2 * tx.x, 0)).a);
    ma = max(ma, tex2D(SourceSamp, uv + float2( d3 * tx.x, 0)).a);
    ma = max(ma, tex2D(SourceSamp, uv + float2(-d1 * tx.x, 0)).a);
    ma = max(ma, tex2D(SourceSamp, uv + float2(-d2 * tx.x, 0)).a);
    ma = max(ma, tex2D(SourceSamp, uv + float2(-d3 * tx.x, 0)).a);
    ma = max(ma, tex2D(SourceSamp, uv + float2(0,  d1 * tx.y)).a);
    ma = max(ma, tex2D(SourceSamp, uv + float2(0,  d2 * tx.y)).a);
    ma = max(ma, tex2D(SourceSamp, uv + float2(0,  d3 * tx.y)).a);
    ma = max(ma, tex2D(SourceSamp, uv + float2(0, -d1 * tx.y)).a);
    ma = max(ma, tex2D(SourceSamp, uv + float2(0, -d2 * tx.y)).a);
    ma = max(ma, tex2D(SourceSamp, uv + float2(0, -d3 * tx.y)).a);
    ma = max(ma, tex2D(SourceSamp, uv + float2( d1*0.707*tx.x,  d1*0.707*tx.y)).a);
    ma = max(ma, tex2D(SourceSamp, uv + float2( d2*0.707*tx.x,  d2*0.707*tx.y)).a);
    ma = max(ma, tex2D(SourceSamp, uv + float2( d3*0.707*tx.x,  d3*0.707*tx.y)).a);
    ma = max(ma, tex2D(SourceSamp, uv + float2(-d1*0.707*tx.x, -d1*0.707*tx.y)).a);
    ma = max(ma, tex2D(SourceSamp, uv + float2(-d2*0.707*tx.x, -d2*0.707*tx.y)).a);
    ma = max(ma, tex2D(SourceSamp, uv + float2(-d3*0.707*tx.x, -d3*0.707*tx.y)).a);
    ma = max(ma, tex2D(SourceSamp, uv + float2(-d1*0.707*tx.x,  d1*0.707*tx.y)).a);
    ma = max(ma, tex2D(SourceSamp, uv + float2(-d2*0.707*tx.x,  d2*0.707*tx.y)).a);
    ma = max(ma, tex2D(SourceSamp, uv + float2(-d3*0.707*tx.x,  d3*0.707*tx.y)).a);
    ma = max(ma, tex2D(SourceSamp, uv + float2( d1*0.707*tx.x, -d1*0.707*tx.y)).a);
    ma = max(ma, tex2D(SourceSamp, uv + float2( d2*0.707*tx.x, -d2*0.707*tx.y)).a);
    ma = max(ma, tex2D(SourceSamp, uv + float2( d3*0.707*tx.x, -d3*0.707*tx.y)).a);

    float outAlpha = smoothstep(0.15, 0.6, ma) * encMask;

    float outA = outAlpha * outlineColor.a;
    float txtA = center.a * textColor.a;

    float  a   = outA * (1.0 - txtA) + txtA;
    float3 col = (outlineColor.rgb * outA * (1.0 - txtA) + textColor.rgb * txtA) / max(a, 0.001);

    return float4(col, a);
}

// ===================== PS: Glow =====================
// Simple text + gaussian blur glow behind
float4 PS_Glow(PS_IN input) : COLOR0
{
    float2 uv = input.UV;
    float2 tx = float2(1.0 / texWidth, 1.0 / texHeight);
    float4 center = tex2D(SourceSamp, uv);

    float glowAlpha = 0.0;
    float gw = glowWidth;

    for (int y = -5; y <= 5; y++)
    {
        for (int x = -5; x <= 5; x++)
        {
            float2 off = float2(x, y) * tx;
            float dist = length(float2(x, y));
            float valid = (dist <= gw) ? 1.0 : 0.0;
            float weight = exp(-(dist * dist) / (2.0 * gw * gw)) * valid;
            glowAlpha += tex2D(SourceSamp, uv + off).a * weight;
        }
    }
    glowAlpha = saturate(glowAlpha);

    float glowA = glowAlpha * glowColor.a * glowIntensity;
    float txtA  = center.a * textColor.a;

    float  a   = glowA * (1.0 - txtA) + txtA;
    float3 col = (glowColor.rgb * glowA * (1.0 - txtA) + textColor.rgb * txtA) / max(a, 0.001);

    return float4(col, a);
}

// ===================== PS: Outline + Glow combined =====================
float4 PS_OutlineGlow(PS_IN input) : COLOR0
{
    float2 uv = input.UV;
    float4 center = tex2D(SourceSamp, uv);
    float sz = outlineWidth;
    float2 tx = float2(1.0 / texWidth, 1.0 / texHeight);

    // --- Glow (11x11 gaussian) ---
    float glowAlpha = 0.0;
    float gw = glowWidth;
    for (int y = -5; y <= 5; y++)
    {
        for (int x = -5; x <= 5; x++)
        {
            float2 off = float2(x, y) * tx;
            float dist = length(float2(x, y));
            float valid = (dist <= gw) ? 1.0 : 0.0;
            float weight = exp(-(dist * dist) / (2.0 * gw * gw)) * valid;
            glowAlpha += tex2D(SourceSamp, uv + off).a * weight;
        }
    }
    glowAlpha = saturate(glowAlpha);
    float4 glowResult = float4(glowColor.rgb, glowAlpha * glowColor.a * glowIntensity);

    // --- Outline dilation ---
    float encMin = min(
        min(max(tex2D(SourceSamp, uv + float2( sz*0.5*tx.x, 0)).a, tex2D(SourceSamp, uv + float2( sz*tx.x, 0)).a),
            max(tex2D(SourceSamp, uv + float2(-sz*0.5*tx.x, 0)).a, tex2D(SourceSamp, uv + float2(-sz*tx.x, 0)).a)),
        min(max(tex2D(SourceSamp, uv + float2(0,  sz*0.5*tx.y)).a, tex2D(SourceSamp, uv + float2(0,  sz*tx.y)).a),
            max(tex2D(SourceSamp, uv + float2(0, -sz*0.5*tx.y)).a, tex2D(SourceSamp, uv + float2(0, -sz*tx.y)).a)));
    float encMask = 1.0 - step(0.5, encMin) * step(center.a, 0.5);

    float ma = 0;
    float d1 = sz * 0.33; float d2 = sz * 0.66; float d3 = sz;
    ma = max(ma, tex2D(SourceSamp, uv + float2( d1*tx.x, 0)).a);
    ma = max(ma, tex2D(SourceSamp, uv + float2( d2*tx.x, 0)).a);
    ma = max(ma, tex2D(SourceSamp, uv + float2( d3*tx.x, 0)).a);
    ma = max(ma, tex2D(SourceSamp, uv + float2(-d1*tx.x, 0)).a);
    ma = max(ma, tex2D(SourceSamp, uv + float2(-d2*tx.x, 0)).a);
    ma = max(ma, tex2D(SourceSamp, uv + float2(-d3*tx.x, 0)).a);
    ma = max(ma, tex2D(SourceSamp, uv + float2(0,  d1*tx.y)).a);
    ma = max(ma, tex2D(SourceSamp, uv + float2(0,  d2*tx.y)).a);
    ma = max(ma, tex2D(SourceSamp, uv + float2(0,  d3*tx.y)).a);
    ma = max(ma, tex2D(SourceSamp, uv + float2(0, -d1*tx.y)).a);
    ma = max(ma, tex2D(SourceSamp, uv + float2(0, -d2*tx.y)).a);
    ma = max(ma, tex2D(SourceSamp, uv + float2(0, -d3*tx.y)).a);
    ma = max(ma, tex2D(SourceSamp, uv + float2( d1*0.707*tx.x,  d1*0.707*tx.y)).a);
    ma = max(ma, tex2D(SourceSamp, uv + float2( d2*0.707*tx.x,  d2*0.707*tx.y)).a);
    ma = max(ma, tex2D(SourceSamp, uv + float2( d3*0.707*tx.x,  d3*0.707*tx.y)).a);
    ma = max(ma, tex2D(SourceSamp, uv + float2(-d1*0.707*tx.x, -d1*0.707*tx.y)).a);
    ma = max(ma, tex2D(SourceSamp, uv + float2(-d2*0.707*tx.x, -d2*0.707*tx.y)).a);
    ma = max(ma, tex2D(SourceSamp, uv + float2(-d3*0.707*tx.x, -d3*0.707*tx.y)).a);
    ma = max(ma, tex2D(SourceSamp, uv + float2(-d1*0.707*tx.x,  d1*0.707*tx.y)).a);
    ma = max(ma, tex2D(SourceSamp, uv + float2(-d2*0.707*tx.x,  d2*0.707*tx.y)).a);
    ma = max(ma, tex2D(SourceSamp, uv + float2(-d3*0.707*tx.x,  d3*0.707*tx.y)).a);
    ma = max(ma, tex2D(SourceSamp, uv + float2( d1*0.707*tx.x, -d1*0.707*tx.y)).a);
    ma = max(ma, tex2D(SourceSamp, uv + float2( d2*0.707*tx.x, -d2*0.707*tx.y)).a);
    ma = max(ma, tex2D(SourceSamp, uv + float2( d3*0.707*tx.x, -d3*0.707*tx.y)).a);

    float outAlpha = smoothstep(0.15, 0.6, ma) * encMask;
    float outA = outAlpha * outlineColor.a;
    float txtA = center.a * textColor.a;

    float3 olCol = outlineColor.rgb * outA * (1.0 - txtA) + textColor.rgb * txtA;
    float  olA   = outA * (1.0 - txtA) + txtA;
    float4 outlineResult = float4(olCol, olA);

    // --- Composite: glow behind, outline+text on top ---
    float4 combined = glowResult;
    combined.rgb = lerp(combined.rgb, outlineResult.rgb, outlineResult.a);
    combined.a = max(combined.a, outlineResult.a);

    return combined;
}

// ===================== PS: Dissolve =====================
// Noise-based dissolve with glowing edge (matches phase.fx ps_dissolve)
float4 PS_Dissolve(PS_IN input) : COLOR0
{
    float2 uv = input.UV;
    float4 src = tex2D(SourceSamp, uv);
    float2 localUV = (uv - quadMin) / (quadMax - quadMin);
    float noise = tex2D(NoiseSamp, localUV).r;

    float edge = max(0.0001, edgeWidth);
    float dissolveEdge  = progress - edge;
    float dissolveEdge2 = progress + edge;

    // smoothstep inline: dissolve alpha
    float tA = saturate((noise - dissolveEdge) / (dissolveEdge2 - dissolveEdge));
    float alpha = tA * tA * (3.0 - 2.0 * tA);

    // smoothstep inline: glow mask
    float tG1 = saturate((noise - dissolveEdge) / (dissolveEdge2 - dissolveEdge));
    float g1 = tG1 * tG1 * (3.0 - 2.0 * tG1);
    float tG2 = saturate((noise - dissolveEdge2) / (edge));
    float g2 = tG2 * tG2 * (3.0 - 2.0 * tG2);
    float glowMask = g1 * (1.0 - g2);
    float glow = saturate(glowMask * edgeIntensity);

    // text visible where noise > progress threshold
    float visible = 1.0 - alpha;
    float4 color = src * textColor;
    color.a = src.a * visible;

    // edge glow on dissolve border
    color.rgb = lerp(color.rgb, edgeColor.rgb, glow * src.a * 0.7);
    color.rgb += edgeColor.rgb * glow * src.a * edgeIntensity;
    color.a = max(color.a, glow * src.a * edgeColor.a);

    return color;
}

// ===================== PS: Combined (Glow + Outline + Text + Dissolve) =====================
float4 PS_Combined(PS_IN input) : COLOR0
{
    float2 uv = input.UV;
    float2 tx = float2(1.0 / texWidth, 1.0 / texHeight);

    // --- 1. Glow (11x11 gaussian) ---
    float glowAlpha = 0.0;
    float gw = glowWidth;
    for (int gy = -5; gy <= 5; gy++)
    {
        for (int gx = -5; gx <= 5; gx++)
        {
            float2 off = float2(gx, gy) * tx;
            float dist = length(float2(gx, gy));
            float valid = (dist <= gw) ? 1.0 : 0.0;
            float weight = exp(-(dist * dist) / (2.0 * gw * gw)) * valid;
            glowAlpha += tex2D(SourceSamp, uv + off).a * weight;
        }
    }
    glowAlpha = saturate(glowAlpha);
    float4 glowResult = float4(glowColor.rgb, glowAlpha * glowColor.a * glowIntensity);

    // --- 2. Outline (16-direction sampling) ---
    float4 outlineResult = float4(0, 0, 0, 0);
    float sz = outlineWidth;
    float angleStep = 3.14159265 * 2.0 / 16.0;

    for (int i = 0; i < 16; i++)
    {
        float angle = angleStep * i;
        float2 off = float2(cos(angle), sin(angle)) * sz * tx;
        float4 s = tex2D(SourceSamp, uv + off);
        outlineResult.rgb = outlineResult.rgb * (1.0 - s.a * outlineColor.a) +
                           outlineColor.rgb * s.a * outlineColor.a;
        outlineResult.a = outlineResult.a + s.a * outlineColor.a * (1.0 - outlineResult.a);
    }

    // --- 3. Text on top of outline ---
    float4 textSample = tex2D(SourceSamp, uv);
    outlineResult.rgb = outlineResult.rgb * (1.0 - textSample.a * textColor.a) +
                       textColor.rgb * textSample.a * textColor.a;
    outlineResult.a = outlineResult.a + textSample.a * textColor.a * (1.0 - outlineResult.a);

    // un-premultiply outline+text
    outlineResult.rgb = outlineResult.rgb / max(outlineResult.a, 0.001);

    // --- 4. Glow + Outline+Text composite ---
    float4 combined = glowResult;
    combined.rgb = lerp(combined.rgb, outlineResult.rgb, outlineResult.a);
    combined.a = max(combined.a, outlineResult.a);

    // --- 5. Dissolve ---
    float2 localUV = (uv - quadMin) / (quadMax - quadMin);
    float noise = tex2D(NoiseSamp, localUV).r;

    float edge = max(0.0001, edgeWidth);
    float dissolveEdge  = progress - edge;
    float dissolveEdge2 = progress + edge;

    float tA = saturate((noise - dissolveEdge) / (dissolveEdge2 - dissolveEdge));
    float dAlpha = tA * tA * (3.0 - 2.0 * tA);

    float tG1 = saturate((noise - dissolveEdge) / (dissolveEdge2 - dissolveEdge));
    float g1 = tG1 * tG1 * (3.0 - 2.0 * tG1);
    float tG2 = saturate((noise - dissolveEdge2) / (edge));
    float g2 = tG2 * tG2 * (3.0 - 2.0 * tG2);
    float dGlowMask = g1 * (1.0 - g2);
    float dGlow = saturate(dGlowMask * edgeIntensity);

    float visible = 1.0 - dAlpha;
    float4 color = combined;
    color.a = combined.a * visible;

    color.rgb = lerp(color.rgb, edgeColor.rgb, dGlow * combined.a * 0.7);
    color.rgb += edgeColor.rgb * dGlow * combined.a * edgeIntensity;
    color.a = max(color.a, dGlow * combined.a * edgeColor.a);

    return color;
}

// ===================== Techniques =====================

technique Tech_TextOutline
{
    pass P0_Outline
    {
        VertexShader     = NULL;
        PixelShader      = compile ps_3_0 PS_Outline();
        AlphaBlendEnable = true;
        SrcBlend         = SrcAlpha;
        DestBlend        = InvSrcAlpha;
        ZEnable          = false;
        ZWriteEnable     = false;
    }

    pass P1_Text
    {
        VertexShader     = NULL;
        PixelShader      = compile ps_2_0 PS_Simple();
        AlphaBlendEnable = true;
        SrcBlend         = SrcAlpha;
        DestBlend        = InvSrcAlpha;
        ZEnable          = false;
        ZWriteEnable     = false;
    }

    pass P2_Glow
    {
        VertexShader     = NULL;
        PixelShader      = compile ps_3_0 PS_Glow();
        AlphaBlendEnable = true;
        SrcBlend         = SrcAlpha;
        DestBlend        = InvSrcAlpha;
        ZEnable          = false;
        ZWriteEnable     = false;
    }

    pass P3_OutlineAndGlow
    {
        VertexShader     = NULL;
        PixelShader      = compile ps_3_0 PS_OutlineGlow();
        AlphaBlendEnable = true;
        SrcBlend         = SrcAlpha;
        DestBlend        = InvSrcAlpha;
        ZEnable          = false;
        ZWriteEnable     = false;
    }

    pass P4_Dissolve
    {
        VertexShader     = NULL;
        PixelShader      = compile ps_3_0 PS_Dissolve();
        AlphaBlendEnable = true;
        SrcBlend         = SrcAlpha;
        DestBlend        = InvSrcAlpha;
        ZEnable          = false;
        ZWriteEnable     = false;
    }

    pass P5_Combined
    {
        VertexShader     = NULL;
        PixelShader      = compile ps_3_0 PS_Combined();
        AlphaBlendEnable = true;
        SrcBlend         = SrcAlpha;
        DestBlend        = InvSrcAlpha;
        ZEnable          = false;
        ZWriteEnable     = false;
    }
}
