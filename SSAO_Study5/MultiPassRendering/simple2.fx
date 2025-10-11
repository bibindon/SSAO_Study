// simple2.fx - 簡素化版
// SSAO処理

texture texColor;
sampler sampColor = sampler_state
{
    Texture = (texColor);
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

texture texPos;
sampler sampPos = sampler_state
{
    Texture = (texPos);
    MinFilter = LINEAR;
    MagFilter = LINEAR;
    MipFilter = LINEAR;
    AddressU = CLAMP;
    AddressV = CLAMP;
};

float4x4 g_matView;
float4x4 g_matProj;
float g_fNear = 1.0f;
float g_fFar = 1000.0f;

float g_posRange = 50.0f;

float g_aoStepWorld = 1.0f;
float g_aoStrength = 1.0f;
float g_aoBias = 0.00003f;

struct VS_OUT
{
    float4 pos : POSITION;
    float2 uv : TEXCOORD0;
};

VS_OUT VS_Fullscreen(float4 pos : POSITION, float2 uv : TEXCOORD0)
{
    VS_OUT o;
    o.pos = pos;
    o.uv = uv;
    return o;
}

float2 NdcToUv(float4 clip)
{
    float2 ndc = clip.xy / clip.w;
    float2 result = float2(0.f, 0.f);
    result.x = 0.5f * ndc.x + 0.5f;
    result.y = -0.5f * ndc.y + 0.5f;
    return result;
}

float3 DecodeWorldPos(float3 enc)
{
    float3 nrm = (enc - 0.5f) * 2.0f;
    return nrm * g_posRange;
}

float3 HemiDirFromIndex(int k)
{
    float a = frac(sin((k + 1) * 12.9898f) * 43758.5453f);
    float b = frac(sin((k + 1) * 78.2330f) * 19341.2710f);
    float phi = a * 6.2831853f;
    float cosTheta = b;
    float sinTheta = sqrt(saturate(1.0f - cosTheta * cosTheta));
    return float3(cos(phi) * sinTheta, sin(phi) * sinTheta, cosTheta);
}

float4 PS_AO(VS_OUT i) : COLOR0
{
    // 中心点（色は参照しません）
    float3 worldPos = DecodeWorldPos(tex2D(sampPos, i.uv).rgb);
    float3 vCenter = mul(float4(worldPos, 1.0f), g_matView).xyz;

    // 法線（既存ロジックのまま）
    float3 worldPosX = DecodeWorldPos(tex2D(sampPos, i.uv + float2(1.0f / 800.0f, 0)).rgb);
    float3 worldPosY = DecodeWorldPos(tex2D(sampPos, i.uv + float2(0, 1.0f / 600.0f)).rgb);
    float3 Nw = normalize(cross(worldPosX - worldPos, worldPosY - worldPos));
    float3 Nv = normalize(mul(float4(Nw, 0), g_matView).xyz);

    // 接線空間
    float3 up = (abs(Nv.z) < 0.999f) ? float3(0, 0, 1) : float3(0, 1, 0);
    float3 T = normalize(cross(up, Nv));
    float3 B = cross(Nv, T);

    int occ = 0;
    const int kSamples = 64;

    [unroll]
    for (int k = 0; k < kSamples; ++k)
    {
        float3 h = HemiDirFromIndex(k);
        float3 dirV = normalize(T * h.x + B * h.y + Nv * h.z);

        float s = ((float) k + 0.5f) / (float) kSamples;
        float radius = g_aoStepWorld * (s * s);
        float3 vSample = vCenter + dirV * radius;

        float4 cpos = mul(float4(vSample, 1.0f), g_matProj);
        if (cpos.w <= 0.0f)
        {
            continue;
        }

        float2 suv = NdcToUv(cpos);
        if (suv.x < 0.0f || suv.x > 1.0f || suv.y < 0.0f || suv.y > 1.0f)
        {
            continue;
        }

        float zNeighbor = saturate((vSample.z - g_fNear) / (g_fFar - g_fNear));
        float zImage = tex2D(sampZ, suv).a;

        if (zImage + g_aoBias < zNeighbor)
        {
            if (zNeighbor - zImage > 0.01f)
            {
                continue;
            }
            occ++;
        }
    }

    // AO 係数（白=影なし、黒=影）
    float ao = 1.0f - g_aoStrength * (occ / (float) kSamples);
    ao = saturate(ao);
    return float4(ao, ao, ao, 1.0f);
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

// 追加：AO入力
texture texAO;
sampler sampAO = sampler_state
{
    Texture = (texAO);
    MinFilter = LINEAR;
    MagFilter = LINEAR;
    MipFilter = NONE;
    AddressU = CLAMP;
    AddressV = CLAMP;
};
// 追加：ガウスのσ（ピクセル単位）。お好みで 6～12 あたりから調整。
float g_sigmaPx = 8.0f;

// 既存：1/RTサイズ（1/width, 1/height）
float2 g_invSize;

// 51tap ガウス（横）
float4 PS_BlurH(VS_OUT i) : COLOR0
{
    const int R = 25; // 半径
    float2 du = float2(g_invSize.x, 0.0);

    float center = tex2D(sampAO, i.uv).r;
    float asum = center;
    float wsum = 1.0;

    // σ^2 を先に計算
    float sigma2 = g_sigmaPx * g_sigmaPx;

    [unroll]
    for (int o = 1; o <= R; ++o)
    {
        float w = exp(-(o * o) / (2.0f * sigma2)); // ガウス重み
        float s1 = tex2D(sampAO, i.uv + du * o).r;
        float s2 = tex2D(sampAO, i.uv + du * -o).r;
        asum += (s1 + s2) * w;
        wsum += 2.0f * w;
    }

    float a = asum / wsum;
    return float4(a, a, a, 1.0f);
}

// 51tap ガウス（縦）
float4 PS_BlurV(VS_OUT i) : COLOR0
{
    const int R = 25;
    float2 dv = float2(0.0, g_invSize.y);

    float center = tex2D(sampAO, i.uv).r;
    float asum = center;
    float wsum = 1.0;

    float sigma2 = g_sigmaPx * g_sigmaPx;

    [unroll]
    for (int o = 1; o <= R; ++o)
    {
        float w = exp(-(o * o) / (2.0f * sigma2));
        float s1 = tex2D(sampAO, i.uv + dv * o).r;
        float s2 = tex2D(sampAO, i.uv + dv * -o).r;
        asum += (s1 + s2) * w;
        wsum += 2.0f * w;
    }

    float a = asum / wsum;
    return float4(a, a, a, 1.0f);
}

// 既存: AOを作る（PS_AO）
technique TechniqueAO_Create
{
    pass P0
    {
        CullMode = NONE;
        VertexShader = compile vs_3_0 VS_Fullscreen();
        PixelShader = compile ps_3_0 PS_AO();
    }
}

// 追加：横/縦ブラー
technique TechniqueAO_BlurH
{
    pass P0
    {
        CullMode = NONE;
        VertexShader = compile vs_3_0 VS_Fullscreen();
        PixelShader = compile ps_3_0 PS_BlurH();
    }
}
technique TechniqueAO_BlurV
{
    pass P0
    {
        CullMode = NONE;
        VertexShader = compile vs_3_0 VS_Fullscreen();
        PixelShader = compile ps_3_0 PS_BlurV();
    }
}
