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
float g_aoBias;
float g_aoPower;
int g_sampleCount;
int g_blurRadius;

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
    return normalize(enc01 * 2.0f - 1.0f);
}

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
float3 RandomHemiDir(int in_);
float Hash12(float2 p);
float2 Rotate2D(float2 v, float angle);

float4 PS_AO(VS_OUT in_) : COLOR0
{
    in_.uv += g_invSize * 0.5f;

    float3 posWS_center = DecodeWorldPos(tex2D(sampPos, in_.uv).rgb);
    float3 nWS_center = DecodeNormalWS(tex2D(sampNormal, in_.uv).rgb);
    float zCenterInUV = tex2D(sampZ, in_.uv).a;

    float3 vHemisphereAxisVS = normalize(mul(float4(nWS_center, 0), g_matView).xyz);
    float3 vOriginVS = mul(float4(posWS_center, 1.0f), g_matView).xyz;

    float3 vUp = (abs(vHemisphereAxisVS.z) < 0.999f) ? float3(0, 0, 1) : float3(0, 1, 0);
    float3 vTangentVS = normalize(cross(vUp, vHemisphereAxisVS));
    float3 vBinormalVS = cross(vHemisphereAxisVS, vTangentVS);

    float randomAngle = Hash12(in_.uv * float2(983.0f, 613.0f)) * (PI * 2.0f);

    float occlusionNum = 0.0f;
    const int kMaxSamples = 64;

    [unroll]
    for (int i = 0; i < kMaxSamples; ++i)
    {
        if (i >= g_sampleCount)
        {
            continue;
        }

        float3 vRandomDir = RandomHemiDir(i);
        if (g_sampleCount == 1)
        {
            vRandomDir = float3(0.0f, 0.0f, 1.0f);
        }
        else
        {
            vRandomDir.xy = Rotate2D(vRandomDir.xy, randomAngle);
        }

        float3 vRandomDirVS = normalize(vTangentVS * vRandomDir.x +
                                        vBinormalVS * vRandomDir.y +
                                        vHemisphereAxisVS * vRandomDir.z);

        float fNormalizedIndex = ((float)i + 0.5f) / (float)max(g_sampleCount, 1);
        float fRadius = g_aoStepWorld * lerp(0.15f, 1.0f, fNormalizedIndex * fNormalizedIndex);

        float3 vSamplePosVS = vOriginVS + vRandomDirVS * fRadius;
        float4 vClip = mul(float4(vSamplePosVS, 1.0f), g_matProj);
        if (vClip.w <= 0.0f)
        {
            continue;
        }

        float2 sampleUV = PolygonToUV(vClip);
        if (sampleUV.x < 0.0f || sampleUV.x > 1.0f || sampleUV.y < 0.0f || sampleUV.y > 1.0f)
        {
            continue;
        }

        float zSampleInUV = tex2D(sampZ, sampleUV).a;
        if (abs(zSampleInUV - zCenterInUV) > g_edgeZ)
        {
            continue;
        }

        float zSampleInRay = saturate((vSamplePosVS.z - g_fNear) / (g_fFar - g_fNear));
        if ((zSampleInRay - zSampleInUV) > g_aoBias)
        {
            occlusionNum += 1.0f;
        }
    }

    float fOcclusionRate = occlusionNum / (float)max(g_sampleCount, 1);
    float fBrightness = pow(saturate(1.0f - g_aoStrength * fOcclusionRate), g_aoPower);

    return float4(fBrightness.xxx, 1.0f);
}

