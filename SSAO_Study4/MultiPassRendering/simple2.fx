// simple2.fx  — UTF-8 (BOMなし)
// 入力: texColor=RT0, texZ=RT1(RGB=可視化, A=linearZ), texPos=RT2
// 出力: AO を乗算したカラー
// 方法: POS→6方向に 1.0 離した点を View/Proj で投影し、Z画像(α)と centerZ を比較

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
    MagFilter = POINT; // 深度比較向け
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

float4x4 g_matView;
float4x4 g_matProj;
float g_fNear = 1.0f;
float g_fFar = 10000.0f;

// POSデコード（Pass1 と一致させる）
float4 g_posCenter = float4(0, 0, 0, 0);
float g_posRange = 50.0f;

// AO 設定
float g_aoStepWorld = 1.0f;
float g_aoStrength = 0.7f;
float g_aoBias = 0.0002f; // FP16なら極小でOK

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

// clip→UV
float2 NdcToUv(float4 clip)
{
    float2 ndc = clip.xy / clip.w; // [-1,1]
    return float2(0.5f * ndc.x + 0.5f, -0.5f * ndc.y + 0.5f);
}

// POS画像(0..1)→ワールド座標
float3 DecodeWorldPos(float3 enc)
{
    float3 nrm = (enc - 0.5f) * 2.0f; // -1..1
    return nrm * g_posRange + g_posCenter.xyz;
}

// PS_AO — ランダム32サンプル版（ビュー空間）
// ・サンプル点は vCenter 周囲の球内をランダムに分布（近いほど多い：半径 r = (rand^2) * g_aoStepWorld）
// ・遮蔽判定は「Z画像(α) < サンプル点Z」でカウント
float4 PS_AO(VS_OUT i) : COLOR0
{
    float4 color = tex2D(sampColor, i.uv);

    // 中心点（ビュー空間）
    float3 worldPos = DecodeWorldPos(tex2D(sampPos, i.uv).rgb);
    float3 vCenter = mul(float4(worldPos, 1.0f), g_matView).xyz;

    // 乱数シード（画素毎に異なる値：フレームで変えたければ時間を混ぜる）
    float2 seed2 = i.uv * 1024.0f;

    const int kSamples = 32;
    int occ = 0;

    [unroll]
    for (int k = 0; k < kSamples; ++k)
    {
        // ---- 擬似乱数（各サンプルで独立に 0..1 を3つ生成） ----
        float s = (float) k * 37.0f; // サンプル番号によるシードずらし
        float r1 = frac(sin(dot(float3(seed2, s + 0.11f), float3(12.9898f, 78.233f, 37.719f))) * 43758.5453f);
        float r2 = frac(sin(dot(float3(seed2, s + 0.27f), float3(12.9898f, 78.233f, 37.719f))) * 43758.5453f);
        float r3 = frac(sin(dot(float3(seed2, s + 0.49f), float3(12.9898f, 78.233f, 37.719f))) * 43758.5453f);

        // ---- 方向ベクトル：[-1,1]^3 を正規化（ほぼ一様な球面分布）----
        float3 dir = normalize(float3(r1 * 2.0f - 1.0f,
                                      r2 * 2.0f - 1.0f,
                                      r3 * 2.0f - 1.0f) + 1e-5f);

        // ---- 半径：近くほど多い（r = (rand^2) * 最大半径）----
        float radius = (r1 * r1) * g_aoStepWorld; // ※g_aoStepWorld を“最大半径”として利用

        // ビュー空間でサンプル
        float3 vSample = vCenter + dir * radius;

        // View→Proj（ビュー空間なので射影のみ）
        float4 cpos = mul(float4(vSample, 1.0f), g_matProj);
        if (cpos.w <= 0.0f)
            continue; // 後ろ側は無視

        // スクリーンUV
        float2 suv = NdcToUv(cpos);
        if (suv.x < 0.0f || suv.x > 1.0f || suv.y < 0.0f || suv.y > 1.0f)
            continue;

        // サンプル点の線形Z（near..far → 0..1）
        float zNeighbor = saturate((vSample.z - g_fNear) / (g_fFar - g_fNear));

        // Z画像（αに線形Z）
        float zImage = tex2D(sampZ, suv).a;

        // 遮蔽判定（遠すぎる影は弾く軽いガード付き）
        if (zImage + g_aoBias < zNeighbor)
        {
            if (zNeighbor - zImage > 0.001f)  // 必要なければ外してOK
                continue;

            occ++;
        }
    }

    // AO 係数
    float ao = 1.0f - g_aoStrength * (occ / (float) kSamples);
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
