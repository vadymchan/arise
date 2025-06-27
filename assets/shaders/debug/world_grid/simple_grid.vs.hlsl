struct ViewUniformBuffer
{
    float4x4 V;
    float4x4 P;
    float4x4 VP;
    float4x4 InvV;
    float4x4 InvP;
    float4x4 InvVP;
    float3 EyeWorld;
    float padding0;
};

cbuffer ViewParam : register(b0, space0)
{
    ViewUniformBuffer ViewParam;
}

struct VSOut
{
    float4 pos : SV_POSITION;
    float2 ndc : TEXCOORD0;
};


VSOut main(uint id : SV_VertexID)
{
    float2 tc = float2((id << 1) & 2, id & 2);
    VSOut o;
    o.pos = float4(tc * 2.0 - 1.0, 0.0, 1.0);
    o.ndc = o.pos.xy;
    return o;
}