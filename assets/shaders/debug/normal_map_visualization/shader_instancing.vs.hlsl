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

struct VSInput
{
#ifdef __spirv__
    [[vk::location(0)]] float3 Position   : POSITION0;
    [[vk::location(1)]] float2 TexCoord   : TEXCOORD1; 
    [[vk::location(2)]] float3 Normal     : NORMAL2;  
    [[vk::location(3)]] float3 Tangent    : TANGENT3;  
    [[vk::location(4)]] float3 Bitangent  : BITANGENT4;  
    [[vk::location(5)]] float4x4 Instance : INSTANCE5;  
#else
    float3   Position  : POSITION0;
    float2   TexCoord  : TEXCOORD1;
    float3   Normal    : NORMAL2;
    float3   Tangent   : TANGENT3;
    float3   Bitangent : BITANGENT4;
    float4x4 Instance  : INSTANCE5;
#endif
};

struct VSOutput
{
    float4 Position  : SV_POSITION;
    float2 TexCoord  : TEXCOORD1;
    float3 Normal    : NORMAL2;
    float3 Tangent   : TANGENT3;
    float3 Bitangent : BITANGENT4;
};

VSOutput main(VSInput input)
{
    // TODO: model, VP matrices - column major multiplication, doesn't depend on SPIRV, DXIL version. However, instance matrix do. Consider make it the same regardles of rendering API
    
    VSOutput output = (VSOutput) 0;

#ifdef __spirv__
    float4x4 worldMatrix = mul(ModelParam.ModelMatrix, input.Instance);
    
    output.Position = mul(float4(input.Position, 1.0), worldMatrix);
    
    output.Normal = normalize(mul(input.Normal, (float3x3) worldMatrix));
    output.Tangent = normalize(mul(input.Tangent, (float3x3) worldMatrix));
    output.Bitangent = normalize(mul(input.Bitangent, (float3x3) worldMatrix));
#else
    float4x4 worldMatrix = mul(input.Instance, ModelParam.ModelMatrix);
    
    output.Position = mul(worldMatrix, float4(input.Position, 1.0));
    
    output.Normal = normalize(mul((float3x3) worldMatrix, input.Normal));
    output.Tangent = normalize(mul((float3x3) worldMatrix, input.Tangent));
    output.Bitangent = normalize(mul((float3x3) worldMatrix, input.Bitangent));
#endif
    
    output.Position = mul(ViewParam.VP, output.Position);

    output.TexCoord = input.TexCoord;
    
    return output;
}
