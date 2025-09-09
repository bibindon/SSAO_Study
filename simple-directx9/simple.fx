// ===== Matrices =====
float4x4 g_matWorld;
float4x4 g_matView;
float4x4 g_matProj;
float4x4 g_matWorldViewProj;

// ===== Camera params =====
float2 g_NearFar; // (near, far)  左手系

// ===== Scene lighting (simple) =====
float3 g_LightDir = normalize(float3(0.3, 1.0, 0.5));
float3 g_Ambient = float3(0.3, 0.3, 0.3);

// ===== RTTs =====
texture tScene;
sampler2D sScene = sampler_state
{
    Texture = <tScene>;
    MinFilter = LINEAR;
    MagFilter = LINEAR;
    MipFilter = LINEAR;
    AddressU = CLAMP;
    AddressV = CLAMP;
};

texture tDepthNormal;
sampler2D sDepthNormal = sampler_state
{
    Texture = <tDepthNormal>;
    MinFilter = POINT;
    MagFilter = POINT;
    MipFilter = POINT;
    AddressU = CLAMP;
    AddressV = CLAMP;
};

// ===== SSAO params =====
float2 g_TexelSize; // (1/w, 1/h)
float g_SampleRadius = 12.0;
float g_Bias = 0.002;
float g_Intensity = 1.2;

// ---------------------------------------------------------
// 1) Depth + Normal : view空間法線 + **線形深度(LH)** を格納
//   RGB = (nx, ny, depthLinear)
//   ※ LH では vpos.z は near..far の正値。負号はつけない。
// ---------------------------------------------------------
struct DN_VSOUT
{
    float4 pos : POSITION;
    float3 nrmVS : TEXCOORD0;
    float zView : TEXCOORD1; // view-space z（LH: 前方が +）
};
DN_VSOUT VS_DepthNormal(float4 pos : POSITION, float3 nrm : NORMAL)
{
    DN_VSOUT o;
    float4 wpos = mul(pos, g_matWorld);
    float3 nW = normalize(mul(float4(nrm, 0), g_matWorld).xyz);
    float3 nV = normalize(mul(float4(nW, 0), g_matView).xyz);
    float4 vpos = mul(wpos, g_matView);

    o.pos = mul(vpos, g_matProj);
    o.nrmVS = nV;
    o.zView = vpos.z; // ← LH は正の深度
    return o;
}
float4 PS_DepthNormal(DN_VSOUT IN) : COLOR
{
    float nearZ = g_NearFar.x;
    float farZ = g_NearFar.y;

    // **LH正の深度**を 0..1 に正規化（前=0, 奥=1）
    float depthLinear = saturate((IN.zView - nearZ) / (farZ - nearZ));

    float2 encN = IN.nrmVS.xy * 0.5 + 0.5; // [-1,1]→[0,1]
    return float4(encN, depthLinear, 1.0);
}

// ---------------------------------------------------------
// 2) Scene color （Lambert）
// ---------------------------------------------------------
struct SC_VSOUT
{
    float4 pos : POSITION;
    float ndl : TEXCOORD0;
};
SC_VSOUT VS_Scene(float4 pos : POSITION, float3 nrm : NORMAL)
{
    SC_VSOUT o;
    float3 nW = normalize(mul(float4(nrm, 0), g_matWorld).xyz);
    o.ndl = max(0, dot(nW, normalize(g_LightDir)));
    o.pos = mul(pos, g_matWorldViewProj);
    return o;
}
float4 PS_Scene(SC_VSOUT IN) : COLOR
{
    float3 albedo = float3(0.85, 0.85, 0.85);
    float3 col = albedo * (g_Ambient + IN.ndl);
    return float4(col, 1);
}

// ---------------------------------------------------------
// 3) 合成 & デバッグ
// ---------------------------------------------------------
float4 PS_ShowScene(float2 uv : TEXCOORD0) : COLOR
{
    return tex2D(sScene, uv);
}
float4 PS_ShowDepthNormal(float2 uv : TEXCOORD0) : COLOR
{
    return tex2D(sDepthNormal, uv);
}
float4 PS_ShowDepthOnly(float2 uv : TEXCOORD0) : COLOR
{
    float d = tex2D(sDepthNormal, uv).b;
    return float4(d, d, d, 1);
}

// SSAO（16サンプル、円形カーネル＋距離減衰＋深度依存スケール）
float4 PS_SSAO(float2 uv : TEXCOORD0) : COLOR
{
    float4 dnC = tex2D(sDepthNormal, uv);
    float depthC = dnC.b;

    if (depthC <= 0.0001 || depthC >= 0.9999)
        return tex2D(sScene, uv);

    // screen-space 半径を深度でスケール（遠いほど小さく）
    float scale = lerp(1.5, 0.6, depthC);
    float r = g_SampleRadius * scale;

    float2 dir[16] =
    {
        float2(1, 0), float2(0.9239, 0.3827), float2(0.7071, 0.7071), float2(0.3827, 0.9239),
        float2(0, 1), float2(-0.3827, 0.9239), float2(-0.7071, 0.7071), float2(-0.9239, 0.3827),
        float2(-1, 0), float2(-0.9239, -0.3827), float2(-0.7071, -0.7071), float2(-0.3827, -0.9239),
        float2(0, -1), float2(0.3827, -0.9239), float2(0.7071, -0.7071), float2(0.9239, -0.3827)
    };

    float occ = 0.0;
    [unroll]
    for (int i = 0; i < 16; i++)
    {
        float2 duv = dir[i] * g_TexelSize * r;
        float ds = tex2D(sDepthNormal, uv + duv).b;

        // **LH線形深度**: 手前ほど小さい → 近傍が“手前”なら遮蔽
        float stepOcc = (depthC - ds > g_Bias) ? 1.0 : 0.0;
        float w = 1.0 - (float) i / 15.0; // 近傍ほど重く
        occ += stepOcc * w;
    }
    occ /= 8.0; // 正規化係数（経験的）

    float ao = saturate(1.0 - occ * g_Intensity);
    float3 sceneCol = tex2D(sScene, uv).rgb;
    return float4(sceneCol * ao, 1.0);
}

// ---------------------------------------------------------
// Techniques (SM3.0)
// ---------------------------------------------------------
technique Tech_DepthNormal
{
    pass P
    {
        VertexShader = compile vs_3_0 VS_DepthNormal();
        PixelShader = compile ps_3_0 PS_DepthNormal();
    }
}
technique Tech_Scene
{
    pass P
    {
        VertexShader = compile vs_3_0 VS_Scene();
        PixelShader = compile ps_3_0 PS_Scene();
    }
}
technique Tech_Show_Scene
{
    pass P
    {
        PixelShader = compile ps_3_0 PS_ShowScene();
    }
}
technique Tech_Show_DN
{
    pass P
    {
        PixelShader = compile ps_3_0 PS_ShowDepthNormal();
    }
}
technique Tech_Show_Depth
{
    pass P
    {
        PixelShader = compile ps_3_0 PS_ShowDepthOnly();
    }
}
technique Tech_SSAOCombine
{
    pass P
    {
        PixelShader = compile ps_3_0 PS_SSAO();
    }
}
