Texture2D gPositionMeshIDPrev : register(t0);
Texture2D gNormalPrev : register(t1);
Texture2D gPositionMeshID : register(t2);
Texture2D gNormal : register(t3);

SamplerState s1 : register(s0);

struct PerFrameData
{
    float4 u;
    float4 v;
    float4 w;
    float4 cameraPosition;
    uint frameNumber;
    uint totalFrameNumber;
    uint lightNumber;
    uint renderMode;
    float4x4 envMapTransform;
    float4x4 previousProjView;
};

struct VS_OUTPUT
{
    float4 pos: SV_POSITION;
    float2 texCoord: TEXCOORD;
};

cbuffer ConstantBuffer : register(b0)
{
    PerFrameData g_frameData;
    // float4x4 previousProjView;
};


float4 main(VS_OUTPUT input) : SV_TARGET
{
    float3 position = gPositionMeshID.Sample(s1, input.texCoord).rgb;

    float4 projCoord = mul(float4(position, 1.0f), g_frameData.previousProjView);
    projCoord /= projCoord.w;
    float2 prevPixel = float2(projCoord.x, -projCoord.y);
    prevPixel = (prevPixel + 1) * 0.5;

    float previousMeshID = gPositionMeshIDPrev.Sample(s1, prevPixel).w;
    float3 previousNormal = gNormalPrev.Sample(s1, prevPixel).rgb;
    float meshID = gPositionMeshID.Sample(s1, input.texCoord).w;
    float3 normal = gNormal.Sample(s1, input.texCoord).rgb;

    bool consistency = (meshID == previousMeshID) && (dot(normal, previousNormal) > sqrt(2) / 2.0);

    return float4(prevPixel.x, prevPixel.y, 0.0f, float(consistency));
}