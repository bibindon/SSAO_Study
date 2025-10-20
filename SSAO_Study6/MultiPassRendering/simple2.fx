
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
float3 HemiDirFromIndex(int i);

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
    float3 T = normalize(cross(up, normalizedView));
    float3 B = cross(normalizedView, T);

    int occ = 0;
    const int kSamples = 64;

    [unroll]
    for (int k = 0; k < kSamples; ++k)
    {
        float3 h = HemiDirFromIndex(k);
        float3 dirV = normalize(T * h.x + B * h.y + normalizedView * h.z);

        float u = ((float) k + 0.5f) / (float) kSamples;
        float radius = g_aoStepWorld * (u * u);

        float3 vSample = vOrigin + dirV * radius;

        float4 clip = mul(float4(vSample, 1.0f), g_matProj);
        if (clip.w <= 0.0f)
            continue;

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
            occ++;
        }
    }

    float occl = (float) occ / (float) kSamples;
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

        float zL = tex2D(sampZ, uvL).a;
        float zR = tex2D(sampZ, uvR).a;

        // Z値が大きく異なる場所の陰はブラーに使わない
        if (abs(zL - centerZ) <= g_depthReject)
        {
            float aoL = tex2D(sampAO, uvL).r;
            sumAO += aoL * WIDTH;
            sumW += WIDTH;
        }

        if (abs(zR - centerZ) <= g_depthReject)
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

        float zD = tex2D(sampZ, uvD).a;
        float zU = tex2D(sampZ, uvU).a;

        if (abs(zD - centerZ) <= g_depthReject)
        {
            float aoD = tex2D(sampAO, uvD).r;
            sumAO += aoD * WIDTH;
            sumW += WIDTH;
        }

        if (abs(zU - centerZ) <= g_depthReject)
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
    i.uv.x += g_invSize.x;
    i.uv.y += g_invSize.y;

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

float3 HemiDirFromIndex(int i)
{
    float a = frac(0.754877666f * (i + 0.5f));
    float b = frac(0.569840296f * (i + 0.5f));
    float phi = a * 6.2831853f;
    float c = b; // cos(theta) in [0,1]
    float s = sqrt(saturate(1.0f - c * c));
    return float3(cos(phi) * s, sin(phi) * s, c);
}

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

    float zC = tex2D(sampZ, uv).a;
    float3 pC = DecodeWorldPos(tex2D(sampPos, uv).rgb);

    float2 dx = float2(g_invSize.x, 0.0f) * 2;
    float2 dy = float2(0.0f, g_invSize.y) * 2;

    float3 pR = DecodeWorldPos(tex2D(sampPos, uv + dx).rgb);
    float3 pL = DecodeWorldPos(tex2D(sampPos, uv - dx).rgb);
    float3 pU = DecodeWorldPos(tex2D(sampPos, uv - dy).rgb);
    float3 pD = DecodeWorldPos(tex2D(sampPos, uv + dy).rgb);

    float zR = tex2D(sampZ, uv + dx).a;
    float zL = tex2D(sampZ, uv - dx).a;
    float zU = tex2D(sampZ, uv - dy).a;
    float zD = tex2D(sampZ, uv + dy).a;

    // --- “輪郭かつ遠側を採るか” をレンジで判定 ---
    float dzX = abs(zR - zL);
    float dzY = abs(zD - zU);

    bool adoptFarX = false;
    bool adoptFarY = false;

    if ((dzX >= g_farAdoptMinZ) && (dzX <= g_farAdoptMaxZ))
    {
        adoptFarX = true;
    }

    if ((dzY >= g_farAdoptMinZ) && (dzY <= g_farAdoptMaxZ))
    {
        adoptFarY = true;
    }

    // 法線用の差分：軸ごとにレンジ内なら FAR 側、そうでなければセンターに近い側
    float3 vx;
    if (adoptFarX)
    {
        if (zR > zL)
        {
            vx = pR - pC;
        }
        else
        {
            vx = pC - pL;
        }
    }
    else
    {
        if (abs(zR - zC) <= abs(zL - zC))
        {
            vx = pR - pC;
        }
        else
        {
            vx = pC - pL;
        }
    }

    float3 vy;
    if (adoptFarY)
    {
        if (zD > zU)
        {
            vy = pD - pC;
        }
        else
        {
            vy = pC - pU;
        }
    }
    else
    {
        if (abs(zD - zC) <= abs(zU - zC))
        {
            vy = pD - pC;
        }
        else
        {
            vy = pC - pU;
        }
    }

    float3 normalizedWorld = normalize(cross(vx, vy));
    float3 normalizedView = normalize(mul(float4(normalizedWorld, 0), g_matView).xyz);

    // 原点（位置）：どちらかの軸で採用する場合は、その軸の “より遠い方” を使う
    float zFarN = zC;
    float3 pFarN = pC;
    if (adoptFarX)
    {
        float zX = max(zR, zL);
        float3 pX = (zR > zL) ? pR : pL;
        if (zX > zFarN)
        {
            zFarN = zX;
            pFarN = pX;
        }
    }

    if (adoptFarY)
    {
        float zY = max(zD, zU);
        float3 pY = (zD > zU) ? pD : pU;
        if (zY > zFarN)
        {
            zFarN = zY;
            pFarN = pY;
        }
    }

    // 参照Z（zRef）は従来どおり“遠い側”を使って明るいハロを防止（ここはレンジ外でもOK）
    const float kEdge = 0.004f; // 以前の kEdge（シルエット検出） – 必要なら 0.003〜0.006
    float zRef = zC;
    float3 pRef = pC;
    if (abs(zR - zL) > kEdge)
    {
        if (zR > zRef)
        {
            zRef = zR;
            pRef = pR;
        }

        if (zL > zRef)
        {
            zRef = zL;
            pRef = pL;
        }
    }

    if (abs(zD - zU) > kEdge)
    {
        if (zD > zRef)
        {
            zRef = zD;
            pRef = pD;
        }

        if (zU > zRef)
        {
            zRef = zU;
            pRef = pU;
        }
    }

    // 出力（原点はレンジガード付きの選択、それ以外は従来どおり）
    result.normalizedView = normalizedView;
    result.vOrigin = mul(float4(pFarN, 1.0f), g_matView).xyz; // 採用しない場合は pC になる
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

