// -----------------------------------------------------------------------------
// simple2.fx
//
// ポストプロセス側のシェーダー。
// 深度、法線、背面深度、カラーを読み、thickness、SSAO、ぼかし、最終合成を行う。
// -----------------------------------------------------------------------------

float4x4 g_matWorldViewProj;
float4 g_lightNormal = { -0.3f, -1.0f, -0.5f, 0.0f };
float3 g_ambient = { 0.3f, 0.3f, 0.3f };

bool g_bUseTexture = true;
bool g_bSingleChannelInput = false;
bool g_bEnableSimpleSsao = true;
bool g_bUseThicknessForSsao = true;
bool g_bDepthScaledSampleDistance = false;
bool g_bEnableThicknessCap = false;
bool g_bUseFixedSsaoSampleDistance = false;
bool g_bUseShadowSaturation = false;
int g_simpleSsaoSampleCount = 1;

// half-pixel 補正や texel サイズ計算に使う。
float2 g_screenSize = { 1600.0f, 900.0f };

// 深度からビュー空間位置を復元するための投影スケール。
float2 g_projectionScale = { 1.0f, 1.0f };

float g_simpleSsaoSampleDistanceMeters = 1.0f;
float g_thicknessScale = 1.0f;
float g_ssaoDepthRange = 50.0f;
float g_depthCompareThreshold = 0.0f;
float g_sampleDepthBiasThreshold = 1.0f;
float g_targetNormalBiasScale = 2.0f;
float g_targetDepthBiasScale = 1.0f;
float g_thicknessCap = 0.02f;
float g_shadowStrength = 1.0f;
float g_shadowSaturationStrength = 1.0f;

texture texture1;
sampler textureSampler = sampler_state {
    Texture = (texture1);
    MipFilter = NONE;
    MinFilter = NONE;
    MagFilter = NONE;
};

texture depthTexture;
sampler depthSampler = sampler_state {
    Texture = (depthTexture);
    MipFilter = NONE;
    MinFilter = NONE;
    MagFilter = NONE;
};

texture backDepthTexture;
sampler backDepthSampler = sampler_state {
    Texture = (backDepthTexture);
    MipFilter = NONE;
    MinFilter = NONE;
    MagFilter = NONE;
};

texture thicknessTexture;
sampler thicknessSampler = sampler_state {
    Texture = (thicknessTexture);
    MipFilter = NONE;
    MinFilter = NONE;
    MagFilter = NONE;
};

texture normalTexture;
sampler normalSampler = sampler_state {
    Texture = (normalTexture);
    MipFilter = NONE;
    MinFilter = NONE;
    MagFilter = NONE;
};

texture ssaoTexture;
sampler ssaoSampler = sampler_state {
    Texture = (ssaoTexture);
    MipFilter = NONE;
    MinFilter = NONE;
    MagFilter = NONE;
};

// 簡易ノイズ。
float Random01(float2 seed)
{
    return frac(sin(dot(seed, float2(12.9898f, 78.233f))) * 43758.5453f);
}

// 深度と UV からビュー空間位置を復元する。
float3 ReconstructViewPosition(float2 texCoord, float currentDepth)
{
    float viewDepthMeters = currentDepth * g_ssaoDepthRange;
    float2 ndc = float2(texCoord.x * 2.0f - 1.0f,
                        1.0f - texCoord.y * 2.0f);
    return float3(ndc.x * viewDepthMeters / g_projectionScale.x,
                  ndc.y * viewDepthMeters / g_projectionScale.y,
                  viewDepthMeters);
}

// ビュー空間位置をスクリーン UV へ戻す。
float2 ProjectViewPositionToTexCoord(float3 viewPosition)
{
    float viewDepthSafe = max(viewPosition.z, 0.0001f);
    float2 ndc = float2(viewPosition.x * g_projectionScale.x / viewDepthSafe,
                        viewPosition.y * g_projectionScale.y / viewDepthSafe);
    return float2(ndc.x * 0.5f + 0.5f,
                  0.5f - ndc.y * 0.5f);
}

