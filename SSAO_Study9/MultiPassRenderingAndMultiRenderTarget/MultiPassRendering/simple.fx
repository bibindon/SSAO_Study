float4x4 g_matWorldViewProj;
float4x4 g_matWorldView;
float4 g_lightNormal = { -0.3f, -1.0f, -0.5f, 0.0f };
float3 g_ambient = { 0.3f, 0.3f, 0.3f };
float g_ssaoDepthRange = 50.0f;

bool g_bUseTexture = true;
bool g_bUseLambert = true;
bool g_bShowNormalInfo = false;

texture texture1;
sampler textureSampler = sampler_state
{
    Texture = (texture1);
    MipFilter = LINEAR;
    MinFilter = LINEAR;
    MagFilter = LINEAR;
};

// ▼ 頂点シェーダー：clip空間の z/w を 0..1 にして渡す（非線形だが追加定数なしで簡単）
void VertexShader1(
    in float4 inPosition : POSITION,
    in float3 inNormal : NORMAL,
    in float2 inTexCoord0 : TEXCOORD0,
    out float4 outPosition : POSITION0,
    out float2 outTexCoord0 : TEXCOORD0,
    out float3 outViewPosition : TEXCOORD1,
    out float3 outNormal : TEXCOORD2,
    out float3 outViewNormal : TEXCOORD3)
{
    float4 clipPosition = mul(inPosition, g_matWorldViewProj);
    float4 viewPosition = mul(inPosition, g_matWorldView);
    outPosition = clipPosition;
    outTexCoord0 = inTexCoord0;
    outNormal = normalize(inNormal);
    outViewNormal = normalize(mul(float4(inNormal, 0.0f), g_matWorldView).xyz);
    outViewPosition = viewPosition.xyz;
}

// ▼ ピクセルシェーダー：MRTのCOLOR1にグレースケールで深度を書き込む
void PixelShaderMRT(
    in float2 inTexCoord0 : TEXCOORD0,
    in float3 inViewPosition : TEXCOORD1,
    in float3 inNormal : TEXCOORD2,
    in float3 inViewNormal : TEXCOORD3,
    out float4 outColor0 : COLOR0,
    out float4 outDepth : COLOR1,
    out float4 outColor2 : COLOR2)
{
    float4 baseColor = float4(0.5, 0.5, 0.5, 1.0);

    if (g_bUseTexture)
    {
        baseColor = tex2D(textureSampler, inTexCoord0);
    }

    float3 normal = normalize(inNormal);
    float3 lighting = 1.0.xxx;
    if (g_bUseLambert)
    {
        float3 lightDir = normalize(-g_lightNormal.xyz);
        float lambert = saturate(dot(normal, lightDir));
        lighting = saturate(g_ambient + lambert.xxx);
    }

    outColor0 = float4(baseColor.rgb * lighting, baseColor.a);

    // 近いほど黒、遠いほど白
    float viewDepth = max(0.0f, inViewPosition.z);
    float depth01 = saturate(viewDepth / g_ssaoDepthRange);
    float3 viewNormal = normalize(inViewNormal);
    outDepth = float4(depth01, 0.0f, 0.0f, 1.0f);
    outColor2 = float4(viewNormal * 0.5f + 0.5f, 1.0f);
}

void PixelShaderBackDepth(
    in float3 inViewPosition : TEXCOORD1,
    in float faceSign : VFACE,
    out float4 outDepth : COLOR0)
{
    if (faceSign > 0.0f)
    {
        clip(-1.0f);
    }

    float viewDepth = max(0.0f, inViewPosition.z);
    float depth01 = saturate(viewDepth / g_ssaoDepthRange);
    outDepth = float4(depth01, 0.0f, 0.0f, 1.0f);
}

// ==== 追加: MRT を使うテクニック ====
technique TechniqueMRT
{
    pass P0
    {
        CullMode = NONE;
        VertexShader = compile vs_3_0 VertexShader1();
        PixelShader = compile ps_3_0 PixelShaderMRT();
    }
}

technique TechniqueBackDepth
{
    pass P0
    {
        CullMode = NONE;
        VertexShader = compile vs_3_0 VertexShader1();
        PixelShader = compile ps_3_0 PixelShaderBackDepth();
    }
}
