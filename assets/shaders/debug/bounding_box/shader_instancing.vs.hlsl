// ======================================
// BOUNDING BOX VISUALIZATION SHADER
// ======================================
// This shader supports two rendering modes:
//
// 1. OBB (Oriented Bounding Box) Mode:
//    - Uses local model bounds + model matrix transformation
//    - Bounding box rotates and scales with the model
//    - More accurate representation of model geometry
//    - Yellow wireframe color
//
// 2. AABB (Axis-Aligned Bounding Box) Mode:
//    - Uses pre-calculated world bounds directly
//    - Always aligned with world axes (no rotation)
//    - Simpler, faster rendering (no matrix multiplication)
//    - Cyan wireframe color
//
// To switch modes: Comment/uncomment the #define lines in main()
// ======================================

struct VSInput
{
#ifdef __spirv__
    [[vk::location(0)]] float3   Position        : POSITION0;
    [[vk::location(1)]] float4x4 InstanceMatrix  : INSTANCE1;
    [[vk::location(5)]] float3   LocalMinCorner  : LOCAL_MIN_CORNER5;
    [[vk::location(6)]] float3   LocalMaxCorner  : LOCAL_MAX_CORNER6;
    [[vk::location(7)]] float3   WorldMinCorner  : WORLD_MIN_CORNER7;
    [[vk::location(8)]] float3   WorldMaxCorner  : WORLD_MAX_CORNER8;
#else
    float3 Position : POSITION0;
    float4x4 InstanceMatrix : INSTANCE1;
    float3 LocalMinCorner : LOCAL_MIN_CORNER5;
    float3 LocalMaxCorner : LOCAL_MAX_CORNER6;
    float3 WorldMinCorner : WORLD_MIN_CORNER7;
    float3 WorldMaxCorner : WORLD_MAX_CORNER8;
#endif
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
    float3 Color : COLOR;
};

VSOutput main(VSInput input)
{
    VSOutput output = (VSOutput) 0;
    
    // ======================================
    // RENDERING MODE SELECTION
    // ======================================
    // Uncomment one of the modes below:
    
    // MODE 1: OBB (Oriented Bounding Box) - uses local bounds + model matrix
    // More accurate for rotated objects, follows exact model orientation
    // #define USE_OBB_MODE
    
    // MODE 2: AABB (Axis-Aligned Bounding Box) - uses world bounds directly  
    // Simpler, always aligned with world axes, no matrix multiplication needed
    #define USE_AABB_MODE
    
    // ======================================
    
    float4 worldPos;
    
#ifdef USE_OBB_MODE
    // OBB MODE: Local bounds + model matrix transformation
    float3 size = input.LocalMaxCorner - input.LocalMinCorner;
    float3 center = (input.LocalMinCorner + input.LocalMaxCorner) * 0.5;
    
    // Transform unit cube vertex to local bounding box space
    float3 localPosition = input.Position * size + center;
    
    // Transform by model matrix to world space
#ifdef __spirv__
    worldPos = mul(float4(localPosition, 1.0), input.InstanceMatrix);
#else
    worldPos = mul(input.InstanceMatrix, float4(localPosition, 1.0));
#endif

    output.Color = float3(1.0, 1.0, 0.0); // Yellow for OBB mode

#elif defined(USE_AABB_MODE)
    // AABB MODE: World bounds directly (no matrix needed)
    float3 size = input.WorldMaxCorner - input.WorldMinCorner;
    float3 center = (input.WorldMinCorner + input.WorldMaxCorner) * 0.5;
    
    // Transform unit cube vertex directly to world bounding box space
    float3 worldPosition = input.Position * size + center;
    worldPos = float4(worldPosition, 1.0);

    output.Color = float3(0.0, 1.0, 1.0); // Cyan for AABB mode  

#else
    // ERROR: No mode selected - render in red as warning
    worldPos = float4(input.Position, 1.0);
    output.Color = float3(1.0, 0.0, 0.0); // Red wireframe
#endif

    output.Position = mul(ViewParam.VP, worldPos);
    
    return output;
}