// 深度差ベースのぼかし重み。
float ComputeDepthAwareBlurWeight(float centerDepth, float sampleDepth)
{
    if (centerDepth >= 0.999f || sampleDepth >= 0.999f)
    {
        return 0.0f;
    }

    float centerDepthMeters = max(centerDepth * g_ssaoDepthRange, 0.001f);
    float sampleDepthMeters = sampleDepth * g_ssaoDepthRange;
    float depthDifferenceMeters = abs(sampleDepthMeters - centerDepthMeters);

    float depthToleranceMeters = max(0.01f, centerDepthMeters * 0.08f);
    float normalizedDifference = depthDifferenceMeters / depthToleranceMeters;
    if (normalizedDifference >= 1.0f)
    {
        return 0.0f;
    }

    float closeness = 1.0f - normalizedDifference;
    return closeness * closeness;
}

// 0..1 に保存された法線を -1..1 に戻す。
float3 DecodeNormal(float4 encodedNormal)
{
    float3 normal = encodedNormal.xyz * 2.0f - 1.0f;
    return normalize(normal);
}

// 法線差ベースのぼかし重み。
float ComputeNormalAwareBlurWeight(float3 centerNormal, float3 sampleNormal)
{
    float normalAlignment = dot(centerNormal, sampleNormal);
    if (normalAlignment <= 0.55f)
    {
        return 0.0f;
    }

    float normalizedDifference = saturate((1.0f - normalAlignment) / 0.45f);
    float closeness = 1.0f - normalizedDifference;
    return closeness * closeness;
}

// 深度差と法線差をまとめたぼかし重み。
float ComputeBlurSampleWeight(float baseWeight, float centerDepth, float sampleDepth, float3 centerNormal, float3 sampleNormal)
{
    float depthWeight = ComputeDepthAwareBlurWeight(centerDepth, sampleDepth);
    float normalWeight = ComputeNormalAwareBlurWeight(centerNormal, sampleNormal);
    return baseWeight * depthWeight * normalWeight;
}

// 1 回分の遮蔽サンプル評価。
void ComputeOcclusionSample(float2 shiftedTexCoord,
                            float currentDepth,
                            float3 currentNormal,
                            float3 currentViewPosition,
                            float sampleDistanceMeters,
                            float distanceScale,
                            out float occluded,
                            out float valid,
                            out float3 hitColor)
{
    occluded = 0.0f;
    valid = 0.0f;
    hitColor = float3(0.0f, 0.0f, 0.0f);

    float sampleDistanceMetersScaled = sampleDistanceMeters * distanceScale;
    float3 sampleViewPosition = currentViewPosition + currentNormal * sampleDistanceMetersScaled;
    if (sampleViewPosition.z <= 0.0f)
    {
        return;
    }

    float2 sampleTexCoord = ProjectViewPositionToTexCoord(sampleViewPosition);
    if (sampleTexCoord.x < 0.0f || sampleTexCoord.x > 1.0f || sampleTexCoord.y < 0.0f || sampleTexCoord.y > 1.0f)
    {
        return;
    }
    float4 sampleTexCoordLod = float4(sampleTexCoord, 0.0f, 0.0f);
    float sampleDepth = tex2Dlod(depthSampler, sampleTexCoordLod).r;
    if (sampleDepth >= 0.999f)
    {
        return;
    }

    float expectedSampleDepth = saturate(sampleViewPosition.z / g_ssaoDepthRange);
    float targetNormalDepthBiasFactor = saturate(abs(currentNormal.z)) * g_targetNormalBiasScale;
    float work = (currentDepth - expectedSampleDepth);
    float sampleDepthBias = work * targetNormalDepthBiasFactor * g_targetDepthBiasScale;
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

    if (frontDepthWithMargin <= currentDepth && currentDepth <= backDepthWithMargin)
    {
        occluded = 1.0f;
        hitColor = tex2Dlod(textureSampler, sampleTexCoordLod).rgb;
    }

    valid = 1.0f;
}

