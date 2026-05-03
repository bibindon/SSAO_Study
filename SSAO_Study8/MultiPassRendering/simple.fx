float4x4 g_matWorld;
float4x4 g_matView;
float4x4 g_matWorldViewProj;

float g_fNear;
float g_fFar;

float g_posRange;

bool g_bUseTexture = false;
bool g_bUseLambert = true;

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

    float3 nWS = mul(inNormal.xyz, (float3x3)g_matWorld);
    nWS = normalize(nWS);

    float lightIntensity = 1.0f;
    if (g_bUseLambert)
    {
        float3 lightDir = normalize(float3(-0.3f, 1.0f, -0.5f));
        lightIntensity = max(0.0f, dot(nWS, lightDir)) + 0.3f;
    }

    outDiffuse.rgb = lightIntensity.xxx;
    outDiffuse.a   = 1.0f;

    outTexCood = inTexCood;

    float4 vpos = mul(worldPos, g_matView);
    outViewZ = vpos.z;

    outWorldPos = worldPos.xyz;
    outNormalWS = nWS;
}

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
    float3 lit  = inScreenColor.rgb;
    float3 base = lit;

    if (g_bUseTexture)
    {
        float3 tex = tex2D(sampBase, inTexCood).rgb;
        base = tex * lit;
    }
    outColor = float4(base, 1.0f);

    float linearZ = saturate((inViewZ - g_fNear) / (g_fFar - g_fNear));
    outZ = float4(linearZ, linearZ, linearZ, linearZ);

    float3 normalizedPosWS = inWorldPos / g_posRange;
    float3 encPos = saturate(normalizedPosWS * 0.5f + 0.5f);
    outPosWS = float4(encPos, 1.0f);

    float3 encN = saturate(inNormalWS * 0.5f + 0.5f);
    outNormalWS = float4(encN, 1.0f);
}

technique TechniqueMRT
{
    pass P0
    {
        CullMode    = NONE;
        VertexShader = compile vs_3_0 VertexShader1();
        PixelShader  = compile ps_3_0 PixelShaderMRT4();
    }
}
