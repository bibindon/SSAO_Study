



float4x4 g_matView;
float4x4 g_matProj;

float g_fNear;
float g_fFar;

float2 g_invSize;
float g_posRange;

float g_aoStrength;
float g_aoStepWorld;

float g_edgeZ;

float g_depthReject;

float PI = 3.1415926535;

texture texZ;
texture texPos;
texture texNormal;

texture texAO;
texture texColor;

sampler sampZ = sampler_state
{
    Texture = (texZ);
    MinFilter = POINT;
    MagFilter = POINT;
    MipFilter = NONE;
    AddressU = CLAMP;
    AddressV = CLAMP;
};

sampler sampPos = sampler_state
{
    Texture = (texPos);
    MinFilter = POINT;
    MagFilter = POINT;
    MipFilter = NONE;
    AddressU = CLAMP;
    AddressV = CLAMP;
};

sampler sampNormal = sampler_state
{
    Texture   = (texNormal);
    MinFilter = POINT;
    MagFilter = POINT;
    MipFilter = NONE;
    AddressU  = CLAMP;
    AddressV  = CLAMP;
};

sampler sampAO = sampler_state
{
    Texture = (texAO);
    MinFilter = POINT;
    MagFilter = POINT;
    MipFilter = NONE;
    AddressU = CLAMP;
    AddressV = CLAMP;
};

sampler sampColor = sampler_state
{
    Texture = (texColor);
    MinFilter = POINT;
    MagFilter = POINT;
    MipFilter = NONE;
    AddressU = CLAMP;
    AddressV = CLAMP;
};

float3 DecodeNormalWS(float3 enc01)
{
    // 0..1 → -1..1 に戻して正規化
    return normalize(enc01 * 2.0f - 1.0f);
}

//-----------------------------------------------------------------
// 頂点シェーダー
//-----------------------------------------------------------------
struct VS_OUT
{
    float4 pos : POSITION;
    float2 uv : TEXCOORD0;
};

VS_OUT VS_Fullscreen(float4 p : POSITION, float2 uv : TEXCOORD0)
{
    VS_OUT o;
    o.pos = p;
    o.uv = uv;
    return o;
}

float3 DecodeWorldPos(float3 enc);

float2 PolygonToUV(float4 vClip);

// Low-discrepancy hemisphere dir
float3 RandomHemiDir(int in_);

// BuildBasis関数の戻り値用構造体
// HLSLは複数の戻り値を戻したい場合、構造体しか方法がない。
struct Basis
{
    float3 vHemisphereAxisVS;
    float3 vNormalizedNormalWS;
    float3 vOriginVS;
    float fZRef;
};

Basis BuildBasis(float2 uv);

//-------------------------------------------------------------
// Ambient Occlusion
//-------------------------------------------------------------
float4 PS_AO(VS_OUT in_) : COLOR0
{
    in_.uv += g_invSize * 0.5f;
    
    // 中心の WS 位置と WS 法線を取得
    float3 posWS_center = DecodeWorldPos(tex2D(sampPos,    in_.uv).rgb);
    float3 nWS_center   = DecodeNormalWS( tex2D(sampNormal, in_.uv).rgb );

    // 半球軸：WS 法線を View 空間へ
    float3 vHemisphereAxisVS = normalize(mul(float4(nWS_center, 0), g_matView).xyz);

    // 原点：中心位置を View 空間へ
    float3 vOriginVS = mul(float4(posWS_center, 1.0f), g_matView).xyz;

    // ===== 以下は従来のまま（TBN を作り、半球内でサンプルして可視判定） =====
    float3 vUp = float3(0, 0, 0);
    if (abs(vHemisphereAxisVS.z) < 0.999f)
    {
        vUp.z = 1.f;
    }

    if (vUp.z == 0.f)
    {
        vUp.y = 1.f;
    }

    float3 vTangentVS = normalize(cross(vUp, vHemisphereAxisVS));
    float3 vBinormalVS = cross(vHemisphereAxisVS, vTangentVS);

    int occlusionNum = 0;
    const int kSamples = 64;

    [unroll]
    for (int i = 0; i < kSamples; ++i)
    {
        float3 vRandomDir = RandomHemiDir(i);
        float3 vRandomDirVS = normalize(vTangentVS * vRandomDir.x +
                                    vBinormalVS * vRandomDir.y +
                                    vHemisphereAxisVS * vRandomDir.z);

        float fNormalizedIndex = ((float) i + 0.5f) / (float) kSamples;
        float fRadius = g_aoStepWorld * (fNormalizedIndex * fNormalizedIndex);

        float3 vSamplePosVS = vOriginVS + vRandomDirVS * fRadius;

        float4 vClip = mul(float4(vSamplePosVS, 1.0f), g_matProj);
        if (vClip.w <= 0.0f)
            continue;

        float2 sampleUV = PolygonToUV(vClip);
        if (sampleUV.x < 0.0f || sampleUV.x > 1.0f ||
        sampleUV.y < 0.0f || sampleUV.y > 1.0f)
            continue;

        float Z_SampleInUV = tex2D(sampZ, sampleUV).a;
        float Z_CenterInUV = tex2D(sampZ, in_.uv).a;

        if (abs(Z_SampleInUV - Z_CenterInUV) > g_edgeZ)
            continue;

        float Z_SampleInRay = saturate((vSamplePosVS.z - g_fNear) / (g_fFar - g_fNear));

        float fOcclusionMin = 0.0001f * (g_posRange / 8);
        if (Z_SampleInRay - Z_SampleInUV > fOcclusionMin)
        {
            occlusionNum++;
        }
    }

    float fOcclusionRate = (float) occlusionNum / (float) kSamples;
    float fBrightness = 1.0f - g_aoStrength * fOcclusionRate;

    return float4(saturate(fBrightness).xxx, 1.0f);
}