// ポストプロセス用のフルスクリーンクアッド。
void VertexShader1(in  float4 inPosition  : POSITION,
                   in  float2 inTexCood   : TEXCOORD0,

                   out float4 outPosition : POSITION,
                   out float2 outTexCood  : TEXCOORD0)
{
    outPosition = inPosition;
    outTexCood = inTexCood;
}

// 現在ピクセルの SSAO 係数を計算する。
void ComputeSsaoData(float2 shiftedTexCoord,
                     out float ssaoFactor,
                     out float3 indirectColor)
{
    float currentDepth = tex2D(depthSampler, shiftedTexCoord).r;
    float3 currentNormal = tex2D(normalSampler, shiftedTexCoord).xyz * 2.0f - 1.0f;
    currentNormal = normalize(currentNormal);
    indirectColor = float3(0.0f, 0.0f, 0.0f);
    ssaoFactor = 1.0f;

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
                float occluded = 0.0f;
                float valid = 0.0f;
                float3 hitColor = float3(0.0f, 0.0f, 0.0f);
                ComputeOcclusionSample(shiftedTexCoord,
                                       currentDepth,
                                       currentNormal,
                                       currentViewPosition,
                                       sampleDistanceMeters,
                                       1.0f,
                                       occluded,
                                       valid,
                                       hitColor);
                if (valid > 0.5f && occluded > 0.5f)
                {
                    ssaoFactor = 0.0f;
                    indirectColor = hitColor;
                }
            }
            else
            {
                float occlusionCount = 0.0f;
                float validSampleCount = 0.0f;
                float3 occlusionColorSum = float3(0.0f, 0.0f, 0.0f);
                [loop]
                for (int sampleIndex = 0; sampleIndex < 128; ++sampleIndex)
                {
                    if (sampleIndex < g_simpleSsaoSampleCount)
                    {
                        float sampleIndexFloat = (float)sampleIndex;
                        float randomValue = 0.0f;
                        float distanceScale = 0.f;;

                        if (g_bUseFixedSsaoSampleDistance)
                        {
                            distanceScale = 1.0f;
                            if (g_simpleSsaoSampleCount > 1)
                            {
                                float fixedDistanceU = sampleIndexFloat / (float)(g_simpleSsaoSampleCount - 1);
                                distanceScale = fixedDistanceU * fixedDistanceU * fixedDistanceU;
                            }
                        }
                        else
                        {
                            randomValue = Random01(shiftedTexCoord * g_screenSize + float2(sampleIndexFloat * 13.37f,
                                                                                                 sampleIndexFloat * 7.91f));
                            distanceScale = randomValue * randomValue * randomValue;
                        }

                        distanceScale += (0.1 * currentDepth);

                        float occluded = 0.0f;
                        float valid = 0.0f;
                        float3 hitColor = float3(0.0f, 0.0f, 0.0f);
                        ComputeOcclusionSample(shiftedTexCoord,
                                               currentDepth,
                                               currentNormal,
                                               currentViewPosition,
                                               sampleDistanceMeters,
                                               distanceScale,
                                               occluded,
                                               valid,
                                               hitColor);
                        occlusionCount += occluded;
                        validSampleCount += valid;
                        occlusionColorSum += hitColor * occluded;
                    }
                }

                if (validSampleCount > 0.0f)
                {
                    float occlusionRate = occlusionCount / validSampleCount;
                    ssaoFactor = 1.0f - occlusionRate;
                }
                if (occlusionCount > 0.0f)
                {
                    indirectColor = occlusionColorSum / occlusionCount;
                }
            }
        }
    }

    ssaoFactor = saturate(ssaoFactor);
}

void PixelShaderSsao(in float4 inPosition    : POSITION,
                     in float2 inTexCood     : TEXCOORD0,

                     out float4 outColor     : COLOR)
{
    float2 halfPixelOffset = 0.5f / g_screenSize;
    float2 shiftedTexCoord = inTexCood + halfPixelOffset;
    float ssaoFactor = 1.0f;
    float3 indirectColor = float3(0.0f, 0.0f, 0.0f);
    ComputeSsaoData(shiftedTexCoord, ssaoFactor, indirectColor);
    outColor = float4(indirectColor, ssaoFactor);
}

