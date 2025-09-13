// simple.fx  — MRT対応 + RT1に線形化Zを書き出し

float4x4 g_matWorldViewProj;
float4x4 g_matView; // ← 追加
float4x4 g_matProj; // ← 追加

float g_fNear = 1.0f; // ← 追加
float g_fFar = 10000.0f;

float4 g_lightNormal = { 0.3f, 1.0f, 0.5f, 0.0f };
float3 g_ambient = { 0.3f, 0.3f, 0.3f };

bool g_bUseTexture = true;

// 追加の可視化パラメータ（必要なら effect から書き換え）
float g_vizMax = 100.0f; // この距離までを 0..1 に表示（例: 100）
float g_vizGamma = 0.25f; // 1/4 ガンマで持ち上げ（0.25〜0.5 あたり）

texture texture1;
sampler textureSampler = sampler_state
{
    Texture = (texture1);
    MipFilter = LINEAR;
    MinFilter = LINEAR;
    MagFilter = LINEAR;
};

// =====================
// Vertex Shader
// =====================
void VertexShader1(
    in float4 inPosition : POSITION,
    in float4 inNormal : NORMAL0,
    in float4 inTexCood : TEXCOORD0,

    out float4 outPosition : POSITION,
    out float4 outDiffuse : COLOR0,
    out float4 outTexCood : TEXCOORD0,
    out float outViewZ : TEXCOORD1 // ← 追加: ビュー空間Z
)
{
    // 位置
    outPosition = mul(inPosition, g_matWorldViewProj);

    // 簡易ライティング
    float lightIntensity = dot(inNormal, g_lightNormal);
    outDiffuse.rgb = max(0, lightIntensity) + g_ambient;
    outDiffuse.a = 1.0f;

    // テクスチャ座標
    outTexCood = inTexCood;

    // ビュー空間Z（Left-Handed: 前方が +Z）
    float4 viewPos = mul(inPosition, g_matView);
    outViewZ = viewPos.z; // これをピクセルシェーダで線形化
}

// =====================
// Pixel Shader (単一出力)
// =====================
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

// =====================
// Pixel Shader (MRT: RT0=カラー, RT1=線形Z)
// =====================
void PixelShaderMRT(
    in float4 inScreenColor : COLOR0,
    in float2 inTexCood : TEXCOORD0,
    in float inViewZ : TEXCOORD1, // VS から受け取ったビュー空間Z
    out float4 outColor0 : COLOR0, // RT0
    out float4 outColor1 : COLOR1 // RT1
)
{
    float4 sampled = tex2D(textureSampler, inTexCood);
    float4 baseColor = g_bUseTexture ? (inScreenColor * sampled) : inScreenColor;

    // RT0: これまで通りのカラー
    outColor0 = baseColor;
    
    float viz = saturate((inViewZ - g_fNear) / (g_vizMax - g_fNear));
    viz = pow(viz, g_vizGamma); // 影を持ち上げて見やすく
    outColor1 = float4(viz, viz, viz, 1);
}

// 既存の単一出力テクニック
technique Technique1
{
    pass P0
    {
        CullMode = NONE;
        VertexShader = compile vs_3_0 VertexShader1();
        PixelShader = compile ps_3_0 PixelShader1();
    }
}

// MRT 用テクニック（RT0:COLOR0, RT1:COLOR1）
technique TechniqueMRT
{
    pass P0
    {
        CullMode = NONE;
        VertexShader = compile vs_3_0 VertexShader1();
        PixelShader = compile ps_3_0 PixelShaderMRT();
    }
}
