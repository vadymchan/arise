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

struct PSIn
{
    float4 pos : SV_POSITION;
    float2 ndc : TEXCOORD0;
};

float4 main(PSIn inp) : SV_TARGET
{
    //float2 ndc = inp.pos.xy;
    float2 ndc = inp.ndc;
    
    // H - homogeneous coordinates, W - world coordinate
    //float4 nearH = mul(ViewParam.InvVP, float4(ndc, 0.0, 1.0));
    float4 nearH = mul(ViewParam.InvP, float4(ndc, 0.0, 1.0));
    nearH = mul(ViewParam.InvV, nearH);
    float3 nearW = nearH.xyz / nearH.w;

    //float4 farH = mul(ViewParam.InvVP, float4(ndc, 1.0, 1.0));
    //float4 farH = mul(float4(ndc, 1.0, 1.0), ViewParam.InvVP);
    float4 farH = mul(ViewParam.InvP, float4(ndc, 1.0, 1.0));
    farH = mul(ViewParam.InvV, farH);
    float3 farW = farH.xyz / farH.w;

    //float3 rayDir = normalize(farW - nearW);

    float3 rayDir = normalize(farW - ViewParam.EyeWorld);
    
    const float EPS = 1e-5;
    if (abs(rayDir.y) < EPS)
        discard;

    
    float t = -ViewParam.EyeWorld.y / rayDir.y;
    if (t <= 0.0)
        discard; 

    //return float4(1, 0, 0, 1); 
    
    float3 wPos = ViewParam.EyeWorld + rayDir * t;


    float gridSize = 1.0;
    float2 g = abs(frac(wPos.xz / gridSize - 0.5) - 0.5) / fwidth(wPos.xz / gridSize);
    float minor = 1.0 - saturate(min(g.x, g.y));

    float2 gM = abs(frac(wPos.xz / (gridSize * 10) - 0.5) - 0.5) / fwidth(wPos.xz / (gridSize * 10));
    float major = 1.0 - saturate(min(gM.x, gM.y));

    float axisW = 0.02;
    float xAxis = 1.0 - smoothstep(0.0, axisW, abs(wPos.x));
    float zAxis = 1.0 - smoothstep(0.0, axisW, abs(wPos.z));

    float3 color = float3(0.3, 0.3, 0.3);
    color = lerp(color, float3(0.6, 0.6, 0.6), major);
    if (xAxis > 0.01)
        color = float3(1, 0, 0);
    if (zAxis > 0.01)
        color = float3(0, 0, 1); 

    float alpha = max(max(minor, major), max(xAxis, zAxis));
    alpha *= saturate(1.0 - t / 150.0); 

    if (alpha < 0.01)
        discard;
    return float4(color, alpha);
}