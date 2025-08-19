
#include "../../shader_semantics.hlsli"

struct VSInput
{
    VERTEX_ATTR(POSITION, float3,   Position);
    VERTEX_ATTR(NORMAL,   float3,   Normal);
    VERTEX_ATTR(INSTANCE, float4x4, Instance);
};

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

struct ModelUniformBuffer
{
    float4x4 ModelMatrix;
};
cbuffer ModelParam : register(b0, space1)
{
    ModelUniformBuffer ModelParam;
}

struct VSOutput
{
    float4 Position : SV_POSITION;
    float4 Color : COLOR0;
};

VSOutput main(VSInput input)
{
    VSOutput output = (VSOutput) 0;

#ifdef __spirv__
    float4x4 worldMatrix = mul(ModelParam.ModelMatrix, input.Instance);
    float4 worldPos = mul(float4(input.Position, 1.0), worldMatrix);
#else
    float4x4 worldMatrix = mul(input.Instance, ModelParam.ModelMatrix);
    float4 worldPos = mul(worldMatrix, float4(input.Position, 1.0));
#endif
    
    output.Position = mul(ViewParam.VP, worldPos);
    
    output.Color = float4(0.0, 0.0, 0.0, 0.0);
    
    return output;
}