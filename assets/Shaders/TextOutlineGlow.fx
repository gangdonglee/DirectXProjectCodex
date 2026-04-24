// ===============================================
// Text UI Outline Shader (2-Pass)
// ===============================================
float outlineWidth = 0.0f;
float4 outlineColor = float4(1, 0, 0, 1);
float texWidth = 0.0f;
float texHeight = 0.0f;

float glowWidth = 5.0f;
float glowIntensity = 0.8f;
float4 glowColor = float4(0.4, 0.7, 0.1, 1.0);
float4 textColor = float4(0.4, 0.7, 0.1, 1.0);

float glowFalloff = 1.5f;
float textScale = 1.05f;
float textThickness = 1.5f;

texture SourceTex;
sampler SourceSamp = sampler_state
{
    Texture = <SourceTex>;
    MinFilter = LINEAR;
    MagFilter = LINEAR;
    MipFilter = POINT;
    AddressU = CLAMP;
    AddressV = CLAMP;
};

struct VS_IN
{
    float4 pos : POSITION;
    float2 uv : TEXCOORD0;
};

struct VS_OUT
{
    float4 pos : POSITION;
    float2 uv : TEXCOORD0;
};

VS_OUT vs_passthrough(VS_IN vin)
{
    VS_OUT vout;
    vout.pos = vin.pos;
    vout.uv = vin.uv;
    return vout;
}

float4 PS_Glow(float2 texCoord : TEXCOORD0) : COLOR0
{
    float2 texelSize = float2(1.0 / texWidth, 1.0 / texHeight);
    float glowAlpha = 0.0f;

    for (int y = -5; y <= 5; y++)
    {
        for (int x = -5; x <= 5; x++)
        {
            float2 offset = float2(x, y) * texelSize;
            float dist = length(float2(x, y));
            float validSample = (dist <= glowWidth) ? 1.0f : 0.0f;
            float weight = exp(-(dist * dist) / (2.0f * glowWidth * glowWidth)) * validSample;
            float sampleAlpha = tex2D(SourceSamp, texCoord + offset).a;
            glowAlpha += sampleAlpha * weight;
        }
    }
    glowAlpha = saturate(glowAlpha);
    return float4(glowColor.rgb, glowAlpha * glowColor.a * glowIntensity);
}

float4 PS_OutlineStacked(float2 texCoord : TEXCOORD0) : COLOR0
{
    float2 offsets[16];
    float angleStep = 3.14159265 * 2.0 / 16.0;
    float off = outlineWidth;
    float4 TextColor = (1, 1, 1, 1);
    float2 pixelSize = float2(1.0 / texWidth, 1.0 / texHeight);

    for (int i = 0; i < 16; i++)
    {
        float angle = angleStep * i;
        offsets[i] = float2(cos(angle), sin(angle)) * off * pixelSize;
    }

    float4 result = float4(0, 0, 0, 0);

    for (int j = 0; j < 16; j++)
    {
        float4 s = tex2D(SourceSamp, texCoord + offsets[j]);
        result.rgb = result.rgb * (1.0 - s.a * outlineColor.a) +
                     outlineColor.rgb * s.a * outlineColor.a;
        result.a = result.a + s.a * outlineColor.a * (1.0 - result.a);
    }

    float4 textSample = tex2D(SourceSamp, texCoord);
    result.rgb = result.rgb * (1.0 - textSample.a * TextColor.a) +
                 TextColor.rgb * textSample.a * TextColor.a;
    result.a = result.a + textSample.a * TextColor.a * (1.0 - result.a);

    return result;
}

float4 PS_Combined(float2 texCoord : TEXCOORD0) : COLOR0
{
    float2 texelSize = float2(1.0 / texWidth, 1.0 / texHeight);
    float2 pixelSize = texelSize;

    // 1. Glow
    float glowAlpha = 0.0f;
    for (int gy = -5; gy <= 5; gy++)
    {
        for (int gx = -5; gx <= 5; gx++)
        {
            float2 offset = float2(gx, gy) * texelSize;
            float dist = length(float2(gx, gy));
            float validSample = (dist <= glowWidth) ? 1.0f : 0.0f;
            float weight = exp(-(dist * dist) / (2.0f * glowWidth * glowWidth)) * validSample;
            float sampleAlpha = tex2D(SourceSamp, texCoord + offset).a;
            glowAlpha += sampleAlpha * weight;
        }
    }
    glowAlpha = saturate(glowAlpha);
    float4 glowResult = float4(glowColor.rgb, glowAlpha * glowColor.a * glowIntensity);

    // 2. Outline + Text
    float2 offsets[16];
    float angleStep = 3.14159265 * 2.0 / 16.0;
    float outOffset = outlineWidth;

    for (int i = 0; i < 16; i++)
    {
        float angle = angleStep * i;
        offsets[i] = float2(cos(angle), sin(angle)) * outOffset * pixelSize;
    }

    float4 outlineResult = float4(0, 0, 0, 0);

    for (int j = 0; j < 16; j++)
    {
        float4 s = tex2D(SourceSamp, texCoord + offsets[j]);
        outlineResult.rgb = outlineResult.rgb * (1.0 - s.a * outlineColor.a) +
                           outlineColor.rgb * s.a * outlineColor.a;
        outlineResult.a = outlineResult.a + s.a * outlineColor.a * (1.0 - outlineResult.a);
    }

    float4 textSample = tex2D(SourceSamp, texCoord);
    outlineResult.rgb = outlineResult.rgb * (1.0 - textSample.a * textColor.a) +
                       textColor.rgb * textSample.a * textColor.a;
    outlineResult.a = outlineResult.a + textSample.a * textColor.a * (1.0 - outlineResult.a);

    // 3. Glow + Outline+Text composite
    float4 combined = glowResult;
    combined.rgb = lerp(combined.rgb, outlineResult.rgb, outlineResult.a);
    combined.a = max(combined.a, outlineResult.a);

    return combined;
}

float4 ps_text(VS_OUT IN) : COLOR
{
    return tex2D(SourceSamp, IN.uv);
}

technique Tech_TextOutline
{
    pass P0_Outline
    {
        AlphaBlendEnable = true;
        SrcBlend = SrcAlpha;
        DestBlend = InvSrcAlpha;
        VertexShader = compile vs_3_0 vs_passthrough();
        PixelShader = compile ps_3_0 PS_Glow();
    }

    pass P1_Text
    {
        AlphaBlendEnable = true;
        SrcBlend = SrcAlpha;
        DestBlend = InvSrcAlpha;
        VertexShader = compile vs_3_0 vs_passthrough();
        PixelShader = compile ps_3_0 ps_text();
    }

    pass P2_OutlineAndGlow
    {
        VertexShader = compile vs_3_0 vs_passthrough();
        PixelShader = compile ps_3_0 PS_Combined();
        AlphaBlendEnable = true;
        SrcBlend = SrcAlpha;
        DestBlend = InvSrcAlpha;
    }
}
