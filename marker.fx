// Click marker shader - animated red dot with expanding ring
// progress: 1.0 = just placed, 0.0 = expired

float4x4 matWVP;
float    progress;
float4   markerColor = float4(1.0, 0.2, 0.2, 1.0);

struct VS_IN
{
    float4 Pos : POSITION;
    float2 UV  : TEXCOORD0;
};

struct VS_OUT
{
    float4 Pos : POSITION;
    float2 UV  : TEXCOORD0;
};

VS_OUT VS_Marker(VS_IN input)
{
    VS_OUT output;
    output.Pos = mul(input.Pos, matWVP);
    output.UV  = input.UV;
    return output;
}

float4 PS_Marker(VS_OUT input) : COLOR0
{
    // Distance from quad center, normalized (edge = 1)
    float2 c = float2(0.5, 0.5);
    float  d = length(input.UV - c) * 2.0;

    // Expanding ring (from center outward)
    float ringPos  = 1.0 - progress;
    float ringEdge = 0.10;
    float ring     = 1.0 - saturate(abs(d - ringPos) / ringEdge);
    ring = ring * ring;                             // sharpen
    ring *= smoothstep(0.0, 0.3, progress);         // fade in at start

    // Central pulsing dot
    float dotCore   = 1.0 - smoothstep(0.12, 0.28, d);
    float pulse     = 0.75 + 0.25 * sin(progress * 30.0);
    float centerDot = dotCore * pulse;

    // Composite
    float mask  = saturate(ring + centerDot);
    float alpha = mask * progress * markerColor.a;

    if (alpha < 0.01) discard;
    return float4(markerColor.rgb, alpha);
}

technique Tech_Marker
{
    pass P0
    {
        VertexShader     = compile vs_3_0 VS_Marker();
        PixelShader      = compile ps_3_0 PS_Marker();
        AlphaBlendEnable = true;
        SrcBlend         = SrcAlpha;
        DestBlend        = InvSrcAlpha;
        ZEnable          = true;
        ZWriteEnable     = false;
        CullMode         = None;
    }
}
