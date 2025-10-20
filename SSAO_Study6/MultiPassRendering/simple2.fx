
float4x4 g_matView;
float4x4 g_matProj;

float g_fNear;
float g_fFar;

float2 g_invSize;
float g_posRange;

float g_aoStrength;
float g_aoStepWorld;
float g_aoBias;

float g_edgeZ;
float g_originPush;

float g_farAdoptMinZ;
float g_farAdoptMaxZ;

float g_depthReject;

float PI = 3.1415926535;

texture texZ;
texture texPos;

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

// D3D9 half-texel aware
float2 NdcToUv(float4 clip);

// Low-discrepancy hemisphere dir
float3 RandomHemiDir(int i);

// BuildBasis関数の戻り値用構造体
// HLSLは複数の戻り値を戻したい場合、構造体しか方法がない。
struct Basis
{
    float3 normalizedView;
    float3 vOrigin;
    float zRef;
};

Basis BuildBasis(float2 uv);

//-------------------------------------------------------------
// Ambient Occlusion
//-------------------------------------------------------------
float4 PS_AO(VS_OUT i) : COLOR0
{
    Basis basis = BuildBasis(i.uv);

    float3 normalizedView = basis.normalizedView;

    // small lift along +normalizedView
    float3 vOrigin = basis.vOrigin + normalizedView * (g_originPush * g_aoStepWorld);
    float zRef = basis.zRef;

    // TBN
    float3 up = (abs(normalizedView.z) < 0.999f) ? float3(0, 0, 1) : float3(0, 1, 0);
    float3 tangent = normalize(cross(up, normalizedView));
    float3 binormal = cross(normalizedView, tangent);

    int occlusionNum = 0;
    const int kSamples = 64;

    [unroll]
    for (int k = 0; k < kSamples; ++k)
    {
        float3 h = RandomHemiDir(k);
        float3 dirV = normalize(tangent * h.x + binormal * h.y + normalizedView * h.z);

        float u = ((float) k + 0.5f) / (float) kSamples;
        float radius = g_aoStepWorld * (u * u);

        float3 vSample = vOrigin + dirV * radius;

        float4 clip = mul(float4(vSample, 1.0f), g_matProj);
        if (clip.w <= 0.0f)
        {
            continue;
        }

        float2 suv = NdcToUv(clip);
        if (suv.x < 0.0f || suv.x > 1.0f || suv.y < 0.0f || suv.y > 1.0f)
        {
            continue;
        }

        // Edge guard: sample is valid if it's near the FAR side OR the center depth
        float zImg = tex2D(sampZ, suv).a;
        float zCtr = tex2D(sampZ, i.uv).a;
        if (abs(zImg - zRef) > g_edgeZ && abs(zImg - zCtr) > g_edgeZ)
        {
            continue;
        }

        // Depth test in linear-Z (no plane-based rejection here)
        float zNei = saturate((vSample.z - g_fNear) / (g_fFar - g_fNear));
        if (zImg + g_aoBias < zNei)
        {
            occlusionNum++;
        }
    }

    float occl = (float) occlusionNum / (float) kSamples;
    float ao = 1.0f - g_aoStrength * occl;

    return float4(saturate(ao).xxx, 1.0f);
}

