float4x4 g_matWorldViewProj;
float4 g_lightNormal = { -0.3f, -1.0f, -0.5f, 0.0f };
float3 g_ambient = { 0.3f, 0.3f, 0.3f };

bool g_bUseTexture = true;
bool g_bSingleChannelInput = false;
bool g_bEnableSimpleSsao = true;
bool g_bUseThicknessForSsao = true;
bool g_bDepthScaledSampleDistance = false;
int g_simpleSsaoSampleCount = 1;
float2 g_screenSize = { 1600.0f, 900.0f };
float2 g_projectionScale = { 1.0f, 1.0f };
float g_simpleSsaoSampleDistanceMeters = 1.0f;
float g_thicknessScale = 1.0f;
float g_ssaoDepthRange = 50.0f;
float g_depthCompareThreshold = 0.0f;
float g_sampleDepthBiasThreshold = 1.0f;
float g_targetNormalBiasScale = 2.0f;
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

float Random01(float2 seed)
{
    return frac(sin(dot(seed, float2(12.9898f, 78.233f))) * 43758.5453f);
}

float2 ComputeOcclusionSample(float2 shiftedTexCoord,
                              float currentDepth,
                              float3 currentNormal,
                              float sampleDistanceMeters,
                              float viewDepthMeters,
                              float2 sampleDirectionNormalized,
                              float distanceScale)
{
    float sampleDistanceMetersScaled = sampleDistanceMeters * distanceScale;
    float viewDepthSafe = max(viewDepthMeters, 0.0001f);
    float2 sampleOffset = float2(sampleDirectionNormalized.x * (sampleDistanceMetersScaled * g_projectionScale.x * 0.5f / viewDepthSafe),
                                 -sampleDirectionNormalized.y * (sampleDistanceMetersScaled * g_projectionScale.y * 0.5f / viewDepthSafe));
    float2 sampleTexCoord = shiftedTexCoord + sampleOffset;
    if (sampleTexCoord.x < 0.0f || sampleTexCoord.x > 1.0f || sampleTexCoord.y < 0.0f || sampleTexCoord.y > 1.0f)
    {
        return float2(0.0f, 0.0f);
    }
    float4 sampleTexCoordLod = float4(sampleTexCoord, 0.0f, 0.0f);
    float sampleDepth = tex2Dlod(depthSampler, sampleTexCoordLod).r;
    if (sampleDepth >= 0.999f)
    {
        return float2(0.0f, 0.0f);
    }

    float targetNormalDepthBiasFactor = saturate(length(currentNormal.xy)) * g_targetNormalBiasScale;
    float sampleDepthBias = g_sampleDepthBiasThreshold * targetNormalDepthBiasFactor * (currentDepth * g_targetDepthBiasScale);
    float adjustedSampleDepth = max(0.0f, sampleDepth + sampleDepthBias);
    float sampleThickness = tex2Dlod(thicknessSampler, sampleTexCoordLod).r;
    float frontDepthWithMargin = adjustedSampleDepth - g_depthCompareThreshold;
    float backDepthWithMargin = adjustedSampleDepth + g_depthCompareThreshold;
    if (g_bUseThicknessForSsao)
    {
        backDepthWithMargin += sampleThickness * g_thicknessScale;
    }

    float occluded = (frontDepthWithMargin <= currentDepth && currentDepth <= backDepthWithMargin) ? 1.0f : 0.0f;
    return float2(occluded, 1.0f);
}

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
            float viewDepthMeters = max(currentDepth * g_ssaoDepthRange, 0.0001f);
            float sampleDistanceMeters = g_simpleSsaoSampleDistanceMeters * saturate(directionLength);
            if (g_bDepthScaledSampleDistance)
            {
                float depthDistanceScale = 1.0f + saturate(1.0f - currentDepth);
                sampleDistanceMeters *= depthDistanceScale;
            }
            if (g_simpleSsaoSampleCount <= 1)
            {
                float2 occlusionSample = ComputeOcclusionSample(shiftedTexCoord,
                                                                currentDepth,
                                                                currentNormal,
                                                                sampleDistanceMeters,
                                                                viewDepthMeters,
                                                                sampleDirectionNormalized,
                                                                1.0f);
                if (occlusionSample.x > 0.5f)
                {
                    workColor = float4(0.0f, 0.0f, 0.0f, workColor.a);
                }
            }
            else
            {
                float occlusionCount = 0.0f;
                float validSampleCount = 0.0f;
                [loop]
                for (int sampleIndex = 0; sampleIndex < 128; ++sampleIndex)
                {
                    if (sampleIndex < g_simpleSsaoSampleCount)
                    {
                        float sampleIndexFloat = (float)sampleIndex;
                        float randomValue = Random01(shiftedTexCoord * g_screenSize + float2(sampleIndexFloat * 13.37f,
                                                                                             sampleIndexFloat * 7.91f));
                        float distanceScale = randomValue * randomValue;
                        float2 occlusionSample = ComputeOcclusionSample(shiftedTexCoord,
                                                                        currentDepth,
                                                                        currentNormal,
                                                                        sampleDistanceMeters,
                                                                        viewDepthMeters,
                                                                        sampleDirectionNormalized,
                                                                        distanceScale);
                        occlusionCount += occlusionSample.x;
                        validSampleCount += occlusionSample.y;
                    }
                }

                if (validSampleCount > 0.0f)
                {
                    float occlusionRate = occlusionCount / validSampleCount;
                    workColor.rgb *= (1.0f - occlusionRate);
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
