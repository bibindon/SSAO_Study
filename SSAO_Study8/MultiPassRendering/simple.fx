float4x4 g_matWorld;
float4x4 g_matView;
float4x4 g_matWorldViewProj;

float g_fNear;
float g_fFar;

float g_posRange;

bool g_bUseTexture = false;

texture g_texBase;
sampler sampBase = sampler_state
{
    Texture   = (g_texBase);
    MinFilter = POINT;
    MagFilter = POINT;
    MipFilter = NONE;
    AddressU  = WRAP;
    AddressV  = WRAP;
};

// ------------------------------------------------------------
// VS: 法線（WS）を計算して TEXCOORD3 で渡す
// ------------------------------------------------------------
void VertexShader1(in  float4 inPosition   : POSITION,
                   in  float4 inNormal     : NORMAL0,
                   in  float4 inTexCood    : TEXCOORD0,

                   out float4 outPosition  : POSITION,
                   out float4 outDiffuse   : COLOR0,
                   out float4 outTexCood   : TEXCOORD0,
                   out float  outViewZ     : TEXCOORD1,
                   out float3 outWorldPos  : TEXCOORD2,
                   out float3 outNormalWS  : TEXCOORD3)
{
    float4 worldPos = mul(inPosition, g_matWorld);
    outPosition = mul(worldPos, g_matWorldViewProj);

    // 簡単なライティング（元コードを踏襲）
    float lightIntensity = 0.0f;

    // 平行光源によるライティングあり or なし
    if (true)
    {
        lightIntensity = dot(inNormal, normalize(float4(-0.3, 1.0, -0.5, 0)));
    }
    else
    {
        lightIntensity = 1.0f;
    }

    outDiffuse.rgb = max(0, lightIntensity) + 0.3;
    outDiffuse.a   = 1.0f;

    outTexCood = inTexCood;

    // View 空間 Z（線形化は PS で実施）
    float4 vpos = mul(worldPos, g_matView);
    outViewZ = vpos.z;

    // ワールド座標
    outWorldPos = worldPos.xyz;

    // ワールド法線
    // 本来は inverse-transpose(g_matWorld) を使うが、回転・等方スケール前提ならこれで十分
    float3 nWS = mul(inNormal.xyz, (float3x3)g_matWorld);
    outNormalWS = normalize(nWS);
}

// ------------------------------------------------------------
// PS: MRT4
// COLOR0: Color
// COLOR1: LinearZ (rrrr)
// COLOR2: PosWS（g_posRange で正規化して 0..1 エンコード）
// COLOR3: NormalWS（-1..1 → 0..1 エンコード）
// ------------------------------------------------------------
void PixelShaderMRT4(in  float4 inScreenColor : COLOR0,
                     in  float2 inTexCood     : TEXCOORD0,
                     in  float  inViewZ       : TEXCOORD1,
                     in  float3 inWorldPos    : TEXCOORD2,
                     in  float3 inNormalWS    : TEXCOORD3,

                     out float4 outColor      : COLOR0,
                     out float4 outZ          : COLOR1,
                     out float4 outPosWS      : COLOR2,
                     out float4 outNormalWS   : COLOR3)
{
    // ベース色
    float3 lit  = inScreenColor.rgb;
    float3 base = lit;

    if (g_bUseTexture)
    {
        float3 tex = tex2D(sampBase, inTexCood).rgb;
        base = tex * lit;
    }
    outColor = float4(base, 1.0f);

    // 線形 Z を 0..1 へ
    float linearZ = saturate((inViewZ - g_fNear) / (g_fFar - g_fNear));
    outZ = float4(linearZ, linearZ, linearZ, linearZ);

    // ワールド座標を -g_posRange..+g_posRange とみなして 0..1 へエンコード
    float3 normalizedPosWS = inWorldPos / g_posRange;
    float3 encPos = saturate(normalizedPosWS * 0.5 + 0.5);
    outPosWS = float4(encPos, 1.0f);

    // 法線（WS）を 0..1 へエンコード
    float3 encN = saturate(inNormalWS * 0.5 + 0.5);
    outNormalWS = float4(encN, 1.0f);
}

// MRT...Multi Render Target
technique TechniqueMRT
{
    pass P0
    {
        CullMode    = NONE;
        VertexShader = compile vs_3_0 VertexShader1();
        PixelShader  = compile ps_3_0 PixelShaderMRT4();
    }
}