// 元カラーと SSAO を合成する。
void PixelShaderComposite(in float4 inPosition    : POSITION,
                          in float2 inTexCood     : TEXCOORD0,

                          out float4 outColor     : COLOR)
{
    float2 halfPixelOffset = 0.5f / g_screenSize;
    float2 shiftedTexCoord = inTexCood + halfPixelOffset;
    float4 workColor = tex2D(textureSampler, shiftedTexCoord);
    float4 ssaoData = tex2D(ssaoSampler, shiftedTexCoord);
    float ssaoFactor = ssaoData.a;
    float3 indirectColor = ssaoData.rgb;
    float shadowAmount = saturate(g_shadowStrength * (1.0f - ssaoFactor));
    workColor.rgb = workColor.rgb * ssaoFactor + indirectColor * shadowAmount;
    if (g_bUseShadowSaturation)
    {
        float saturationAmount = saturate(g_shadowSaturationStrength * (1.0f - ssaoFactor));
        float luminance = dot(workColor.rgb, float3(0.299f, 0.587f, 0.114f));
        float3 grayscale = float3(luminance, luminance, luminance);
        float saturationScale = 1.0f + saturationAmount;
        workColor.rgb = grayscale + (workColor.rgb - grayscale) * saturationScale;
    }
    workColor = saturate(workColor);
    outColor = workColor;
}

// 5x5 ぼかし。
void PixelShaderSsaoBlur5x5(in float4 inPosition    : POSITION,
                            in float2 inTexCood     : TEXCOORD0,

                            out float4 outColor     : COLOR)
{
    float2 halfPixelOffset = 0.5f / g_screenSize;
    float2 shiftedTexCoord = inTexCood + halfPixelOffset;
    float2 texelSize = 1.0f / g_screenSize;
    float centerDepth = tex2D(depthSampler, shiftedTexCoord).r;
    float3 centerNormal = DecodeNormal(tex2D(normalSampler, shiftedTexCoord));
    float4 blurredValue = float4(0.0f, 0.0f, 0.0f, 0.0f);
    float weightSum = 0.0f;

    [unroll]
    for (int y = -2; y <= 2; ++y)
    {
        [unroll]
        for (int x = -2; x <= 2; ++x)
        {
            float2 sampleTexCoord = shiftedTexCoord + float2((float)x * texelSize.x, (float)y * texelSize.y);
            sampleTexCoord = saturate(sampleTexCoord);
            float sampleDepth = tex2D(depthSampler, sampleTexCoord).r;
            float3 sampleNormal = DecodeNormal(tex2D(normalSampler, sampleTexCoord));
            float weight = ComputeBlurSampleWeight(1.0f, centerDepth, sampleDepth, centerNormal, sampleNormal);
            blurredValue += tex2D(ssaoSampler, sampleTexCoord) * weight;
            weightSum += weight;
        }
    }

    float4 ssaoData = float4(0.0f, 0.0f, 0.0f, 1.0f);
    if (weightSum > 0.0f)
    {
        ssaoData = blurredValue / weightSum;
    }
    outColor = ssaoData;
}