//--------------------------------------------------------------
// Blur H
//--------------------------------------------------------------
float4 PS_BlurH(VS_OUT in_) : COLOR0
{
    in_.uv += g_invSize * 0.5f;

    // 奇数であること
    const int WIDTH = 25;

    float centerZ = tex2D(sampZ, in_.uv).a;
    float centerAO = tex2D(sampAO, in_.uv).r;

    float2 stepUV = float2(g_invSize.x, 0.0f);

    float sumAO = centerAO;
    float sumW = 1.0f;

    [unroll]
    for (int i = 1; i < (WIDTH / 2); ++i)
    {
        float2 uvL = in_.uv - stepUV * i;
        float2 uvR = in_.uv + stepUV * i;

        float fZLeft = tex2D(sampZ, uvL).a;
        float fZRight = tex2D(sampZ, uvR).a;

        // Z値が大きく異なる場所の陰はブラーに使わない
        if (abs(fZLeft - centerZ) <= g_depthReject)
        {
            float aoL = tex2D(sampAO, uvL).r;
            sumAO += aoL * WIDTH;
            sumW += WIDTH;
        }

        if (abs(fZRight - centerZ) <= g_depthReject)
        {
            float aoR = tex2D(sampAO, uvR).r;
            sumAO += aoR * WIDTH;
            sumW += WIDTH;
        }
    }

    float ao = sumAO / sumW;
    return float4(ao, ao, ao, 1.0f);
}

//--------------------------------------------------------------
// Blur V
//--------------------------------------------------------------
float4 PS_BlurV(VS_OUT in_) : COLOR0
{
    in_.uv += g_invSize * 0.5f;

    // 奇数であること
    const int WIDTH = 25;

    float centerZ = tex2D(sampZ, in_.uv).a;
    float centerAO = tex2D(sampAO, in_.uv).r;

    float2 stepUV = float2(0.0f, g_invSize.y);

    float sumAO = centerAO;
    float sumW = 1.0f;

    [unroll]
    for (int i = 1; i < (WIDTH / 2); ++i)
    {
        float2 uvD = in_.uv + stepUV * i;
        float2 uvU = in_.uv - stepUV * i;

        float fZDown = tex2D(sampZ, uvD).a;
        float fZUp = tex2D(sampZ, uvU).a;

        if (abs(fZDown - centerZ) <= g_depthReject)
        {
            float aoD = tex2D(sampAO, uvD).r;
            sumAO += aoD * WIDTH;
            sumW += WIDTH;
        }

        if (abs(fZUp - centerZ) <= g_depthReject)
        {
            float aoU = tex2D(sampAO, uvU).r;
            sumAO += aoU * WIDTH;
            sumW += WIDTH;
        }
    }

    float ao = sumAO / sumW;
    return float4(ao, ao, ao, 1.0f);
}

//--------------------------------------------------------------
// Composite
//--------------------------------------------------------------
float4 PS_Composite(VS_OUT in_) : COLOR0
{
    float2 uv2 = in_.uv;
    float2 uv3 = in_.uv;

    float3 col = tex2D(sampColor, uv3).rgb;

    float ao = tex2D(sampAO, uv2).r;

    return float4(col * ao, 1.0f);
}

//--------------------------------------------------------------
// Other functions
//--------------------------------------------------------------
float3 DecodeWorldPos(float3 enc)
{
    return (enc * 2.0f - 1.0f) * g_posRange;
}

// ランダムな方向を返す。ただし半球状
float3 RandomHemiDir(int index)
{
    // 準乱数（0..1）
    // frac関数は小数部分を返す
    float randomU1 = frac(0.754877666f * (index + 0.5f));
    float randomU2 = frac(0.569840296f * (index + 0.5f));

    float angle  = randomU1 * PI * 2;

    // z 成分
    // sin2乗 + cos2乗 = 1、というのがある。
    // 変形すると以下のようになる
    // sin = ルート(1 - cos2乗)
    float cosTheta  = randomU2;
    float sinTheta  = sqrt(1.0f - cosTheta * cosTheta);

    float3 directionLocal;
    directionLocal.x = cos(angle) * sinTheta;
    directionLocal.y = sin(angle) * sinTheta;
    directionLocal.z = cosTheta;               // +Z 半球
    return directionLocal;                     // 既に単位長
}

// -1 ~ +1を0 ~ 1にする
float2 PolygonToUV(float4 vClip)
{
    float2 Polygon = vClip.xy / vClip.w;
    float2 uv;

    uv.x = Polygon.x * 0.5f + 0.5f;
    uv.y = -Polygon.y * 0.5f + 0.5f;

    return uv;
}

technique TechniqueAO_Create
{
    pass P0
    {
        CullMode = NONE;
        VertexShader = compile vs_3_0 VS_Fullscreen();
        PixelShader = compile ps_3_0 PS_AO();
    }
}

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

technique TechniqueAO_Composite
{
    pass P0
    {
        CullMode = NONE;
        VertexShader = compile vs_3_0 VS_Fullscreen();
        PixelShader = compile ps_3_0 PS_Composite();
    }
}

