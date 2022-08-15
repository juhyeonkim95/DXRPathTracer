#include "RELAXCommon.hlsli"

Texture2D gColorHistory : register(t0);
Texture2D gCurrentColor : register(t1);
Texture2D<float2> gMomentsHistory : register(t2);
Texture2D<float> gHistoryLength : register(t3);

Texture2D<float2> gMotionVector : register(t4);

Texture2D gPositionMeshID : register(t5);
Texture2D gPositionMeshIDPrev : register(t6);
Texture2D gNormal : register(t7);
Texture2D gNormalPrev : register(t8);


SamplerState s1 : register(s0);


struct VS_OUTPUT
{
    float4 pos: SV_POSITION;
    float2 texCoord: TEXCOORD;
};

struct PS_OUT
{
    float4 color: SV_Target0;
    float2 moment: SV_Target1;
    float historyLength : SV_Target2;
};

bool isReprojectionTapValid(in float3 currentWorldPos, in float3 previousWorldPos, in float3 currentNormal, float disocclusionThreshold)
{
    // Check if plane distance is acceptable
    float3 posDiff = currentWorldPos - previousWorldPos;
    float maxPlaneDistance = abs(dot(posDiff, currentNormal));

    return maxPlaneDistance < disocclusionThreshold;
}

bool checkConsistency(in int2 ipos, in float2 prevPixel)
{
    float3 position = gPositionMeshID.Load(int3(ipos, 0)).rgb;
    float meshID = gPositionMeshID.Load(int3(ipos, 0)).w;
    float3 normal = gNormal.Load(int3(ipos, 0)).rgb;
    float depth = gNormal.Load(int3(ipos, 0)).w;

    float3 previousPosition = gPositionMeshIDPrev.Sample(s1, prevPixel).rgb;
    float previousMeshID = gPositionMeshIDPrev.Sample(s1, prevPixel).w;
    float3 previousNormal = gNormalPrev.Sample(s1, prevPixel).rgb;
    float previousDepth = gNormalPrev.Sample(s1, prevPixel).w;

    bool consistency = true;

    // (1) Check geometry consistency
    // float disocclusionThreshold = gDisocclusionDepthThreshold * depth;
    // consistency = consistency && isReprojectionTapValid(position, previousPosition, normal, disocclusionThreshold);
    
    consistency = consistency && (length(position - previousPosition) < 0.1f);

    // (2) check normal
    // consistency = consistency && (dot(normal, previousNormal) > gNormalThreshold);

    // (3) check material
    // consistency = consistency && (meshID == previousMeshID);

    // (4) check outside
    bool outside = (prevPixel.x < 0) || (prevPixel.x > 1) || (prevPixel.y < 0) || (prevPixel.y > 1);
    consistency = consistency && !outside;

    // (5) check param changed
    // consistency = consistency && !g_frameData.paramChanged;

    // (6) if camera not moved, consistent
    // consistency = consistency || !g_frameData.cameraChanged;

    return consistency;
}


PS_OUT main(VS_OUTPUT input) : SV_Target
{
    const int2 ipos = int2(input.pos.xy);
    float2 prev_uv = gMotionVector.Load(int3(ipos, 0)).rg;

    bool consistency = checkConsistency(ipos, prev_uv);

    // float consistency = gMotionVector.Sample(s1, uv).a;
    float3 col = gCurrentColor.Load(int3(ipos, 0)).rgb;

    // float3 col = gCurrentColor.Sample(s1, uv).rgb;
    float3 col_prev = gColorHistory.Sample(s1, prev_uv).rgb;
    float2 moment_prev = gMomentsHistory.Sample(s1, prev_uv).rg;
    float historyLength = gHistoryLength.Load(int3(ipos, 0)).x;
    // bool success = historyLength > 1.0f;
    //bool success = consistency >= 1.0;
    historyLength = min(255.0f, (consistency ? (historyLength + 1.0f) : 1.0f));

    // this adjusts the alpha for the case where insufficient history is available.
    // It boosts the temporal accumulation to give the samples equal weights in
    // the beginning.
    //const float alpha = max(gAlpha, 1.0 / historyLength);
    //const float alphaMoments = max(gMomentsAlpha, 1.0 / historyLength);

    //const float alpha = max(1 - historyLength, 0.2f);
    //const float alphaMoments = max(1 - historyLength, 0.2f);

    float alpha = consistency? max(1.0 / gMaxAccumulatedFrame, 1.0 / historyLength) : 1.0f;
    const float alphaMoments = consistency? max(1.0 / gMaxAccumulatedFrame, 1.0 / historyLength) : 1.0f;
    // alpha = 1.0f;

    //const float alpha = success ? 1.0 / historyLength : 1.0f;
    //const float alphaMoments = success ? 1.0 / historyLength : 1.0f;

    //const float alpha =  max(gAlpha, 1.0 / historyLength);
    //const float alphaMoments =  max(gMomentsAlpha, 1.0 / historyLength);

    // compute first two moments of luminance
    float new_luma = luma(col);
    float2 moments = float2(new_luma, new_luma * new_luma);

    // temporal integration of the moments
    moments = lerp(moment_prev, moments, alphaMoments);

    float variance = max(0.f, moments.g - moments.r * moments.r);

    PS_OUT output;
    output.color = float4(lerp(col_prev, col, alpha), variance);
    //output.color = float4(alpha, 0, 0, 1);

    output.moment = moments;
    output.historyLength = historyLength;

    return output;
}