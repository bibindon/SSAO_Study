// simple.fx  — UTF-8 (BOMなし)
// MRT3: RT0=color, RT1=Z画像 (RGB=可視化, A=linearZ 0..1), RT2=WorldPos(0..1エンコード)

float4x4 g_matWorldViewProj;
float4x4 g_matWorld;
float4x4 g_matView;
float4x4 g_matProj;

float g_fNear = 1.0f;
float g_fFar = 10000.0f;

// Z 可視化用
float g_vizMax = 100.0f;
float g_vizGamma = 0.25f;

// World 座標エンコード用
float4 g_posCenter = float4(0, 0, 0, 0);
float g_posRange = 50.0f;

float4 g_lightNormal = float4(0.3f, 1.0f, 0.5f, 0.0f);
float3 g_ambient = float3(0.3f, 0.3f, 0.3f);

bool g_bUseTexture = true;

texture texture1;
sampler textureSampler = sampler_state
{
    Texture = (texture1);
    MipFilter = LINEAR;
    MinFilter = LINEAR;
    MagFilter = LINEAR;
};

void VertexShader1(
    in float4 inPosition : POSITION,
    in float4 inNormal : NORMAL0,
    in float4 inTexCood : TEXCOORD0,

    out float4 outPosition : POSITION,
    out float4 outDiffuse : COLOR0,
    out float4 outTexCood : TEXCOORD0,
    out float outViewZ : TEXCOORD1,
    out float3 outWorldPos : TEXCOORD2
)
{
    float4 worldPos = mul(inPosition, g_matWorld);
    outPosition = mul(mul(worldPos, g_matView), g_matProj);

    float lightIntensity = dot(inNormal, g_lightNormal);
    outDiffuse.rgb = max(0, lightIntensity) + g_ambient;
    outDiffuse.a = 1.0f;

    outTexCood = inTexCood;

    float4 vpos = mul(worldPos, g_matView);
    outViewZ = vpos.z;

    outWorldPos = worldPos.xyz;
}

void PixelShader1(
    in float4 inScreenColor : COLOR0,
    in float2 inTexCood : TEXCOORD0,
    out float4 outColor : COLOR
)
{
    float4 sampled = tex2D(textureSampler, inTexCood);
    float4 baseColor = g_bUseTexture ? (inScreenColor * sampled) : inScreenColor;
    outColor = baseColor;
}

void PixelShaderMRT3(
    in float4 inScreenColor : COLOR0,
    in float2 inTexCood : TEXCOORD0,
    in float inViewZ : TEXCOORD1,
    in float3 inWorldPos : TEXCOORD2,
    out float4 outColor0 : COLOR0, // Color
    out float4 outColor1 : COLOR1, // Z画像
    out float4 outColor2 : COLOR2 // POS画像
)
{
    float4 sampled = tex2D(textureSampler, inTexCood);
    float4 baseColor = g_bUseTexture ? (inScreenColor * sampled) : inScreenColor;

    // RT0: 色
    outColor0 = baseColor;

    // 線形Z（near..far → 0..1）
    float linearZ = saturate((inViewZ - g_fNear) / (g_fFar - g_fNear));

    // 可視化用（vizMaxまでを0..1, ガンマ持ち上げ）
    float viz = saturate((inViewZ - g_fNear) / (g_vizMax - g_fNear));
    viz = pow(viz, g_vizGamma);

    // RT1: RGB=可視化, A=線形Z
    outColor1 = float4(viz, viz, viz, linearZ);

    // RT2: World座標を0..1にエンコード
    float3 nrm = (inWorldPos - g_posCenter.xyz) / g_posRange; // -1..1
    float3 enc = saturate(nrm * 0.5 + 0.5); //  0..1
    outColor2 = float4(enc, 1.0f);
}

technique Technique1
{
    pass P0
    {
        CullMode = NONE;
        VertexShader = compile vs_3_0 VertexShader1();
        PixelShader = compile ps_3_0 PixelShader1();
    }
}

technique TechniqueMRT
{
    pass P0
    {
        CullMode = NONE;
        VertexShader = compile vs_3_0 VertexShader1();
        PixelShader = compile ps_3_0 PixelShaderMRT3();
    }
}
