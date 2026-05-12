// -----------------------------------------------------------------------------
// simple.fx
//
// ジオメトリ描画側のシェーダー。
// カラーだけでなく、後段の SSAO で使う深度、法線、実深度、背面深度もここで作る。
// -----------------------------------------------------------------------------

// 最終的な画面位置へ変換する行列。
float4x4 g_matWorldViewProj;

// ビュー空間位置を作るための行列。
float4x4 g_matWorldView;

// 法線をワールド空間へ回すための行列。
float4x4 g_matWorld;

// 簡易的な平行光源方向。
float4 g_lightNormal = { -0.3f, -1.0f, -0.5f, 0.0f };

// 最低限の明るさ。
float3 g_ambient = { 0.3f, 0.3f, 0.3f };

// 深度正規化の基準距離。
float g_ssaoDepthRange = 50.0f;

bool g_bUseTexture = true;
bool g_bUseLambert = true;
bool g_bShowNormalInfo = false;

texture texture1;
sampler textureSampler = sampler_state
{
    Texture = (texture1);
    MipFilter = NONE;
    MinFilter = NONE;
    MagFilter = NONE;
};

// 頂点シェーダー。
// clip space 位置、UV、法線、ローカル座標を後段へ渡す。
void VertexShader1(
    in float4 inPosition : POSITION,
    in float3 inNormal : NORMAL,
    in float2 inTexCoord0 : TEXCOORD0,
    out float4 outPosition : POSITION0,
    out float2 outTexCoord0 : TEXCOORD0,
    out float4 outLocalPosition : TEXCOORD1,
    out float3 outNormal : TEXCOORD2)
{
    float4 clipPosition = mul(inPosition, g_matWorldViewProj);
    outPosition = clipPosition;

    outTexCoord0 = inTexCoord0;

    // 法線は方向ベクトルなので w=0 として扱う。
    outNormal = normalize(mul(float4(inNormal, 0.0f), g_matWorld).xyz);

    outLocalPosition = inPosition;
}

// MRT 用ピクセルシェーダー。
// COLOR0: 通常カラー
// COLOR1: 正規化前面深度
// COLOR2: 法線
// COLOR3: 実深度
void PixelShaderMRT(
    in float2 inTexCoord0 : TEXCOORD0,
    in float4 inLocalPosition : TEXCOORD1,
    in float3 inNormal : TEXCOORD2,
    out float4 outColor0 : COLOR0,
    out float4 outDepth : COLOR1,
    out float4 outColor2 : COLOR2,
    out float4 outRawDepth : COLOR3)
{
    float4 baseColor = float4(0.5, 0.5, 0.5, 1.0);

    if (g_bUseTexture)
    {
        baseColor = tex2D(textureSampler, inTexCoord0);
    }

    float3 normal = normalize(inNormal);
    float3 lighting = 1.0.xxx;
    if (g_bUseLambert)
    {
        float3 lightDir = normalize(-g_lightNormal.xyz);
        float lambert = saturate(dot(normal, lightDir));
        lighting = saturate(g_ambient + lambert.xxx);
    }

    outColor0 = float4(baseColor.rgb * lighting, baseColor.a);

    float4 viewPosition = mul(inLocalPosition, g_matWorldView);
    float viewDepth = max(0.0f, viewPosition.z);
    float depth01 = saturate(viewDepth / g_ssaoDepthRange);

    // 後段ではビュー空間法線を使う。
    float3 viewNormal = normalize(mul(float4(normal, 0.0f), g_matWorldView).xyz);

    outDepth = float4(depth01, 0.0f, 0.0f, 1.0f);

    // 法線は 0..1 にエンコードして保存。
    outColor2 = float4(viewNormal * 0.5f + 0.5f, 1.0f);

    outRawDepth = float4(viewDepth, 0.0f, 0.0f, 1.0f);
}

// 背面深度出力用。
// thickness を作るため、背面だけを別に描く。
void PixelShaderBackDepth(
    in float4 inLocalPosition : TEXCOORD1,
    in float faceSign : VFACE,
    out float4 outDepth : COLOR0)
{
    // front face は捨てる。
    if (faceSign > 0.0f)
    {
        clip(-1.0f);
    }

    float4 viewPosition = mul(inLocalPosition, g_matWorldView);
    float viewDepth = max(0.0f, viewPosition.z);
    float depth01 = saturate(viewDepth / g_ssaoDepthRange);
    outDepth = float4(depth01, 0.0f, 0.0f, 1.0f);
}

// 通常描画 + MRT 出力
technique TechniqueMRT
{
    pass P0
    {
        CullMode = NONE;
        VertexShader = compile vs_3_0 VertexShader1();
        PixelShader = compile ps_3_0 PixelShaderMRT();
    }
}

// 背面深度取得
technique TechniqueBackDepth
{
    pass P0
    {
        CullMode = NONE;
        VertexShader = compile vs_3_0 VertexShader1();
        PixelShader = compile ps_3_0 PixelShaderBackDepth();
    }
}
