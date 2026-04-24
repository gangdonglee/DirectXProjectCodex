// Selection ring shader - green ring under selected units with pulse animation

float4x4 matWVP;
float    time;
float4   ringColor = float4(0.25, 1.0, 0.3, 1.0);

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

VS_OUT VS_Selection(VS_IN input)
{
    VS_OUT output;
    output.Pos = mul(input.Pos, matWVP);
    output.UV  = input.UV;
    return output;
}

float4 PS_Selection(VS_OUT input) : COLOR0
{
    // Distance from center, normalized so edge = 1
    float2 c = float2(0.5, 0.5);
    float  d = length(input.UV - c) * 2.0;

    // Ring between inner and outer radii
    const float ringOuter = 0.92;
    const float ringInner = 0.76;
    const float edgeSoft  = 0.05;

    float inner = smoothstep(ringInner - edgeSoft, ringInner, d);
    float outer = 1.0 - smoothstep(ringOuter, ringOuter + edgeSoft, d);
    float mask  = inner * outer;

    // Soft breathing pulse
    float pulse = 0.75 + 0.25 * sin(time * 3.5);

    float alpha = mask * ringColor.a * pulse;
    if (alpha < 0.01) discard;

    return float4(ringColor.rgb, alpha);
}

technique Tech_Selection
{
    pass P0
    {
        VertexShader     = compile vs_3_0 VS_Selection();
        PixelShader      = compile ps_3_0 PS_Selection();
        AlphaBlendEnable = true;
        SrcBlend         = SrcAlpha;
        DestBlend        = InvSrcAlpha;
        ZEnable          = true;
        ZWriteEnable     = false;
        CullMode         = None;
    }
}
