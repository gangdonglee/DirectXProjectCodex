// ======================================================
// Dissolve / Phase Out Effect
// 글자가 노이즈 패턴을 따라 사라지며 Glow가 남는 효과
// ======================================================

// [파라미터들]
float progress = 0.0f; // 0=원본 100% 보임, 1=완전히 사라짐
float edgeWidth = 0.1f; // 경계선 부드러운 영역 (0~0.3 정도 추천)
float2 noiseScale = float2(1.0, 1.0); // 노이즈 타일링
float4 edgeColor = float4(1, 1.0, 1.0, 1); // 경계선 불빛 (주황색)
float edgeIntensity = 1.5f; // 불빛 강도

// 공통 파라미터
float texWidth = 1024.0f;
float texHeight = 768.0f;
float4 textColor = float4(1, 1, 1, 1);

// Outline 파라미터
float outlineWidth = 0.0f;
float4 outlineColor = float4(1, 0, 0, 1);

// Glow 파라미터
float glowWidth = 5.0f;
float glowIntensity = 0.8f;
float4 glowColor = float4(0.4, 0.7, 0.1, 1.0);



// [텍스처 슬롯]
texture SourceTex; // 원본 글자 텍스처
texture NoiseTex; // 노이즈 텍스처 (grayscale)

sampler SourceSamp = sampler_state
{
    Texture = <SourceTex>;
    MinFilter = LINEAR;
    MagFilter = LINEAR;
    MipFilter = POINT;
    AddressU = CLAMP;
    AddressV = CLAMP;
};
sampler NoiseSamp = sampler_state
{
    Texture = <NoiseTex>;
    MinFilter = LINEAR;
    MagFilter = LINEAR;
    MipFilter = POINT;
    AddressU = WRAP;
    AddressV = WRAP;
};

// [정점 입력/출력 구조체]
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

// 단순 패스 스루 정점 셰이더
VS_OUT vs_passthrough(VS_IN vin)
{
    VS_OUT vout;
    vout.pos = vin.pos;
    vout.uv = vin.uv;
    return vout;
}

// HLSL smoothstep 구현 (DX9 ps_3_0 호환용)
float smoothstep_custom(float edge0, float edge1, float x)
{
    float t = saturate((x - edge0) / (edge1 - edge0));
    return t * t * (3.0 - 2.0 * t);
}

// 픽셀 셰이더
float4 ps_dissolve(VS_OUT IN) : COLOR
{
    float2 uv = IN.uv;
    float4 src = tex2D(SourceSamp, uv);
    float noise = tex2D(NoiseSamp, uv * noiseScale).r;
    
    if (src.a < 0.01f)
        discard;
    
    float edge = max(0.0001, edgeWidth);
    
    // 1. 디졸브 경계 계산 (더 넓은 범위)
    float dissolveEdge = progress - edge;
    float dissolveEdge2 = progress + edge;
    
    // 2. Glow 경계 계산 (디졸브보다 앞서서)
    float glowEdge1 = progress - edge; // 더 넓게
    float glowEdge2 = progress + edge;
    
    // 3. 본체 알파
    float alpha = smoothstep_custom(dissolveEdge, dissolveEdge2, noise);
    
    // 4. Glow 강도 (사라지기 직전이 가장 밝음)
    float glowMask = smoothstep_custom(glowEdge1, glowEdge2, noise)
                   * (1.0 - smoothstep_custom(dissolveEdge2, dissolveEdge2 + edge, noise));
    
    float glow = saturate(glowMask * edgeIntensity); // 강도 증가
    
    // 5. 최종 색상 합성
    float4 color = src;
    color.rgb = lerp(src.rgb, edgeColor.rgb, glow * 0.7); // 원본과 glow 블렌딩
    color.rgb += edgeColor.rgb * glow * edgeIntensity; // 추가 발광
    
    // 6. 알파 처리
    color.a = max(src.a * (1.0 - alpha), glow * edgeColor.a);
    
    return color;
}

