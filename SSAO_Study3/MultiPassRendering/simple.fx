float4x4 g_matWorldViewProj;
float4 g_lightNormal = { 0.3f, 1.0f, 0.5f, 0.0f };
float3 g_ambient = { 0.3f, 0.3f, 0.3f };

bool g_bUseTexture = true;

texture texture1;
sampler textureSampler = sampler_state
{
    Texture = (texture1);
    MipFilter = LINEAR;
    MinFilter = LINEAR;
    MagFilter = LINEAR;
};

void VertexShader1(in float4 inPosition : POSITION,
                   in float4 inNormal : NORMAL0,
                   in float4 inTexCood : TEXCOORD0,

                   out float4 outPosition : POSITION,
                   out float4 outDiffuse : COLOR0,
                   out float4 outTexCood : TEXCOORD0)
{
    outPosition = mul(inPosition, g_matWorldViewProj);

    float lightIntensity = dot(inNormal, g_lightNormal);
    outDiffuse.rgb = max(0, lightIntensity) + g_ambient;
    outDiffuse.a = 1.0f;

    outTexCood = inTexCood;
}

void PixelShader1(in float4 inScreenColor : COLOR0,
                  in float2 inTexCood : TEXCOORD0,
                  out float4 outColor : COLOR)
{
    float4 workColor = tex2D(textureSampler, inTexCood);

    if (g_bUseTexture)
    {
        outColor = inScreenColor * workColor;
    }
    else
    {
        outColor = inScreenColor;
    }
}

// ==== 追加: MRT 用ピクセルシェーダ ====
void PixelShaderMRT(in float4 inScreenColor : COLOR0,
                    in float2 inTexCood : TEXCOORD0,
                    out float4 outColor0 : COLOR0,
                    out float4 outColor1 : COLOR1)
{
    float4 sampled = tex2D(textureSampler, inTexCood);

    // ベース色（ライティング * テクスチャ or テクスチャなし）
    float4 baseColor = g_bUseTexture ? (inScreenColor * sampled) : inScreenColor;

    // RT0 には通常の結果、RT1 にはテクスチャ（またはベース色）を出力
    outColor0 = baseColor;
    outColor1 = g_bUseTexture ? sampled : baseColor;
}

technique Technique1
{
    pass Pass1
    {
        CullMode = NONE;
        VertexShader = compile vs_3_0 VertexShader1();
        PixelShader = compile ps_3_0 PixelShader1();
    }
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
