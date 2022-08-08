#include "../Core/Common/CommonStructs.hlsli"

Texture2D gPositionMeshIDPrev : register(t0);
Texture2D gNormalPrev : register(t1);
Texture2D gPositionMeshID : register(t2);
Texture2D gNormal : register(t3);
Texture2D gHistoryLength : register(t4);

SamplerState s1 : register(s0);


struct VS_OUTPUT
{
    float4 pos: SV_POSITION;
    float2 texCoord: TEXCOORD;
};

struct PS_OUT
{
    float4 motionVector: SV_Target0;
    float historyLength : SV_Target1;
};

cbuffer ConstantBuffer : register(b0)
{
    PerFrameData g_frameData;
};


PS_OUT main(VS_OUTPUT input) : SV_TARGET
{
    float3 position = gPositionMeshID.Sample(s1, input.texCoord).rgb;

    // Reprojection
    float4 projCoord = mul(float4(position, 1.0f), g_frameData.previousProjView);
    projCoord /= projCoord.w;
    float2 prevPixel = float2(projCoord.x, -projCoord.y);
    prevPixel = (prevPixel + 1) * 0.5;

    bool consistency = true;

    float3 previousNormal = gNormalPrev.Sample(s1, prevPixel).rgb;
    float3 normal = gNormal.Sample(s1, input.texCoord).rgb;

    float previousDepth = gNormalPrev.Sample(s1, prevPixel).w;
    float depth = gNormal.Sample(s1, input.texCoord).w;

    float previousMeshID = gPositionMeshIDPrev.Sample(s1, prevPixel).w;
    float meshID = gPositionMeshID.Sample(s1, input.texCoord).w;

    float3 previousPosition = gPositionMeshIDPrev.Sample(s1, prevPixel).rgb;

    bool outside = (prevPixel.x < 0) || (prevPixel.x > 1) || (prevPixel.y < 0) || (prevPixel.y > 1);
    consistency = (!g_frameData.paramChanged && !outside && (meshID == previousMeshID) && (dot(normal, previousNormal) > sqrt(2) / 2.0)) && (length(position - previousPosition) < 0.1f);
    consistency = !g_frameData.cameraChanged || consistency;

    float historyLength = gHistoryLength.Sample(s1, prevPixel).r;
    // historyLength = min(32.0f, (consistency ? (historyLength + 1.0f) : 1.0f));
    historyLength =  (consistency ? (historyLength + 1.0f) : 1.0f);

    PS_OUT output;
    output.motionVector = float4(prevPixel.x, prevPixel.y, 0, float(consistency));
    output.historyLength = historyLength;

    return output;
}