// Pass 4: 통합 효과 (Glow + Outline + Text + Dissolve 한번에)
float4 PS_Combined(float2 texCoord : TEXCOORD0) : COLOR0
{
    float2 texelSize = float2(1.0 / texWidth, 1.0 / texHeight);
    float2 pixelSize = texelSize;
    
    // 1. Glow 계산
    float glowAlpha = 0.0f;
    for (int gy = -5; gy <= 5; gy++)
    {
        for (int gx = -5; gx <= 5; gx++)
        {
            float2 offset = float2(gx, gy) * texelSize;
            float distance = length(float2(gx, gy));
            float validSample = (distance <= glowWidth) ? 1.0f : 0.0f;
            float weight = exp(-(distance * distance) / (2.0f * glowWidth * glowWidth)) * validSample;
            float sampleAlpha = tex2D(SourceSamp, texCoord + offset).a;
            glowAlpha += sampleAlpha * weight;
        }
    }
    glowAlpha = saturate(glowAlpha);
    float4 glowResult = float4(glowColor.rgb, glowAlpha * glowColor.a * glowIntensity);
    
    // 2. Outline + Text 계산
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
        float4 sample = tex2D(SourceSamp, texCoord + offsets[j]);
        outlineResult.rgb = outlineResult.rgb * (1.0 - sample.a * outlineColor.a) +
                           outlineColor.rgb * sample.a * outlineColor.a;
        outlineResult.a = outlineResult.a + sample.a * outlineColor.a * (1.0 - outlineResult.a);
    }
    
    float4 textSample = tex2D(SourceSamp, texCoord);
    outlineResult.rgb = outlineResult.rgb * (1.0 - textSample.a * textColor.a) +
                       textColor.rgb * textSample.a * textColor.a;
    outlineResult.a = outlineResult.a + textSample.a * textColor.a * (1.0 - outlineResult.a);
    
    // 3. Glow와 Outline+Text 합성
    float4 combined = glowResult;
    combined.rgb = lerp(combined.rgb, outlineResult.rgb, outlineResult.a);
    combined.a = max(combined.a, outlineResult.a);
    

    float2 uv = texCoord;
    float4 src = combined;
    float noise = tex2D(NoiseSamp, uv * noiseScale).r;
    
    if (src.a < 0.01f)
        discard;
    
    float edge = max(0.0001, edgeWidth);
    
    // 1. 디졸브 경계 계산 (더 넓은 범위)
    float dissolveEdge = progress - edge;
    float dissolveEdge2 = progress + edge;
    
    // 2. Glow 경계 계산 (디졸브보다 앞서서)
    float glowEdge1 = progress - edge; // 더 넓게
    float glowEdge2 = progress + edge;
    
    // 3. 본체 알파
    float alpha = smoothstep_custom(dissolveEdge, dissolveEdge2, noise);
    
    // 4. Glow 강도 (사라지기 직전이 가장 밝음)
    float glowMask = smoothstep_custom(glowEdge1, glowEdge2, noise)
                * (1.0 - smoothstep_custom(dissolveEdge2, dissolveEdge2 + edge, noise));
    
    float glow = saturate(glowMask * edgeIntensity); // 강도 증가
    
    // 5. 최종 색상 합성
    float4 color = src;
    color.rgb = lerp(src.rgb, edgeColor.rgb, glow * 0.7); // 원본과 glow 블렌딩
    color.rgb += edgeColor.rgb * glow * edgeIntensity; // 추가 발광
    
    // 6. 알파 처리
    color.a = max(src.a * (1.0 - alpha), glow * edgeColor.a);
 
    
    combined.rgb = lerp(combined.rgb, edgeColor.rgb, color.rgb * 0.7);
    combined.rgb += edgeColor.rgb * color.rgb * edgeIntensity;
    combined.a = max(combined.a * (1.0 - alpha), color.a * edgeColor.a);
    
    return combined;
}

// 테크닉
technique Tech_Dissolve
{
    pass P0
    {
        AlphaBlendEnable = TRUE;
        SrcBlend = SrcAlpha;
        DestBlend = InvSrcAlpha;
        VertexShader = compile vs_3_0 vs_passthrough();
        PixelShader = compile ps_3_0 ps_dissolve();
    }

    pass P1
    {
        AlphaBlendEnable = TRUE;
        SrcBlend = SrcAlpha;
        DestBlend = InvSrcAlpha;
        VertexShader = compile vs_3_0 vs_passthrough();
        PixelShader = compile ps_3_0 PS_Combined();
    }
}
