float4   g_TextColor    = float4(1, 1, 1, 1);
float4   g_OutlineColor = float4(0, 0, 0, 1);
float2   g_TexelSize;
float    g_OutlineSize = 0;

texture g_TextTexture;

sampler TextSampler = sampler_state
{
    Texture   = <g_TextTexture>;
    MinFilter = Linear;
    MagFilter = Linear;
    AddressU  = Clamp;
    AddressV  = Clamp;
};

struct PS_IN
{
    float2 UV : TEXCOORD0;
};

float4 PS_Simple(PS_IN input) : COLOR0
{
    float4 tex = tex2D(TextSampler, input.UV);
    return tex * g_TextColor;
}

float4 PS_Outline(PS_IN input) : COLOR0
{
    float2 uv = input.UV;
    float4 center = tex2D(TextSampler, uv);
    float sz = g_OutlineSize;
    float2 tx = g_TexelSize;

    
    float hR = max(tex2D(TextSampler, uv + float2( sz * 0.5 * tx.x, 0)).a,
                   tex2D(TextSampler, uv + float2( sz       * tx.x, 0)).a);
    float hL = max(tex2D(TextSampler, uv + float2(-sz * 0.5 * tx.x, 0)).a,
                   tex2D(TextSampler, uv + float2(-sz       * tx.x, 0)).a);
    float hD = max(tex2D(TextSampler, uv + float2(0,  sz * 0.5 * tx.y)).a,
                   tex2D(TextSampler, uv + float2(0,  sz       * tx.y)).a);
    float hU = max(tex2D(TextSampler, uv + float2(0, -sz * 0.5 * tx.y)).a,
                   tex2D(TextSampler, uv + float2(0, -sz       * tx.y)).a);

    float encMin = min(min(hR, hL), min(hD, hU));
    float encMask = 1.0 - step(0.5, encMin) * step(center.a, 0.5);

    
    float ma = 0;
    float d1 = sz * 0.33;
    float d2 = sz * 0.66;
    float d3 = sz;

    
    ma = max(ma, tex2D(TextSampler, uv + float2( d1 * tx.x, 0)).a);
    ma = max(ma, tex2D(TextSampler, uv + float2( d2 * tx.x, 0)).a);
    ma = max(ma, tex2D(TextSampler, uv + float2( d3 * tx.x, 0)).a);
    
    ma = max(ma, tex2D(TextSampler, uv + float2(-d1 * tx.x, 0)).a);
    ma = max(ma, tex2D(TextSampler, uv + float2(-d2 * tx.x, 0)).a);
    ma = max(ma, tex2D(TextSampler, uv + float2(-d3 * tx.x, 0)).a);
    
    ma = max(ma, tex2D(TextSampler, uv + float2(0,  d1 * tx.y)).a);
    ma = max(ma, tex2D(TextSampler, uv + float2(0,  d2 * tx.y)).a);
    ma = max(ma, tex2D(TextSampler, uv + float2(0,  d3 * tx.y)).a);
    
    ma = max(ma, tex2D(TextSampler, uv + float2(0, -d1 * tx.y)).a);
    ma = max(ma, tex2D(TextSampler, uv + float2(0, -d2 * tx.y)).a);
    ma = max(ma, tex2D(TextSampler, uv + float2(0, -d3 * tx.y)).a);
    
    ma = max(ma, tex2D(TextSampler, uv + float2( d1 * 0.707 * tx.x,  d1 * 0.707 * tx.y)).a);
    ma = max(ma, tex2D(TextSampler, uv + float2( d2 * 0.707 * tx.x,  d2 * 0.707 * tx.y)).a);
    ma = max(ma, tex2D(TextSampler, uv + float2( d3 * 0.707 * tx.x,  d3 * 0.707 * tx.y)).a);
    
    ma = max(ma, tex2D(TextSampler, uv + float2(-d1 * 0.707 * tx.x, -d1 * 0.707 * tx.y)).a);
    ma = max(ma, tex2D(TextSampler, uv + float2(-d2 * 0.707 * tx.x, -d2 * 0.707 * tx.y)).a);
    ma = max(ma, tex2D(TextSampler, uv + float2(-d3 * 0.707 * tx.x, -d3 * 0.707 * tx.y)).a);
    
    ma = max(ma, tex2D(TextSampler, uv + float2(-d1 * 0.707 * tx.x,  d1 * 0.707 * tx.y)).a);
    ma = max(ma, tex2D(TextSampler, uv + float2(-d2 * 0.707 * tx.x,  d2 * 0.707 * tx.y)).a);
    ma = max(ma, tex2D(TextSampler, uv + float2(-d3 * 0.707 * tx.x,  d3 * 0.707 * tx.y)).a);
    
    ma = max(ma, tex2D(TextSampler, uv + float2( d1 * 0.707 * tx.x, -d1 * 0.707 * tx.y)).a);
    ma = max(ma, tex2D(TextSampler, uv + float2( d2 * 0.707 * tx.x, -d2 * 0.707 * tx.y)).a);
    ma = max(ma, tex2D(TextSampler, uv + float2( d3 * 0.707 * tx.x, -d3 * 0.707 * tx.y)).a);

    float outAlpha = smoothstep(0.15, 0.6, ma) * encMask;

    float outA = outAlpha * g_OutlineColor.a;
    float txtA = center.a * g_TextColor.a;

    float3 col = g_OutlineColor.rgb * outA * (1.0 - txtA) + g_TextColor.rgb * txtA;
    float  a   = outA * (1.0 - txtA) + txtA;

    return float4(col, a);
}

technique TextRender
{
    pass P0
    {
        VertexShader     = NULL;
        PixelShader      = compile ps_2_0 PS_Simple();
        AlphaBlendEnable = true;
        SrcBlend         = SrcAlpha;
        DestBlend        = InvSrcAlpha;
        ZEnable          = false;
        ZWriteEnable     = false;
    }
}

technique TextRenderOutline
{
    pass P0
    {
        VertexShader     = NULL;
        PixelShader      = compile ps_3_0 PS_Outline();
        AlphaBlendEnable = true;
        SrcBlend         = SrcAlpha;
        DestBlend        = InvSrcAlpha;
        ZEnable          = false;
        ZWriteEnable     = false;
    }
}