float4 PS_BlurH(VS_OUT in_) : COLOR0
{
    in_.uv += g_invSize * 0.5f;

    float centerZ = tex2D(sampZ, in_.uv).a;
    float centerAO = tex2D(sampAO, in_.uv).r;
    float2 stepUV = float2(g_invSize.x, 0.0f);
    float sigma = max(1.0f, (float)g_blurRadius * 0.5f);

    float sumAO = centerAO;
    float sumW = 1.0f;

    const int kMaxBlurRadius = 16;
    [unroll]
    for (int i = 1; i <= kMaxBlurRadius; ++i)
    {
        if (i > g_blurRadius)
        {
            continue;
        }

        float2 uvL = in_.uv - stepUV * i;
        float2 uvR = in_.uv + stepUV * i;
        float weight = exp(-0.5f * ((float)(i * i)) / (sigma * sigma));

        float fZLeft = tex2D(sampZ, uvL).a;
        float fZRight = tex2D(sampZ, uvR).a;

        if (abs(fZLeft - centerZ) <= g_depthReject)
        {
            float aoL = tex2D(sampAO, uvL).r;
            sumAO += aoL * weight;
            sumW += weight;
        }

        if (abs(fZRight - centerZ) <= g_depthReject)
        {
            float aoR = tex2D(sampAO, uvR).r;
            sumAO += aoR * weight;
            sumW += weight;
        }
    }

    float ao = sumAO / sumW;
    return float4(ao, ao, ao, 1.0f);
}

float4 PS_BlurV(VS_OUT in_) : COLOR0
{
    in_.uv += g_invSize * 0.5f;

    float centerZ = tex2D(sampZ, in_.uv).a;
    float centerAO = tex2D(sampAO, in_.uv).r;
    float2 stepUV = float2(0.0f, g_invSize.y);
    float sigma = max(1.0f, (float)g_blurRadius * 0.5f);

    float sumAO = centerAO;
    float sumW = 1.0f;

    const int kMaxBlurRadius = 16;
    [unroll]
    for (int i = 1; i <= kMaxBlurRadius; ++i)
    {
        if (i > g_blurRadius)
        {
            continue;
        }

        float2 uvD = in_.uv + stepUV * i;
        float2 uvU = in_.uv - stepUV * i;
        float weight = exp(-0.5f * ((float)(i * i)) / (sigma * sigma));

        float fZDown = tex2D(sampZ, uvD).a;
        float fZUp = tex2D(sampZ, uvU).a;

        if (abs(fZDown - centerZ) <= g_depthReject)
        {
            float aoD = tex2D(sampAO, uvD).r;
            sumAO += aoD * weight;
            sumW += weight;
        }

        if (abs(fZUp - centerZ) <= g_depthReject)
        {
            float aoU = tex2D(sampAO, uvU).r;
            sumAO += aoU * weight;
            sumW += weight;
        }
    }

    float ao = sumAO / sumW;
    return float4(ao, ao, ao, 1.0f);
}

float4 PS_Composite(VS_OUT in_) : COLOR0
{
    float3 col = tex2D(sampColor, in_.uv).rgb;
    float ao = tex2D(sampAO, in_.uv).r;
    return float4(col * ao, 1.0f);
}

float3 DecodeWorldPos(float3 enc)
{
    return (enc * 2.0f - 1.0f) * g_posRange;
}

float3 RandomHemiDir(int index)
{
    float randomU1 = frac(0.754877666f * (index + 0.5f));
    float randomU2 = frac(0.569840296f * (index + 0.5f));

    float angle = randomU1 * PI * 2.0f;
    float cosTheta = randomU2;
    float sinTheta = sqrt(1.0f - cosTheta * cosTheta);

    float3 directionLocal;
    directionLocal.x = cos(angle) * sinTheta;
    directionLocal.y = sin(angle) * sinTheta;
    directionLocal.z = cosTheta;
    return directionLocal;
}

float Hash12(float2 p)
{
    float h = dot(p, float2(127.1f, 311.7f));
    return frac(sin(h) * 43758.5453f);
}

float2 Rotate2D(float2 v, float angle)
{
    float s = sin(angle);
    float c = cos(angle);
    return float2(c * v.x - s * v.y, s * v.x + c * v.y);
}

float2 PolygonToUV(float4 vClip)
{
    float2 polygon = vClip.xy / vClip.w;
    float2 uv;
    uv.x = polygon.x * 0.5f + 0.5f;
    uv.y = -polygon.y * 0.5f + 0.5f;
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
