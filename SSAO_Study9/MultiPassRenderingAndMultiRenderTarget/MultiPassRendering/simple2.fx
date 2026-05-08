float4x4 g_matWorldViewProj;
float4 g_lightNormal = { -0.3f, -1.0f, -0.5f, 0.0f };
float3 g_ambient = { 0.3f, 0.3f, 0.3f };

bool g_bUseTexture = true;
bool g_bSingleChannelInput = false;
bool g_bEnableSimpleSsao = true;
bool g_bUseThicknessForSsao = true;
bool g_bDepthScaledSampleDistance = false;
bool g_bEnableThicknessCap = false;
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
float g_thicknessCap = 0.02f;

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

float3 ReconstructViewPosition(float2 texCoord, float currentDepth)
{
    float viewDepthMeters = currentDepth * g_ssaoDepthRange;
    float2 ndc = float2(texCoord.x * 2.0f - 1.0f,
                        1.0f - texCoord.y * 2.0f);
    return float3(ndc.x * viewDepthMeters / g_projectionScale.x,
                  ndc.y * viewDepthMeters / g_projectionScale.y,
                  viewDepthMeters);
}

float2 ProjectViewPositionToTexCoord(float3 viewPosition)
{
    float viewDepthSafe = max(viewPosition.z, 0.0001f);
    float2 ndc = float2(viewPosition.x * g_projectionScale.x / viewDepthSafe,
                        viewPosition.y * g_projectionScale.y / viewDepthSafe);
    return float2(ndc.x * 0.5f + 0.5f,
                  0.5f - ndc.y * 0.5f);
}

float2 ComputeOcclusionSample(float2 shiftedTexCoord,
                              float currentDepth,
                              float3 currentNormal,
                              float3 currentViewPosition,
                              float sampleDistanceMeters,
                              float distanceScale)
{
    float sampleDistanceMetersScaled = sampleDistanceMeters * distanceScale;
    float3 sampleViewPosition = currentViewPosition + currentNormal * sampleDistanceMetersScaled;
    if (sampleViewPosition.z <= 0.0f)
    {
        return float2(0.0f, 0.0f);
    }

    float2 sampleTexCoord = ProjectViewPositionToTexCoord(sampleViewPosition);
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

    float expectedSampleDepth = saturate(sampleViewPosition.z / g_ssaoDepthRange);
    float targetNormalDepthBiasFactor = saturate(abs(currentNormal.z)) * g_targetNormalBiasScale;
    float sampleDepthBias = (currentDepth - expectedSampleDepth) * targetNormalDepthBiasFactor * (currentDepth * g_targetDepthBiasScale);
    float adjustedSampleDepth = max(0.0f, sampleDepth + sampleDepthBias);
    float sampleThickness = tex2Dlod(thicknessSampler, sampleTexCoordLod).r;
    float frontDepthWithMargin = adjustedSampleDepth - g_depthCompareThreshold;
    float backDepthWithMargin = adjustedSampleDepth + g_depthCompareThreshold;
    if (g_bUseThicknessForSsao)
    {
        backDepthWithMargin += sampleThickness * g_thicknessScale;
    }
    else
    {
        float fallbackThickness = (1.0f / g_ssaoDepthRange) * currentDepth;
        backDepthWithMargin += fallbackThickness * g_thicknessScale;
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
        if (currentDepth < 0.999f)
        {
            float3 currentViewPosition = ReconstructViewPosition(shiftedTexCoord, currentDepth);
            float sampleDistanceMeters = g_simpleSsaoSampleDistanceMeters;
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
                                                                currentViewPosition,
                                                                sampleDistanceMeters,
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
                        float distanceScale = randomValue * randomValue * randomValue;

                        // 0にならないようにする。
                        distanceScale += (0.1 * currentDepth);
                        float2 occlusionSample = ComputeOcclusionSample(shiftedTexCoord,
                                                                        currentDepth,
                                                                        currentNormal,
                                                                        currentViewPosition,
                                                                        sampleDistanceMeters,
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
        if (g_bEnableThicknessCap && thickness > g_thicknessCap)
        {
            thickness = 0.0f;
        }
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
