float4x4 g_matWorldViewProj;
float4x4 g_matView;
float g_Far = 10000.0f; // 遠クリップ。main.cpp側でセット

struct VSIn
{
    float4 pos : POSITION;
    float3 nrm : NORMAL0;
    float2 uv : TEXCOORD0;
};

struct VSOut
{
    float4 pos : POSITION;
    float depth : TEXCOORD0;
};

VSOut VS(VSIn i)
{
    VSOut o;
    o.pos = mul(i.pos, g_matWorldViewProj);

    // ビュー空間Zを取り出し、[0..1]の線形深度に正規化（LH系なので+Z）
    float3 viewPos = mul(i.pos, g_matView).xyz;
    float linZ = viewPos.z / g_Far; // 0(近)～1(遠)  ※near/far比により近端は厳密には0になりません
    o.depth = saturate(linZ);
    return o;
}

float4 PS(VSOut i) : COLOR
{
    float d = i.depth;
    // 16bit浮動小数フォーマットでも見やすいようにRGBへ複製
    return float4(d, d, d, 1.0);
}

technique Technique1
{
    pass P0
    {
        CullMode = NONE;
        ZEnable = TRUE;

        VertexShader = compile vs_3_0 VS();
        PixelShader = compile ps_3_0 PS();
    }
}
