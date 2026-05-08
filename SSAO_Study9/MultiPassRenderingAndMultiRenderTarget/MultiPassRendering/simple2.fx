float4x4 g_matWorldViewProj;
float4 g_lightNormal = { -0.3f, -1.0f, -0.5f, 0.0f };
float3 g_ambient = { 0.3f, 0.3f, 0.3f };

bool g_bUseTexture = true;
bool g_bSingleChannelInput = false;
bool g_bEnableSimpleSsao = true;
bool g_bUseThicknessForSsao = true;
float2 g_screenSize = { 1600.0f, 900.0f };
float g_simpleSsaoSamplePixels = 20.0f;
float g_thicknessScale = 1.0f;
float g_ssaoDepthRange = 50.0f;
float g_depthCompareThreshold = 0.0f;
float g_targetNormalBiasScale = 1.0f;
float g_targetDepthBiasScale = 1.0f;

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

texture backDepthTexture;
sampler backDepthSampler = sampler_state {
    Texture = (backDepthTexture);
    MipFilter = NONE;
    MinFilter = POINT;
    MagFilter = POINT;
};

texture thicknessTexture;
sampler thicknessSampler = sampler_state {
    Texture = (thicknessTexture);
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
        if (currentDepth < 0.999f)
        {
            float2 sampleDirectionNormalized = (directionLength > 0.0001f) ? (sampleDirection / directionLength) : float2(0.0f, 0.0f);
            float samplePixelDistance = g_simpleSsaoSamplePixels * saturate(directionLength);
            float2 sampleOffset = float2(sampleDirectionNormalized.x * (samplePixelDistance / g_screenSize.x),
                                         -sampleDirectionNormalized.y * (samplePixelDistance / g_screenSize.y));
            float2 sampleTexCoord = saturate(shiftedTexCoord + sampleOffset);
            float sampleDepth = tex2D(depthSampler, sampleTexCoord).r;
            if (sampleDepth < 0.999f)
            {
                float targetNormalDepthBiasFactor = saturate(length(currentNormal.xy)) * g_targetNormalBiasScale;
                float sampleDepthBias = g_depthCompareThreshold * targetNormalDepthBiasFactor * (currentDepth * g_targetDepthBiasScale);
                float adjustedSampleDepth = max(0.0f, sampleDepth - sampleDepthBias);
                float sampleThickness = tex2D(thicknessSampler, sampleTexCoord).r;
                float frontDepthWithMargin = adjustedSampleDepth - g_depthCompareThreshold;
                float backDepthWithMargin = adjustedSampleDepth + g_depthCompareThreshold;
                if (g_bUseThicknessForSsao)
                {
                    backDepthWithMargin += sampleThickness * g_thicknessScale;
                }

                if (frontDepthWithMargin <= currentDepth && currentDepth <= backDepthWithMargin)
                {
                    workColor = float4(0.0f, 0.0f, 0.0f, workColor.a);
                }
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

void PixelShaderThickness(in float4 inPosition : POSITION,
                          in float2 inTexCood  : TEXCOORD0,

                          out float4 outColor  : COLOR)
{
    float2 halfPixelOffset = 0.5f / g_screenSize;
    float2 shiftedTexCoord = inTexCood + halfPixelOffset;
    float frontDepth = tex2D(depthSampler, shiftedTexCoord).r;
    float backDepth = tex2D(backDepthSampler, shiftedTexCoord).r;

    float thickness = 0.0f;
    if (backDepth < 0.999f)
    {
        thickness = saturate(backDepth - frontDepth);
    }

    outColor = float4(thickness, 0.0f, 0.0f, 1.0f);
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

technique TechniqueThickness
{
    pass Pass1
    {
        CullMode = NONE;

        VertexShader = compile vs_3_0 VertexShader1();
        PixelShader = compile ps_3_0 PixelShaderThickness();
   }
}