//--------------------------------------------------------------
// Blur H
//--------------------------------------------------------------
float4 PS_BlurH(VS_OUT i) : COLOR0
{
    // 奇数であること
    const int WIDTH = 51;

    float centerZ = tex2D(sampZ, i.uv).a;
    float centerAO = tex2D(sampAO, i.uv).r;

    float2 stepUV = float2(g_invSize.x, 0.0f);

    float sumAO = centerAO;
    float sumW = 1.0f;

    [unroll]
    for (int k = 1; k < (WIDTH / 2); ++k)
    {
        float2 uvL = i.uv - stepUV * k;
        float2 uvR = i.uv + stepUV * k;

        float ZLeft = tex2D(sampZ, uvL).a;
        float ZRight = tex2D(sampZ, uvR).a;

        // Z値が大きく異なる場所の陰はブラーに使わない
        if (abs(ZLeft - centerZ) <= g_depthReject)
        {
            float aoL = tex2D(sampAO, uvL).r;
            sumAO += aoL * WIDTH;
            sumW += WIDTH;
        }

        if (abs(ZRight - centerZ) <= g_depthReject)
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
float4 PS_BlurV(VS_OUT i) : COLOR0
{
    // 奇数であること
    const int WIDTH = 51;

    float centerZ = tex2D(sampZ, i.uv).a;
    float centerAO = tex2D(sampAO, i.uv).r;

    float2 stepUV = float2(0.0f, g_invSize.y);

    float sumAO = centerAO;
    float sumW = 1.0f;

    [unroll]
    for (int k = 1; k < (WIDTH / 2); ++k)
    {
        float2 uvD = i.uv + stepUV * k;
        float2 uvU = i.uv - stepUV * k;

        float ZDown = tex2D(sampZ, uvD).a;
        float ZUp = tex2D(sampZ, uvU).a;

        if (abs(ZDown - centerZ) <= g_depthReject)
        {
            float aoD = tex2D(sampAO, uvD).r;
            sumAO += aoD * WIDTH;
            sumW += WIDTH;
        }

        if (abs(ZUp - centerZ) <= g_depthReject)
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
float4 PS_Composite(VS_OUT i) : COLOR0
{
    float3 col = tex2D(sampColor, i.uv).rgb;

    // なぜか1ピクセルズレている
    // i.uv.x += g_invSize.x;
    // i.uv.y += g_invSize.y;

    float ao = tex2D(sampAO, i.uv).r;
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
    float randomU1 = frac(0.754877666f * (index + 0.5f));
    float randomU2 = frac(0.569840296f * (index + 0.5f));

    // 方位角 φ と、cosθ を一様に取る（これで固有立体角で一様になる）
    float anglePhi  = randomU1 * 6.2831853f;   // = 2π
    float cosTheta  = randomU2;                // z 成分
    float sinTheta  = sqrt(1.0f - cosTheta * cosTheta);

    float3 directionLocal;
    directionLocal.x = cos(anglePhi) * sinTheta;
    directionLocal.y = sin(anglePhi) * sinTheta;
    directionLocal.z = cosTheta;               // +Z 半球
    return directionLocal;                     // 既に単位長
}

// Normalized Device Coordinates（正規化デバイス座標）
float2 NdcToUv(float4 clip)
{
    float2 ndc = clip.xy / clip.w;
    float2 uv;
    uv.x = ndc.x * 0.5f + 0.5f;
    uv.y = -ndc.y * 0.5f + 0.5f;
    return uv + 0.5f * g_invSize;
}

Basis BuildBasis(float2 uv)
{
    Basis result;

    float ZCenter = tex2D(sampZ, uv).a;
    float3 posCenter = DecodeWorldPos(tex2D(sampPos, uv).rgb);

    float2 dx = float2(g_invSize.x, 0.0f) * 2;
    float2 dy = float2(0.0f, g_invSize.y) * 2;

    float3 posRight = DecodeWorldPos(tex2D(sampPos, uv + dx).rgb);
    float3 posLeft  = DecodeWorldPos(tex2D(sampPos, uv - dx).rgb);
    float3 posUp    = DecodeWorldPos(tex2D(sampPos, uv - dy).rgb);
    float3 posDown  = DecodeWorldPos(tex2D(sampPos, uv + dy).rgb);

    float ZRight    = tex2D(sampZ, uv + dx).a;
    float ZLeft     = tex2D(sampZ, uv - dx).a;
    float ZUp       = tex2D(sampZ, uv - dy).a;
    float ZDown     = tex2D(sampZ, uv + dy).a;

    // --- “輪郭かつ遠側を採るか” をレンジで判定 ---
    float ZXDelta = abs(ZRight - ZLeft);
    float ZYDelta = abs(ZDown - ZUp);

    bool adoptFarX = false;
    bool adoptFarY = false;

    if ((ZXDelta >= g_farAdoptMinZ) && (ZXDelta <= g_farAdoptMaxZ))
    {
        adoptFarX = true;
    }

    if ((ZYDelta >= g_farAdoptMinZ) && (ZYDelta <= g_farAdoptMaxZ))
    {
        adoptFarY = true;
    }

    // 法線用の差分：軸ごとにレンジ内なら FAR 側、そうでなければセンターに近い側
    float3 diffX;
    if (adoptFarX)
    {
        if (ZRight > ZLeft)
        {
            diffX = posRight - posCenter;
        }
        else
        {
            diffX = posCenter - posLeft;
        }
    }
    else
    {
        if (abs(ZRight - ZCenter) <= abs(ZLeft - ZCenter))
        {
            diffX = posRight - posCenter;
        }
        else
        {
            diffX = posCenter - posLeft;
        }
    }

    float3 diffY;
    if (adoptFarY)
    {
        if (ZDown > ZUp)
        {
            diffY = posDown - posCenter;
        }
        else
        {
            diffY = posCenter - posUp;
        }
    }
    else
    {
        if (abs(ZDown - ZCenter) <= abs(ZUp - ZCenter))
        {
            diffY = posDown - posCenter;
        }
        else
        {
            diffY = posCenter - posUp;
        }
    }

    float3 normalizedWorld = normalize(cross(diffX, diffY));
    float3 normalizedView = normalize(mul(float4(normalizedWorld, 0), g_matView).xyz);

    // 原点（位置）：どちらかの軸で採用する場合は、その軸の “より遠い方” を使う
    float zFarN = ZCenter;
    float3 pFarN = posCenter;
    if (adoptFarX)
    {
        float zX = max(ZRight, ZLeft);

        float3 pX = float3(0, 0, 0);
        
        if (ZRight > ZLeft)
        {
            pX = posRight;
        }
        else
        {
            pX = posLeft;
        }

        if (zX > zFarN)
        {
            zFarN = zX;
            pFarN = pX;
        }
    }

    if (adoptFarY)
    {
        float zY = max(ZDown, ZUp);

        float3 pY = float3(0, 0, 0);
        if (ZDown > ZUp)
        {
            pY = posDown;
        }
        else
        {
            pY = posUp;
        }

        if (zY > zFarN)
        {
            zFarN = zY;
            pFarN = pY;
        }
    }

    // 参照Z（zRef）は従来どおり“遠い側”を使って明るいハロを防止（ここはレンジ外でもOK）
    const float kEdge = 0.004f; // 以前の kEdge（シルエット検出） – 必要なら 0.003〜0.006
    float zRef = ZCenter;
    float3 pRef = posCenter;
    if (abs(ZRight - ZLeft) > kEdge)
    {
        if (ZRight > zRef)
        {
            zRef = ZRight;
            pRef = posRight;
        }

        if (ZLeft > zRef)
        {
            zRef = ZLeft;
            pRef = posLeft;
        }
    }

    if (abs(ZDown - ZUp) > kEdge)
    {
        if (ZDown > zRef)
        {
            zRef = ZDown;
            pRef = posDown;
        }

        if (ZUp > zRef)
        {
            zRef = ZUp;
            pRef = posUp;
        }
    }

    // 出力（原点はレンジガード付きの選択、それ以外は従来どおり）
    result.normalizedView = normalizedView;

    // 採用しない場合は posCenter になる
    result.vOrigin = mul(float4(pFarN, 1.0f), g_matView).xyz;
    result.zRef = zRef;

    return result;
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

