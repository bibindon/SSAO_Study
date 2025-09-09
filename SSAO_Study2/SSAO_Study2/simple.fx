float4x4 g_matWorldViewProj;
float4x4 g_matView;
float g_Far = 10000.0f; // ���N���b�v�Bmain.cpp���ŃZ�b�g

struct VSIn
{
    float4 pos : POSITION;
    float3 nrm : NORMAL0;
    float2 uv : TEXCOORD0;
};

struct VSOut
{
    float4 pos : POSITION;
    float depth : TEXCOORD0;
};

VSOut VS(VSIn i)
{
    VSOut o;
    o.pos = mul(i.pos, g_matWorldViewProj);

    // �r���[���Z�����o���A[0..1]�̐��`�[�x�ɐ��K���iLH�n�Ȃ̂�+Z�j
    float3 viewPos = mul(i.pos, g_matView).xyz;
    float linZ = viewPos.z / g_Far; // 0(��)�`1(��)  ��near/far��ɂ��ߒ[�͌����ɂ�0�ɂȂ�܂���
    o.depth = saturate(linZ);
    return o;
}

float4 PS(VSOut i) : COLOR
{
    float d = i.depth;
    // 16bit���������t�H�[�}�b�g�ł����₷���悤��RGB�֕���
    return float4(d, d, d, 1.0);
}

technique Technique1
{
    pass P0
    {
        CullMode = NONE;
        ZEnable = TRUE;

        VertexShader = compile vs_3_0 VS();
        PixelShader = compile ps_3_0 PS();
    }
}
