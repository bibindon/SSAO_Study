// simple3.fx - SSAO結果に対するバイラテラルブラー

texture texAO;
sampler sampAO = sampler_state
{
    Texture = (texAO);
    MinFilter = LINEAR;
    MagFilter = LINEAR;
    MipFilter = LINEAR;
    AddressU = CLAMP;
    AddressV = CLAMP;
};

texture texZ;
sampler sampZ = sampler_state
{
    Texture = (texZ);
    MinFilter = POINT;
    MagFilter = POINT;
    MipFilter = POINT;
    AddressU = CLAMP;
    AddressV = CLAMP;
};

// 画面解像度
float g_texelSizeX = 1.0f / 800.0f; // main.cppのkBackWに合わせる
float g_texelSizeY = 1.0f / 600.0f; // main.cppのkBackHに合わせる

// ブラー強度
float g_blurRadius = 4.0f;
// 深度許容範囲 (この値を超えて深度が異なるとブラーしない)
float g_depthThreshold = 0.005f;

// ガウス分布の重み (16要素 - インデックス0から15)
static const float weights[] =
{
    0.0044318, 0.0116661, 0.0263625, 0.0487372, 0.0768406,
    0.106578, 0.132646, 0.145484, 0.145484, 0.132646,
    0.106578, 0.0768406, 0.0487372, 0.0263625, 0.0116661, 0.0044318
};
// 修正: kBlurSamplesを14に変更し、kHalfSamplesを7（安全なインデックス範囲）にする
static const int kBlurSamples = 14;
static const int kHalfSamples = kBlurSamples / 2; // kHalfSamples = 7

// バイラテラルブラー (Horizontal: 水平ブラー)
void PixelShaderBlurH(
    in float4 inPosition : POSITION,
    in float2 inTexCood : TEXCOORD0,
    out float4 outColor : COLOR0
)
{
    float baseDepth = tex2D(sampZ, inTexCood).a; // ZテクスチャのAチャンネルから線形デプスを取得
    float baseAO = tex2D(sampAO, inTexCood).r;
    float finalAO = baseAO * weights[kHalfSamples]; // weights[7]
    float totalWeight = weights[kHalfSamples];

    // 水平方向のサンプリング
    [unroll]
    for (int i = 1; i <= kHalfSamples; ++i) // i = 1 から 7 まで
    {
        float offset = g_texelSizeX * (float) i * g_blurRadius;
        float2 texCoordP = inTexCood + float2(offset, 0);
        float2 texCoordN = inTexCood - float2(offset, 0);

        // 正方向
        float sampleDepthP = tex2D(sampZ, texCoordP).a;
        float depthDiffP = abs(sampleDepthP - baseDepth);

        float weightP = weights[kHalfSamples + i]; // 最大 weights[14]

        // 深度が閾値内であればAOを加算
        if (depthDiffP < g_depthThreshold)
        {
            finalAO += tex2D(sampAO, texCoordP).r * weightP;
            totalWeight += weightP;
        }

        // 負方向
        float sampleDepthN = tex2D(sampZ, texCoordN).a;
        float depthDiffN = abs(sampleDepthN - baseDepth);

        float weightN = weights[kHalfSamples - i]; // 最小 weights[0]

        // 深度が閾値内であればAOを加算
        if (depthDiffN < g_depthThreshold)
        {
            finalAO += tex2D(sampAO, texCoordN).r * weightN;
            totalWeight += weightN;
        }
    }

    outColor = float4(finalAO / totalWeight, finalAO / totalWeight, finalAO / totalWeight, 1.0f);
}

// バイラテラルブラー (Vertical: 垂直ブラー)
void PixelShaderBlurV(
    in float4 inPosition : POSITION,
    in float2 inTexCood : TEXCOORD0,
    out float4 outColor : COLOR0
)
{
    float baseDepth = tex2D(sampZ, inTexCood).a; // ZテクスチャのAチャンネルから線形デプスを取得
    float baseAO = tex2D(sampAO, inTexCood).r;
    float finalAO = baseAO * weights[kHalfSamples]; // weights[7]
    float totalWeight = weights[kHalfSamples];

    // 垂直方向のサンプリング
    [unroll]
    for (int i = 1; i <= kHalfSamples; ++i) // i = 1 から 7 まで
    {
        float offset = g_texelSizeY * (float) i * g_blurRadius;
        float2 texCoordP = inTexCood + float2(0, offset);
        float2 texCoordN = inTexCood - float2(0, offset);

        // 正方向
        float sampleDepthP = tex2D(sampZ, texCoordP).a;
        float depthDiffP = abs(sampleDepthP - baseDepth);

        float weightP = weights[kHalfSamples + i]; // 最大 weights[14]

        // 深度が閾値内であればAOを加算
        if (depthDiffP < g_depthThreshold)
        {
            finalAO += tex2D(sampAO, texCoordP).r * weightP;
            totalWeight += weightP;
        }

        // 負方向
        float sampleDepthN = tex2D(sampZ, texCoordN).a;
        float depthDiffN = abs(sampleDepthN - baseDepth);

        float weightN = weights[kHalfSamples - i]; // 最小 weights[0]

        // 深度が閾値内であればAOを加算
        if (depthDiffN < g_depthThreshold)
        {
            finalAO += tex2D(sampAO, texCoordN).r * weightN;
            totalWeight += weightN;
        }
    }

    outColor = float4(finalAO / totalWeight, finalAO / totalWeight, finalAO / totalWeight, 1.0f);
}

// フルスクリーンクアッド描画用の頂点シェーダー (共有)
void VertexShaderPassThrough(
    in float4 inPosition : POSITION,
    in float2 inTexCood : TEXCOORD0,
    out float4 outPosition : POSITION,
    out float2 outTexCood : TEXCOORD0
)
{
    outPosition = inPosition;
    outTexCood = inTexCood;
}

// Technique
technique Blur
{
    // Pass 0: 水平ブラー (AO -> RT5)
    pass BlurH
    {
        VertexShader = compile vs_3_0 VertexShaderPassThrough();
        PixelShader = compile ps_3_0 PixelShaderBlurH();
    }

    // Pass 1: 垂直ブラー (RT5 -> RT4)
    pass BlurV
    {
        VertexShader = compile vs_3_0 VertexShaderPassThrough();
        PixelShader = compile ps_3_0 PixelShaderBlurV();
    }
}