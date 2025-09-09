// 1パス目で作った深度テクスチャ（R=線形深度）
texture texture1;
sampler depthSamp = sampler_state
{
    Texture = (texture1);
    MinFilter = POINT;
    MagFilter = POINT;
    MipFilter = NONE;
    AddressU = CLAMP;
    AddressV = CLAMP;
};

float4 g_TexelSize = float4(1.0 / 640.0, 1.0 / 480.0, 0, 0); // main.cppからセット
float g_RadiusPixels = 4.0; // 近傍探索半径（画素）
float g_Bias = 0.001; // バイアス（アクネ対策）
float g_Intensity = 1.2; // 濃さ

struct VSIn
{
    float4 pos : POSITION;
    float2 uv : TEXCOORD0;
};
struct VSOut
{
    float4 pos : POSITION;
    float2 uv : TEXCOORD0;
};

VSOut VS(VSIn i)
{
    VSOut o;
    o.pos = i.pos;
    o.uv = i.uv;
    return o;
}

// スクリーン空間の簡易近傍12方向（円＋十字＋斜め＋遠目）
static const float2 OFFS[12] =
{
    float2(1, 0), float2(-1, 0), float2(0, 1), float2(0, -1),
    float2(1, 1), float2(-1, 1), float2(1, -1), float2(-1, -1),
    float2(2, 0), float2(-2, 0), float2(0, 2), float2(0, -2)
};

float4 PS(VSOut i) : COLOR
{
    float center = tex2D(depthSamp, i.uv).r; // 線形深度

    float occ = 0.0;
    [unroll]
    for (int k = 0; k < 12; k++)
    {
        float2 duv = OFFS[k] * g_TexelSize.xy * g_RadiusPixels;
        float sd = tex2D(depthSamp, i.uv + duv).r;

        // “手前にあるサンプルは遮蔽”とみなす超簡易ルール
        // 線形深度は差が非常に小さいのでスケールを強めに
        float diff = center - sd - g_Bias;
        occ += saturate(diff * 500.0); // 見やすさ重視のスケール
    }

    occ = saturate((occ / 12.0) * g_Intensity);
    float ao = 1.0 - occ; // 1=白(非遮蔽), 0=黒(遮蔽)
    return float4(ao, ao, ao, 1.0);
}

technique Technique1
{
    pass P0
    {
        CullMode = NONE;
        ZEnable = FALSE;

        VertexShader = compile vs_3_0 VS();
        PixelShader = compile ps_3_0 PS();
    }
}
