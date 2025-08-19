#include "../shader_semantics.hlsli"

struct VSInput
{
    VERTEX_ATTR(POSITION,  float3,   Position);
    VERTEX_ATTR(TEXCOORD,  float2,   TexCoord);
    VERTEX_ATTR(NORMAL,    float3,   Normal);
    VERTEX_ATTR(TANGENT,   float3,   Tangent);
    VERTEX_ATTR(BITANGENT, float3,   Bitangent);
    VERTEX_ATTR(COLOR,     float4,   Color);
    VERTEX_ATTR(INSTANCE,  float4x4, Instance);
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
    float2 TexCoord : TEXCOORD1;
    float3 Normal : NORMAL2;
    float3 Tangent : TANGENT3;
    float3 Bitangent : BITANGENT4;
    float4 Color : COLOR5;
    float3 WorldPos : TEXCOORD0; 
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

    output.WorldPos = worldPos.xyz;

    output.Position = mul(ViewParam.VP, worldPos);

    float3x3 normalMat =
    {
        normalize(worldMatrix[0].xyz),
        normalize(worldMatrix[1].xyz),
        normalize(worldMatrix[2].xyz)
    };
#ifdef __spirv__
    output.Normal    = normalize(mul(input.Normal,    normalMat));
    output.Tangent   = normalize(mul(input.Tangent,   normalMat));
    output.Bitangent = normalize(mul(input.Bitangent, normalMat));
#else
    output.Normal = normalize(mul(normalMat, input.Normal));
    output.Tangent = normalize(mul(normalMat, input.Tangent));
    output.Bitangent = normalize(mul(normalMat, input.Bitangent));
#endif

    output.TexCoord = input.TexCoord;
    output.Color = input.Color;
    return output;
}
