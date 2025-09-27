// simple2.fx  — UTF-8 (BOMなし)
// 入力: texColor=RT0, texZ=RT1(RGB=可視化, A=linearZ), texPos=RT2
// 出力: AO を乗算したカラー
// 方法: 法線ベースの半球サンプリングによるSSAO

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

// 固定カーネル：半球方向を生成（ピクセル非依存）
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

// 法線ベース半球サンプリング版PS_AO
float4 PS_AO(VS_OUT i) : COLOR0
{
    float4 color = tex2D(sampColor, i.uv);

    // 中心点：POS→World→View
    float3 worldPos = DecodeWorldPos(tex2D(sampPos, i.uv).rgb);
    float3 vCenter = mul(float4(worldPos, 1.0f), g_matView).xyz;

    // ワールド空間の法線を計算（画面空間微分から）
    float3 worldPosX = DecodeWorldPos(tex2D(sampPos, i.uv + float2(1.0f / 1600.0f, 0)).rgb);
    float3 worldPosY = DecodeWorldPos(tex2D(sampPos, i.uv + float2(0, 1.0f / 900.0f)).rgb);
    
    float3 ddxWorld = worldPosX - worldPos;
    float3 ddyWorld = worldPosY - worldPos;
    float3 Nw = normalize(cross(ddxWorld, ddyWorld));
    
    // ワールド法線をビュー空間に変換
    float3 Nv = normalize(mul(float4(Nw, 0), g_matView).xyz);

    // 法線ベースの接線空間を構築
    float3 up = (abs(Nv.z) < 0.999f) ? float3(0, 0, 1) : float3(0, 1, 0);
    float3 T = normalize(cross(up, Nv));
    float3 B = cross(Nv, T);

    const int kSamples = 32;
    int occ = 0;

    [unroll]
    for (int k = 0; k < kSamples; ++k)
    {
        // 固定カーネル方向（+Z半球）を法線半球へ回転
        float3 h = HemiDirFromIndex(k); // ローカル(+Z)半球
        float3 dirV = normalize(T * h.x + B * h.y + Nv * h.z); // ビュー空間へ

        // 近距離重視：半径 scale = ((k+0.5)/N)^2
        float s = ((float) k + 0.5f) / (float) kSamples;
        float radius = g_aoStepWorld * (s * s); // [0..g_aoStepWorld]

        float3 vSample = vCenter + dirV * radius;

        // View→Clip（ビュー空間なので射影だけ）
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
                continue;

            occ++;
        }
    }

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