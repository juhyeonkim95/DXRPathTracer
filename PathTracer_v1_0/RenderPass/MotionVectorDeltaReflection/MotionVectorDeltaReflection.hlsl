#include "../Core/Common/CommonStructs.hlsli"
#include "../Core/BSDF/BSDFLobes.hlsli"

Texture2D gPositionMeshIDPrev : register(t0);
Texture2D gNormalDepthPrev : register(t1);
Texture2D gPositionMeshID : register(t2);
Texture2D gNormalDepth : register(t3);
Texture2D gDeltaReflectionPositionMeshID : register(t4);
Texture2D gDeltaReflectionPositionMeshIDPrev : register(t5);
Texture2D<uint> gPathType : register(t6);


SamplerState s1 : register(s0);


struct VS_OUTPUT
{
    float4 pos: SV_POSITION;
    float2 texCoord: TEXCOORD;
};

struct PS_OUT
{
    float2 motionVector: SV_Target0;
    float historyLength : SV_Target1;
};

cbuffer ConstantBuffer : register(b0)
{
    PerFrameData g_frameData;
};

bool isReprojectionTapValid(in float3 currentWorldPos, in float3 previousWorldPos, in float3 currentNormal, float disocclusionThreshold)
{
    // Check if plane distance is acceptable
    float3 posDiff = currentWorldPos - previousWorldPos;
    float maxPlaneDistance = abs(dot(posDiff, currentNormal));

    return maxPlaneDistance < disocclusionThreshold;
}

float2 main(VS_OUTPUT input) : SV_TARGET
{
    const int2 ipos = int2(input.pos.xy);

    uint pPathType = gPathType.Load(int3(ipos, 0)).r;
    //if (!(pPathType & BSDF_LOBE_DELTA)) {
    //    return float2(0,0);
    //}

    float3 normal = gNormalDepth.Load(int3(ipos, 0)).rgb;
    float depth = gNormalDepth.Load(int3(ipos, 0)).w;
    float meshID = gPositionMeshID.Load(int3(ipos, 0)).w;

    float3 position = gPositionMeshID.Load(int3(ipos, 0)).rgb;
    float3 reflectedPosition = gDeltaReflectionPositionMeshID.Load(int3(ipos, 0)).rgb;

    float3 prevCameraPos = g_frameData.previousCameraPosition;

    float3 v = prevCameraPos - position;
    float3 vN = dot(v, normal) * normal;
    float3 vNPerpen = v - vN;
    float3 prevCameraPosReflected = position + vNPerpen - vN;

    float3 q1 = prevCameraPosReflected;
    float3 q2 = reflectedPosition;
    float3 p = position;

    float k = dot((p - q1), normal) / dot((q2 - q1), normal);
    float3 p2 = q1 + k * (q2 - q1);
    if (g_frameData.renderMode == 1) {
        p2 = position;
    }

    // Reprojection
    float4 projCoord = mul(float4(p2, 1.0f), g_frameData.previousProjView);
    projCoord /= projCoord.w;
    float2 prevPixel = float2(projCoord.x, -projCoord.y);
    prevPixel = (prevPixel + 1) * 0.5;

    return prevPixel;

    //float3 previousNormal = gNormalDepthPrev.Sample(s1, prevPixel).rgb;
    //float previousDepth = gNormalDepthPrev.Sample(s1, prevPixel).w;
    //float previousMeshID = gPositionMeshIDPrev.Sample(s1, prevPixel).w;
    //float3 previousPosition = gPositionMeshIDPrev.Sample(s1, prevPixel).rgb;

    //float3 reflectedPositionFromEstimatedPixel = gDeltaReflectionPositionMeshIDPrev.Sample(s1, prevPixel).rgb;

    //bool consistency = true;

    //// (1) Check geometry consistency
    //// float gDisocclusionDepthThreshold = 0.02f;
    //// float disocclusionThreshold = gDisocclusionDepthThreshold * depth;
    //// consistency = consistency && isReprojectionTapValid(position, previousPosition, normal, disocclusionThreshold);

    //consistency = consistency && length(reflectedPosition - reflectedPositionFromEstimatedPixel) < 0.1f;

    //// (2) check normal
    //// consistency = consistency && (dot(normal, previousNormal) > sqrt(2) / 2.0);

    //// (3) check material
    //// consistency = consistency && (meshID == previousMeshID);

    //// (4) check outside
    //bool outside = (prevPixel.x < 0) || (prevPixel.x > 1) || (prevPixel.y < 0) || (prevPixel.y > 1);
    //consistency = consistency && !outside;

    //// (5) check param changed
    //consistency = consistency && !g_frameData.paramChanged;

    //// (6) if camera not moved, consistenct
    //consistency = consistency || !g_frameData.cameraChanged;

    //float historyLength = gHistoryLength.Sample(s1, prevPixel).r;
    //historyLength = min(255.0f, (consistency ? (historyLength + 1.0f) : 1.0f));

    //PS_OUT output;

    //output.motionVector = float2((prevPixel.x + float(consistency)) * 0.5, prevPixel.y);
    //// output.motionVector = float4(reflectedPositionFromEstimatedPixel, 1);

    //output.historyLength = historyLength;

    //return output;
}