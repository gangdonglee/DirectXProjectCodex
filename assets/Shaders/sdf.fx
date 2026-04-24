#pragma pack_matrix(row_major)

cbuffer SDFCB : register(b0)
{
    float4 screenParams;  // width, height, unused, unused
    float4 textColor;
    float4 outlineColor;
    float4 glowColor;
    float4 sdfParams;     // outlineWidth, glowWidth, glowIntensity, unused
};

Texture2D SDFTexture : register(t0);
SamplerState SDFSampler : register(s0);

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

VS_OUT VS_SDF(VS_IN input)
{
    VS_OUT output;
    float2 ndc;
    ndc.x = (input.Pos.x / screenParams.x) * 2.0 - 1.0;
    ndc.y = 1.0 - (input.Pos.y / screenParams.y) * 2.0;
    output.Pos = float4(ndc, 0.0, 1.0);
    output.UV = input.UV;
    return output;
}

float SampleSDF(float2 uv)
{
    return SDFTexture.Sample(SDFSampler, uv).a;
}

float4 PS_SDFSimple(VS_OUT input) : SV_TARGET
{
    float dist = SampleSDF(input.UV);
    float edge = max(fwidth(dist) * 0.75, 0.001);
    float alpha = smoothstep(0.5 - edge, 0.5 + edge, dist);
    return float4(textColor.rgb, alpha * textColor.a);
}

float4 PS_SDFOutline(VS_OUT input) : SV_TARGET
{
    float dist = SampleSDF(input.UV);
    float edge = max(fwidth(dist) * 0.75, 0.001);
    float textAlpha = smoothstep(0.5 - edge, 0.5 + edge, dist);
    float outThresh = 0.5 - sdfParams.x;
    float outerAlpha = smoothstep(outThresh - edge, outThresh + edge, dist);
    float3 color = lerp(outlineColor.rgb, textColor.rgb, textAlpha);
    float alpha = outerAlpha * lerp(outlineColor.a, textColor.a, textAlpha);
    return float4(color, alpha);
}

float4 PS_SDFGlow(VS_OUT input) : SV_TARGET
{
    float dist = SampleSDF(input.UV);
    float edge = max(fwidth(dist) * 0.75, 0.001);
    float textAlpha = smoothstep(0.5 - edge, 0.5 + edge, dist);
    float glowThresh = 0.5 - sdfParams.y;
    float glowAlpha = smoothstep(glowThresh, 0.5, dist) * sdfParams.z;
    glowAlpha = saturate(glowAlpha);
    float3 color = lerp(glowColor.rgb, textColor.rgb, textAlpha);
    float alpha = max(glowAlpha * glowColor.a, textAlpha * textColor.a);
    return float4(color, alpha);
}

float4 PS_SDFOutlineGlow(VS_OUT input) : SV_TARGET
{
    float dist = SampleSDF(input.UV);
    float edge = max(fwidth(dist) * 0.75, 0.001);
    float textAlpha = smoothstep(0.5 - edge, 0.5 + edge, dist);
    float outThresh = 0.5 - sdfParams.x;
    float outerAlpha = smoothstep(outThresh - edge, outThresh + edge, dist);
    float glowThresh = outThresh - sdfParams.y;
    float glowAlpha = smoothstep(glowThresh, outThresh, dist) * sdfParams.z;
    glowAlpha = saturate(glowAlpha);

    float3 color = glowColor.rgb;
    float alpha = glowAlpha * glowColor.a;
    color = lerp(color, outlineColor.rgb, outerAlpha);
    alpha = max(alpha, outerAlpha * outlineColor.a);
    color = lerp(color, textColor.rgb, textAlpha);
    alpha = max(alpha, textAlpha * textColor.a);
    return float4(color, alpha);
}
