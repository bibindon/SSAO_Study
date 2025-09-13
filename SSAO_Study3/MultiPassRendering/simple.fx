// simple.fx  — MRT3: RT0=color, RT1=linearZ(0..1), RT2=world-position visualization (RGB)
// ※ UTF-8 (BOMなし)

float4x4 g_matWorldViewProj;
float4x4 g_matWorld; // 追加
float4x4 g_matView;
float4x4 g_matProj;

float g_fNear = 1.0f;
float g_fFar = 10000.0f;

// Z可視化の調整（表示しやすくするためのレンジ＆ガンマ）
float g_vizMax = 100.0f; // ここまでを 0..1 で表示（例: 100）
float g_vizGamma = 0.25f; // 視認性向上のためのガンマ（例: 0.25=1/4）

// World座標の可視化パラメータ（[-range..+range] を 0..1 にマップ）
float3 g_posCenter = float3(0, 0, 0); // 中心（必要に応じて変更可）
float g_posRange = 50.0f; // 半レンジ。[-50..50] を 0..1 に

float4 g_lightNormal = float4(0.3f, 1.0f, 0.5f, 0.0f);
float3 g_ambient = float3(0.3f, 0.3f, 0.3f);

bool g_bUseTexture = true;

texture texture1;
sampler textureSampler = sampler_state
{
    Texture = (texture1);
    MipFilter = LINEAR;
    MinFilter = LINEAR;
    MagFilter = LINEAR;
};

void VertexShader1(
    in float4 inPosition : POSITION,
    in float4 inNormal : NORMAL0,
    in float4 inTexCood : TEXCOORD0,

    out float4 outPosition : POSITION,
    out float4 outDiffuse : COLOR0,
    out float4 outTexCood : TEXCOORD0,
    out float outViewZ : TEXCOORD1, // ビュー空間Z
    out float3 outWorldPos : TEXCOORD2 // ワールド座標
)
{
    float4 worldPos = mul(inPosition, g_matWorld);
    outPosition = mul(worldPos, g_matView);
    outPosition = mul(outPosition, g_matProj);

    float lightIntensity = dot(inNormal, g_lightNormal);
    outDiffuse.rgb = max(0, lightIntensity) + g_ambient;
    outDiffuse.a = 1.0f;

    outTexCood = inTexCood;

    // ビュー空間Z（Left-Handedで前方+Zを想定）
    float4 vpos = mul(worldPos, g_matView);
    outViewZ = vpos.z;

    // ワールド座標（そのままPSへ渡す）
    outWorldPos = worldPos.xyz;
}

void PixelShader1(
    in float4 inScreenColor : COLOR0,
    in float2 inTexCood : TEXCOORD0,
    out float4 outColor : COLOR
)
{
    float4 sampled = tex2D(textureSampler, inTexCood);
    float4 baseColor = g_bUseTexture ? (inScreenColor * sampled) : inScreenColor;
    outColor = baseColor;
}

// MRT3: RT0=カラー, RT1=Z 可視化, RT2=ワールド座標 可視化
void PixelShaderMRT3(
    in float4 inScreenColor : COLOR0,
    in float2 inTexCood : TEXCOORD0,
    in float inViewZ : TEXCOORD1,
    in float3 inWorldPos : TEXCOORD2,
    out float4 outColor0 : COLOR0,
    out float4 outColor1 : COLOR1,
    out float4 outColor2 : COLOR2
)
{
    float4 sampled = tex2D(textureSampler, inTexCood);
    float4 baseColor = g_bUseTexture ? (inScreenColor * sampled) : inScreenColor;

    // RT0: カラー
    outColor0 = baseColor;

    // RT1: 線形Zの可視化（0..1）＋ガンマで持ち上げ
    float viz = saturate((inViewZ - g_fNear) / (g_vizMax - g_fNear));
    viz = pow(viz, g_vizGamma);
    outColor1 = float4(viz, viz, viz, 1);

    // RT2: ワールド座標の可視化（[-range..+range] -> 0..1 へマップ）
    float3 nrm = (inWorldPos - g_posCenter) / g_posRange; // -1..1
    float3 enc = saturate(nrm * 0.5 + 0.5); //  0..1
    outColor2 = float4(enc, 1);
}

// 単一出力（互換）
technique Technique1
{
    pass P0
    {
        CullMode = NONE;
        VertexShader = compile vs_3_0 VertexShader1();
        PixelShader = compile ps_3_0 PixelShader1();
    }
}

// MRT3
technique TechniqueMRT
{
    pass P0
    {
        CullMode = NONE;
        VertexShader = compile vs_3_0 VertexShader1();
        PixelShader = compile ps_3_0 PixelShaderMRT3();
    }
}
