// -----------------------------------------------------------------------------
// simple.fx
//
// このファイルは「ジオメトリを描きながら、中間情報を複数のテクスチャへ書き出す」役割を持ちます。
// いわゆる G-buffer に近い考え方で、後段の SSAO 計算に必要な材料をここで準備します。
//
// 本サンプルで出力している主な情報:
// - 通常カラー
// - 0..1 に正規化した前面深度
// - ビュー空間法線
// - ビュー空間の実深度
// - 背面深度
// -----------------------------------------------------------------------------

// 頂点位置を最終的な画面位置へ変換する行列です。
float4x4 g_matWorldViewProj;

// モデル座標 -> ビュー座標へ変換する行列です。
// 実深度(viewPosition.z) を作るときに使います。
float4x4 g_matWorldView;

// モデル座標 -> ワールド座標へ変換する行列です。
// 法線をワールド空間へ回すために使います。
float4x4 g_matWorld;

// 簡易的な平行光源方向です。
// w=0 は「位置ではなく方向ベクトル」であることを表しています。
float4 g_lightNormal = { -0.3f, -1.0f, -0.5f, 0.0f };

// 最低限の明るさです。Lambert が 0 の面も真っ黒にならないようにします。
float3 g_ambient = { 0.3f, 0.3f, 0.3f };

// ビュー空間深度を 0..1 へ正規化するときの基準距離です。
float g_ssaoDepthRange = 50.0f;

// テクスチャ表示の ON/OFF
bool g_bUseTexture = true;

// Lambert 拡散反射の ON/OFF
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

// -----------------------------------------------------------------------------
// 頂点シェーダー
//
// ここでは 1 頂点ごとに次の準備をします。
// 1. 画面に描くための clip space 位置を計算する
// 2. テクスチャ座標をそのまま次段へ渡す
// 3. 法線をワールド空間へ回して正規化する
// 4. ローカル座標を後段へ渡す
//
// ローカル座標を渡している理由:
// ピクセルシェーダー側で g_matWorldView を掛け、各ピクセルの viewPosition を求めたいからです。
// -----------------------------------------------------------------------------
void VertexShader1(
    in float4 inPosition : POSITION,
    in float3 inNormal : NORMAL,
    in float2 inTexCoord0 : TEXCOORD0,
    out float4 outPosition : POSITION0,
    out float2 outTexCoord0 : TEXCOORD0,
    out float4 outLocalPosition : TEXCOORD1,
    out float3 outNormal : TEXCOORD2)
{
    // モデル座標 -> clip space 変換
    float4 clipPosition = mul(inPosition, g_matWorldViewProj);
    outPosition = clipPosition;

    // UV は補間されてそのままピクセルシェーダーへ届きます。
    outTexCoord0 = inTexCoord0;

    // 法線は位置と違って平行移動の影響を受けないので w=0 のベクトルとして扱います。
    outNormal = normalize(mul(float4(inNormal, 0.0f), g_matWorld).xyz);

    // viewPosition の再計算に使うため、ローカル座標もそのまま流します。
    outLocalPosition = inPosition;
}

// -----------------------------------------------------------------------------
// MRT 用ピクセルシェーダー
//
// 1 回の描画で複数の render target(COLOR0, COLOR1...) へ同時に出力します。
// これが MRT(Multiple Render Targets) です。
//
// 本サンプルでは次のように使っています。
// COLOR0: 通常カラー
// COLOR1: 0..1 に正規化した前面深度
// COLOR2: 0..1 へエンコードした法線
// COLOR3: 実深度(線形深度)
// -----------------------------------------------------------------------------
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
        // Lambert 反射は「法線と光方向の内積」で面の向きを明るさへ変換する最も基本的なモデルです。
        float3 lightDir = normalize(-g_lightNormal.xyz);
        float lambert = saturate(dot(normal, lightDir));
        lighting = saturate(g_ambient + lambert.xxx);
    }

    // 元の色へ簡易ライティングを掛けたものが通常カラーです。
    outColor0 = float4(baseColor.rgb * lighting, baseColor.a);

    // ローカル座標からビュー空間位置を作ります。
    // viewPosition.z は「カメラから見てどれだけ前方にあるか」に相当します。
    float4 viewPosition = mul(inLocalPosition, g_matWorldView);
    float viewDepth = max(0.0f, viewPosition.z);

    // 0..1 の範囲へ正規化した深度です。
    // 深度テクスチャとして比較しやすい一方、絶対距離は失われます。
    float depth01 = saturate(viewDepth / g_ssaoDepthRange);

    // 法線をビュー空間へ回します。
    // 後段で「カメラから見てどちらを向いているか」を使うためです。
    float3 viewNormal = normalize(mul(float4(normal, 0.0f), g_matWorldView).xyz);

    outDepth = float4(depth01, 0.0f, 0.0f, 1.0f);

    // 法線は本来 -1..1 ですが、テクスチャへ保存しやすいように 0..1 へ写像します。
    outColor2 = float4(viewNormal * 0.5f + 0.5f, 1.0f);

    // こちらはメートル相当の実深度をそのまま保持します。
    outRawDepth = float4(viewDepth, 0.0f, 0.0f, 1.0f);
}

// -----------------------------------------------------------------------------
// 背面深度出力用ピクセルシェーダー
//
// thickness を求めるには「表面の手前側の深度」だけでなく
// 「その裏面がどれくらい奥にあるか」も知りたいので、背面だけを別パスで描きます。
// VFACE は front face / back face を見分けるための入力です。
// -----------------------------------------------------------------------------
void PixelShaderBackDepth(
    in float4 inLocalPosition : TEXCOORD1,
    in float faceSign : VFACE,
    out float4 outDepth : COLOR0)
{
    // 正面向きの面は捨てて、背面だけ残します。
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

// 背面深度の取得
technique TechniqueBackDepth
{
    pass P0
    {
        CullMode = NONE;
        VertexShader = compile vs_3_0 VertexShader1();
        PixelShader = compile ps_3_0 PixelShaderBackDepth();
    }
}
