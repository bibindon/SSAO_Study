// 1�p�X�ڂō�����[�x�e�N�X�`���iR=���`�[�x�j
texture texture1;
sampler depthSamp = sampler_state
{
    Texture = (texture1);
    MinFilter = POINT;
    MagFilter = POINT;
    MipFilter = NONE;
    AddressU = CLAMP;
    AddressV = CLAMP;
};

float4 g_TexelSize = float4(1.0 / 640.0, 1.0 / 480.0, 0, 0); // main.cpp����Z�b�g
float g_RadiusPixels = 4.0; // �ߖT�T�����a�i��f�j
float g_Bias = 0.001; // �o�C�A�X�i�A�N�l�΍�j
float g_Intensity = 1.2; // �Z��

struct VSIn
{
    float4 pos : POSITION;
    float2 uv : TEXCOORD0;
};
struct VSOut
{
    float4 pos : POSITION;
    float2 uv : TEXCOORD0;
};

VSOut VS(VSIn i)
{
    VSOut o;
    o.pos = i.pos;
    o.uv = i.uv;
    return o;
}

// �X�N���[����Ԃ̊ȈՋߖT12�����i�~�{�\���{�΂߁{���ځj
static const float2 OFFS[12] =
{
    float2(1, 0), float2(-1, 0), float2(0, 1), float2(0, -1),
    float2(1, 1), float2(-1, 1), float2(1, -1), float2(-1, -1),
    float2(2, 0), float2(-2, 0), float2(0, 2), float2(0, -2)
};

float4 PS(VSOut i) : COLOR
{
    float center = tex2D(depthSamp, i.uv).r; // ���`�[�x

    float occ = 0.0;
    [unroll]
    for (int k = 0; k < 12; k++)
    {
        float2 duv = OFFS[k] * g_TexelSize.xy * g_RadiusPixels;
        float sd = tex2D(depthSamp, i.uv + duv).r;

        // �g��O�ɂ���T���v���͎Օ��h�Ƃ݂Ȃ����ȈՃ��[��
        // ���`�[�x�͍������ɏ������̂ŃX�P�[�������߂�
        float diff = center - sd - g_Bias;
        occ += saturate(diff * 500.0); // ���₷���d���̃X�P�[��
    }

    occ = saturate((occ / 12.0) * g_Intensity);
    float ao = 1.0 - occ; // 1=��(��Օ�), 0=��(�Օ�)
    return float4(ao, ao, ao, 1.0);
}

technique Technique1
{
    pass P0
    {
        CullMode = NONE;
        ZEnable = FALSE;

        VertexShader = compile vs_3_0 VS();
        PixelShader = compile ps_3_0 PS();
    }
}
