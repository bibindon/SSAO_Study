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

// PS_AO — ビュー空間(カメラ基準)の上下(±Yv)だけをサンプリングする版
// ※ g_aoStepWorld は“ビュー空間での距離”として使われます
/*
float4 PS_AO(VS_OUT i) : COLOR0
{
    float4 color = tex2D(sampColor, i.uv);

    // 中心点：POS画像→ワールド→ビュー空間へ
    float3 worldPos = DecodeWorldPos(tex2D(sampPos, i.uv).rgb);
    float3 vCenter = mul(float4(worldPos, 1.0f), g_matView).xyz;

    // ビュー空間の上下方向のみ（6回分の重複サンプル）
    const float3 dirsV[6] =
    {
//        float3(1, 0, 0), float3(-1, 0, 0),
//        float3(0, 1, 0), float3(0, -1, 0),
//        float3(0, 0, 1), float3(0, 0, -1),
//        float3(0, 0.5, 0), float3(0, -0.5, 0),
//        float3(0, 0.5, 0), float3(0, -0.5, 0),
//        float3(0, 0.5, 0), float3(0, -0.5, 0),
        float3(0, 1, 0), float3(0, -1.0, 0),
        float3(0, 1, 0), float3(0, -1.0, 0),
        float3(0, 1, 0), float3(0, -1.0, 0),
    };

    int occ = 0;
    [unroll]
    for (int k = 0; k < 6; ++k)
    {
        // ビュー空間でオフセット（カメラ基準の上下）
        float3 vSample = vCenter + dirsV[k] * g_aoStepWorld;

        // View→Proj（ビュー空間なので射影だけでOK）
        float4 cpos = mul(float4(vSample, 1.0f), g_matProj);
        if (cpos.w <= 0.0f)
            continue; // 後ろ側は無視

        // スクリーンUVへ変換
        float2 suv = NdcToUv(cpos);
        if (suv.x < 0.0f || suv.x > 1.0f || suv.y < 0.0f || suv.y > 1.0f)
        {
            continue;
        }

        // サンプル点の線形Z（near..far → 0..1）
        float zNeighbor = saturate((vSample.z - g_fNear) / (g_fFar - g_fNear));

        // Z画像（αに格納された線形Z）
        float zImage = tex2D(sampZ, suv).a;

        // 画像の方が手前にあれば遮蔽
        if (zImage + g_aoBias < zNeighbor)
        {
            // あまりに離れているなら無効
            if (zNeighbor - zImage > 0.001f)
            {
                continue;
            }
            occ++;
        }
    }

    float ao = 1.0f - g_aoStrength * (occ / 6.0f);
    ao = saturate(ao);

    return float4(color.rgb * ao, color.a);
}
*/

float4 PS_AO(VS_OUT i) : COLOR0
{
    float4 color = tex2D(sampColor, i.uv);

    // 中心点：POS画像→ワールド→ビュー空間へ
    float3 worldPos = DecodeWorldPos(tex2D(sampPos, i.uv).rgb);
    float3 vCenter = mul(float4(worldPos, 1.0f), g_matView).xyz;

    // 乱数シード（画素毎に異なる値）
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
        float radius = (r1 * r1) * g_aoStepWorld; // ※g_aoStepWorld を"最大半径"として利用

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

        // 遮蔽判定（画像の方が手前にあれば遮蔽）
        if (zImage + g_aoBias < zNeighbor)
        {
            // 遠すぎる影は弾く（オプション）
            if (zNeighbor - zImage > 0.001f)  // 閾値は調整可能
                continue;

            occ++;
        }
    }

    // AO 係数
    float ao = 1.0f - g_aoStrength * (occ / (float) kSamples);
    ao = saturate(ao);

    return float4(color.rgb * ao, color.a);
}

/*
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
*/

// ---- 便利関数：インデックスだけから一定の半球方向を生成（ピクセル非依存）----
float3 HemiDirFromIndex(int k)
{
    // 疑似乱数（k のみ依存）→ 角度に変換
    float a = frac(sin((k + 1) * 12.9898f) * 43758.5453f); // [0,1)
    float b = frac(sin((k + 1) * 78.2330f) * 19341.2710f); // [0,1)
    float phi = a * 6.2831853f; // [0, 2π)
    float cosTheta = b; // [0,1]  （半球）
    float sinTheta = sqrt(saturate(1.0f - cosTheta * cosTheta));
    return float3(cos(phi) * sinTheta, // x
                  sin(phi) * sinTheta, // y
                  cosTheta); // z >= 0
}

/*
// ---- 固定カーネル32本・二乗分布で「近くほど多い」 ----
// g_aoStepWorld は「最大半径」として使います。
float4 PS_AO(VS_OUT i) : COLOR0
{
    float4 color = tex2D(sampColor, i.uv);

    // 中心点：POS→World→View
    float3 worldPos = DecodeWorldPos(tex2D(sampPos, i.uv).rgb);
    float3 vCenter = mul(float4(worldPos, 1.0f), g_matView).xyz;

    // 法線ベースの半球へ向けたい場合はコメント解除（推奨）
    float3 Nw = normalize(cross(ddx(worldPos), ddy(worldPos)));
    float3 Nv = normalize(mul(float4(Nw, 0), g_matView).xyz); // View空間法線
    float3 up = (abs(Nv.z) < 0.999f) ? float3(0, 0, 1) : float3(0, 1, 0);
    float3 T = normalize(cross(up, Nv));
    float3 B = cross(Nv, T);

    const int kSamples = 32;
    int occ = 0;

    [unroll]
    for (int k = 0; k < kSamples; ++k)
    {
        // 固定カーネル方向（+Z半球）を法線半球へ回す
        float3 h = HemiDirFromIndex(k); // ローカル(+Z)半球
        float3 dirV = normalize(T * h.x + B * h.y + Nv * h.z); // View空間へ

        // 近いほど密：半径 scale = ( (k+0.5)/N )^2
        float s = ((float) k + 0.5f) / (float) kSamples;
        float radius = g_aoStepWorld * (s * s); // [0..g_aoStepWorld]

        float3 vSample = vCenter + dirV * radius;

        // View→Clip（View空間なので射影だけ）
        float4 cpos = mul(float4(vSample, 1.0f), g_matProj);
        if (cpos.w <= 0.0f)
            continue;

        // UVに変換（D3D9はY反転）
        float2 suv = NdcToUv(cpos);
        if (suv.x < 0.0f || suv.x > 1.0f || suv.y < 0.0f || suv.y > 1.0f)
            continue;

        // サンプル点の線形Z、Z画像の線形Z（α）
        float zNeighbor = saturate((vSample.z - g_fNear) / (g_fFar - g_fNear));
        float zImage = tex2D(sampZ, suv).a;

        // 手前に形があれば遮蔽
        if (zImage + g_aoBias < zNeighbor)
        {
            if (zNeighbor - zImage > 0.001f)
            {
                occ++;
            }
        }
    }

    float ao = 1.0f - g_aoStrength * (occ / (float) kSamples);
    ao = saturate(ao);

    return float4(color.rgb * ao, color.a);
}
*/

technique TechniqueAO
{
    pass P0
    {
        CullMode = NONE;
        VertexShader = compile vs_3_0 VS_Fullscreen();
        PixelShader = compile ps_3_0 PS_AO();
    }
}