// 11x11 ぼかし。
void PixelShaderSsaoBlur11x11(in float4 inPosition    : POSITION,
                              in float2 inTexCood     : TEXCOORD0,

                              out float4 outColor     : COLOR)
{
    float2 halfPixelOffset = 0.5f / g_screenSize;
    float2 shiftedTexCoord = inTexCood + halfPixelOffset;
    float2 texelSize = 1.0f / g_screenSize;
    float centerDepth = tex2D(depthSampler, shiftedTexCoord).r;
    float3 centerNormal = DecodeNormal(tex2D(normalSampler, shiftedTexCoord));
    float4 blurredValue = float4(0.0f, 0.0f, 0.0f, 0.0f);
    float weightSum = 0.0f;

    [loop]
    for (int y = -5; y <= 5; ++y)
    {
        [loop]
        for (int x = -5; x <= 5; ++x)
        {
            float2 sampleTexCoord = shiftedTexCoord + float2((float)x * texelSize.x, (float)y * texelSize.y);
            sampleTexCoord = saturate(sampleTexCoord);
            float sampleDepth = tex2D(depthSampler, sampleTexCoord).r;
            float3 sampleNormal = DecodeNormal(tex2D(normalSampler, sampleTexCoord));
            float weight = ComputeBlurSampleWeight(1.0f, centerDepth, sampleDepth, centerNormal, sampleNormal);
            blurredValue += tex2D(ssaoSampler, sampleTexCoord) * weight;
            weightSum += weight;
        }
    }

    float4 ssaoData = float4(0.0f, 0.0f, 0.0f, 1.0f);
    if (weightSum > 0.0f)
    {
        ssaoData = blurredValue / weightSum;
    }
    outColor = ssaoData;
}

// 21x21 ぼかし。
void PixelShaderSsaoBlur21x21(in float4 inPosition    : POSITION,
                              in float2 inTexCood     : TEXCOORD0,

                              out float4 outColor     : COLOR)
{
    float2 halfPixelOffset = 0.5f / g_screenSize;
    float2 shiftedTexCoord = inTexCood + halfPixelOffset;
    float2 texelSize = 1.0f / g_screenSize;
    float centerDepth = tex2D(depthSampler, shiftedTexCoord).r;
    float3 centerNormal = DecodeNormal(tex2D(normalSampler, shiftedTexCoord));
    float4 blurredValue = float4(0.0f, 0.0f, 0.0f, 0.0f);
    float weightSum = 0.0f;

    [loop]
    for (int y = -10; y <= 10; ++y)
    {
        [loop]
        for (int x = -10; x <= 10; ++x)
        {
            float2 sampleTexCoord = shiftedTexCoord + float2((float)x * texelSize.x, (float)y * texelSize.y);
            sampleTexCoord = saturate(sampleTexCoord);
            float sampleDepth = tex2D(depthSampler, sampleTexCoord).r;
            float3 sampleNormal = DecodeNormal(tex2D(normalSampler, sampleTexCoord));
            float weight = ComputeBlurSampleWeight(1.0f, centerDepth, sampleDepth, centerNormal, sampleNormal);
            blurredValue += tex2D(ssaoSampler, sampleTexCoord) * weight;
            weightSum += weight;
        }
    }

    float4 ssaoData = float4(0.0f, 0.0f, 0.0f, 1.0f);
    if (weightSum > 0.0f)
    {
        ssaoData = blurredValue / weightSum;
    }
    outColor = ssaoData;
}

// デバッグ表示用。
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

// 前面深度と背面深度の差から thickness を作る。
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

technique TechniqueSsao
{
    pass Pass1
    {
        CullMode = NONE;

        VertexShader = compile vs_3_0 VertexShader1();
        PixelShader = compile ps_3_0 PixelShaderSsao();
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

technique TechniqueSsaoBlur
{
    pass Pass1
    {
        CullMode = NONE;

        VertexShader = compile vs_3_0 VertexShader1();
        PixelShader = compile ps_3_0 PixelShaderSsaoBlur5x5();
   }
}

technique TechniqueSsaoBlurLarge
{
    pass Pass1
    {
        CullMode = NONE;

        VertexShader = compile vs_3_0 VertexShader1();
        PixelShader = compile ps_3_0 PixelShaderSsaoBlur11x11();
   }
}

technique TechniqueSsaoBlurXLarge
{
    pass Pass1
    {
        CullMode = NONE;

        VertexShader = compile vs_3_0 VertexShader1();
        PixelShader = compile ps_3_0 PixelShaderSsaoBlur21x21();
   }
}

technique TechniqueComposite
{
    pass Pass1
    {
        CullMode = NONE;

        VertexShader = compile vs_3_0 VertexShader1();
        PixelShader = compile ps_3_0 PixelShaderComposite();
   }
}
