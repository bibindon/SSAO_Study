// simple2.fx  — SSAO(6方向) 合成用（UTF-8 / BOMなし）
// 入力: 
//   texColor = RT0(カラー), texZ = RT1(Z画像: RGB=可視化/ A=線形Z), texPos = RT2(POS画像: 0..1にマップ済み)
// 必要パラメータ: g_matView, g_matProj, g_fNear, g_fFar
// AO設定: g_aoStepWorld(=1.0), g_aoStrength(0..1), g_aoBias(深度比較バイアス)

texture texColor;
sampler sampColor = sampler_state
{
    Texture = (texColor);
    MipFilter = LINEAR;
    MinFilter = LINEAR;
    MagFilter = LINEAR;
    AddressU = CLAMP;
    AddressV = CLAMP;
};

texture texZ;
sampler sampZ = sampler_state
{
    Texture = (texZ);
    MipFilter = POINT;
    MinFilter = POINT;
    MagFilter = POINT; // 深度比較なのでPOINTが無難
    AddressU = CLAMP;
    AddressV = CLAMP;
};

texture texPos;
sampler sampPos = sampler_state
{
    Texture = (texPos);
    MipFilter = LINEAR;
    MinFilter = LINEAR;
    MagFilter = LINEAR;
    AddressU = CLAMP;
    AddressV = CLAMP;
};

// 行列・カメラ
float4x4 g_matView;
float4x4 g_matProj;
float g_fNear = 1.0f;
float g_fFar = 10000.0f;

// AO設定
float g_aoStepWorld = 1.0f; // 6方向へ動かす距離（ワールド単位）
float g_aoStrength = 0.7f; // 影の強さ（0..1）
float g_aoBias = 0.002f; // 比較用バイアス（自己遮蔽防止）

struct VS_IN
{
    float4 pos : POSITION;
    float2 uv : TEXCOORD0;
};
struct VS_OUT
{
    float4 pos : POSITION;
    float2 uv : TEXCOORD0;
};

VS_OUT VS_Fullscreen(VS_IN i)
{
    VS_OUT o;
    o.pos = i.pos;
    o.uv = i.uv;
    return o;
}

// NDC(xy/w) → テクスチャUV(0..1) へ変換（D3D9はY軸反転に注意）
float2 NdcToUv(float4 clip)
{
    float2 ndc = clip.xy / clip.w; // [-1,1]
    float2 uv;
    uv.x = 0.5f * ndc.x + 0.5f;
    uv.y = -0.5f * ndc.y + 0.5f; // ← Y 反転
    return uv;
}

// POS画像(0..1)→ワールド座標(-range..+range) への復元
// ※ パス1(simple.fx)で可視化のために 0..1 へエンコードしている前提。
//   もしエンコード式を変えている場合は、ここを一致させてください。
float3 DecodeWorldPos(float3 enc, float3 center, float range)
{
    // enc = (pos - center)/range * 0.5 + 0.5
    float3 nrm = (enc - 0.5f) * 2.0f; // -1..1
    return nrm * range + center;
}

// 正規化線形Z（near..far を 0..1）
float LinearizeZ(float viewZ)
{
    return saturate((viewZ - g_fNear) / (g_fFar - g_fNear));
}

float4 PS_AO(VS_OUT i) : COLOR0
{
    float4 color = tex2D(sampColor, i.uv);

    // 中心ピクセルの線形Z（これを基準に判定）
    const float centerZ = tex2D(sampZ, i.uv).a;

    // 現在のワールド座標（POS画像→復元）
    const float3 posCenter = float3(0, 0, 0);
    const float posRange = 50.0f;
    float3 worldPos = DecodeWorldPos(tex2D(sampPos, i.uv).rgb, posCenter, posRange);

    float3 dirs[6] =
    {
        float3(1, 0, 0), float3(-1, 0, 0),
        float3(0, 1, 0), float3(0, -1, 0),
        float3(0, 0, 1), float3(0, 0, -1)
    };

    int occluded = 0;
    [unroll]
    for (int k = 0; k < 6; ++k)
    {
        float3 wp = worldPos + dirs[k] * g_aoStepWorld;

        float4 vpos = mul(float4(wp, 1), g_matView);
        float4 cpos = mul(vpos, g_matProj);
        if (cpos.w <= 0)
            continue;

        float2 suv = NdcToUv(cpos);
        if (suv.x < 0 || suv.x > 1 || suv.y < 0 || suv.y > 1)
            continue;

        // ここを「サンプル点のZ」ではなく「中心Z」と比較に変更
        float zImage = tex2D(sampZ, suv).a;
        if (zImage + g_aoBias < centerZ)   // ← これで前景の筒にも効く
            occluded++;
    }

    float ao = 1.0f - g_aoStrength * (occluded / 6.0f);
    ao = saturate(ao);

    return float4(color.rgb * ao, color.a);
}


technique TechniqueAO
{
    pass P0
    {
        CullMode = NONE;
        VertexShader = compile vs_3_0 VS_Fullscreen();
        PixelShader = compile ps_3_0 PS_AO();
    }
}
