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

float4 PS_AO(VS_OUT i) : COLOR0
{
    float4 color = tex2D(sampColor, i.uv);
    float centerZ = tex2D(sampZ, i.uv).a; // 0..1（線形Z）

    float3 worldPos = DecodeWorldPos(tex2D(sampPos, i.uv).rgb);

    // 6方向
    float3 dirs[6] =
    {
        float3(1, 0, 0), float3(-1, 0, 0),
        float3(0, 1, 0), float3(0, -1, 0),
        float3(0, 0, 1), float3(0, 0, -1)
    };

    int occ = 0;
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

        float zImage = tex2D(sampZ, suv).a; // 0..1
        if (zImage + g_aoBias < centerZ)
            occ++;
    }

    float ao = 1.0f - g_aoStrength * (occ / 6.0f);
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
