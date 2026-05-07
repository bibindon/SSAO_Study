float4x4 g_matWorldViewProj;
float4 g_lightNormal = { -0.3f, -1.0f, -0.5f, 0.0f };
float3 g_ambient = { 0.3f, 0.3f, 0.3f };

bool g_bUseTexture = true;
bool g_bSingleChannelInput = false;
bool g_bEnableSimpleSsao = true;
float2 g_screenSize = { 1600.0f, 900.0f };
float g_simpleSsaoSamplePixels = 20.0f;
float g_depthCompareThreshold = 0.002f;

texture texture1;
sampler textureSampler = sampler_state {
    Texture = (texture1);
    MipFilter = NONE;
    MinFilter = POINT;
    MagFilter = POINT;
};

texture depthTexture;
sampler depthSampler = sampler_state {
    Texture = (depthTexture);
    MipFilter = NONE;
    MinFilter = POINT;
    MagFilter = POINT;
};

texture normalTexture;
sampler normalSampler = sampler_state {
    Texture = (normalTexture);
    MipFilter = NONE;
    MinFilter = POINT;
    MagFilter = POINT;
};

void VertexShader1(in  float4 inPosition  : POSITION,
                   in  float2 inTexCood   : TEXCOORD0,

                   out float4 outPosition : POSITION,
                   out float2 outTexCood  : TEXCOORD0)
{
    outPosition = inPosition;
    outTexCood = inTexCood;
}

void PixelShader1(in float4 inPosition    : POSITION,
                  in float2 inTexCood     : TEXCOORD0,

                  out float4 outColor     : COLOR)
{
    float4 workColor = (float4)0;
    float2 halfPixelOffset = 0.5f / g_screenSize;
    float2 shiftedTexCoord = inTexCood + halfPixelOffset;
    workColor = tex2D(textureSampler, shiftedTexCoord);

    float currentDepth = tex2D(depthSampler, shiftedTexCoord).r;
    float3 currentNormal = tex2D(normalSampler, shiftedTexCoord).xyz * 2.0f - 1.0f;
    currentNormal = normalize(currentNormal);

    if (g_bEnableSimpleSsao)
    {
        float2 sampleDirection = currentNormal.xy;
        float directionLength = length(sampleDirection);
        if (directionLength > 0.1f)
        {
            sampleDirection /= directionLength;
            float2 sampleOffset = float2(sampleDirection.x * (g_simpleSsaoSamplePixels / g_screenSize.x),
                                         -sampleDirection.y * (g_simpleSsaoSamplePixels / g_screenSize.y));
            float2 sampleTexCoord = saturate(shiftedTexCoord + sampleOffset);
            float sampleDepth = tex2D(depthSampler, sampleTexCoord).r;

            if (abs(currentDepth - sampleDepth) < g_depthCompareThreshold)
            {
                workColor = float4(0.0f, 0.0f, 0.0f, workColor.a);
            }
        }
    }

    workColor = saturate(workColor);

    if (false)
    {
        float2 pixelCoord = shiftedTexCoord * g_screenSize;
        float lineX = 1.0f - step(1.0f, fmod(pixelCoord.x, 5.0f));
        float lineY = 1.0f - step(1.0f, fmod(pixelCoord.y, 5.0f));
        float lineMask = saturate(lineX + lineY);
        float4 lineColor = float4(0.0f, 1.0f, 0.0f, 1.0f);
        workColor = lerp(workColor, lineColor, lineMask);
    }

    outColor = workColor;
    
}

void PixelShaderDebug(in float4 inPosition    : POSITION,
                      in float2 inTexCood     : TEXCOORD0,

                      out float4 outColor     : COLOR)
{
    float4 sampleColor = tex2D(textureSampler, inTexCood);
    if (g_bSingleChannelInput)
    {
        outColor = float4(sampleColor.r, sampleColor.r, sampleColor.r, 1.0f);
    }
    else
    {
        outColor = sampleColor;
    }
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

technique TechniqueDebug
{
    pass Pass1
    {
        CullMode = NONE;

        VertexShader = compile vs_3_0 VertexShader1();
        PixelShader = compile ps_3_0 PixelShaderDebug();
   }
}
