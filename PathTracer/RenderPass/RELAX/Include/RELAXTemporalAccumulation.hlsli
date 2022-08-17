#include "Include/RELAXCommon.hlsli"

Texture2D gColorHistory : register(t0);
Texture2D gCurrentColor : register(t1);
Texture2D<float2> gMomentsHistory : register(t2);
Texture2D<float> gHistoryLength : register(t3);

Texture2D<float2> gMotionVector : register(t4);

Texture2D gPositionMeshID : register(t5);
Texture2D gPositionMeshIDPrev : register(t6);
Texture2D gNormalDepth : register(t7);
Texture2D gNormalDepthPrev : register(t8);
Texture2D<uint> gPathType : register(t9);
Texture2D<float> gRoughness : register(t10);


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
    float3 normal = gNormalDepth.Load(int3(ipos, 0)).rgb;
    float depth = gNormalDepth.Load(int3(ipos, 0)).w;

    float3 previousPosition = gPositionMeshIDPrev.Sample(s1, prevPixel).rgb;
    float previousMeshID = gPositionMeshIDPrev.Sample(s1, prevPixel).w;
    float3 previousNormal = gNormalDepthPrev.Sample(s1, prevPixel).rgb;
    float previousDepth = gNormalDepthPrev.Sample(s1, prevPixel).w;

    bool consistency = true;

    // (1) Check geometry consistency
    float disocclusionThreshold = gDisocclusionDepthThreshold * depth;
    consistency = consistency && (depth == 0 || isReprojectionTapValid(position, previousPosition, normal, disocclusionThreshold));
    
    consistency = consistency && (length(position - previousPosition) < gPositionThreshold);

    // (2) check normal
    consistency = consistency && (dot(normal, previousNormal) > gNormalDepthThreshold);

    // (3) check material
    consistency = consistency && (meshID == previousMeshID);

    // (4) check outside
    bool outside = (prevPixel.x < 0) || (prevPixel.x > 1) || (prevPixel.y < 0) || (prevPixel.y > 1);
    consistency = consistency && !outside;

#ifdef RELAX_SPECULAR
    float roughness = gRoughness.Load(int3(ipos, 0)).r;
    // Specular Lobe
    float3 viewDir = normalize(g_frameData.cameraPosition - position);
    float3 prevViewDir = normalize(g_frameData.previousCameraPosition - position);
    float cosFactor = pow(dot(viewDir, prevViewDir), 1 / (roughness + 0.01));
    consistency = consistency && (cosFactor > 0.8);
#endif

    // (5) check param changed
    consistency = consistency && !g_frameData.paramChanged;

    // (6) if camera not moved, consistent
    consistency = consistency || !g_frameData.cameraChanged;

    return consistency;
}


PS_OUT main(VS_OUTPUT input) : SV_Target
{
    PS_OUT output;
    const int2 ipos = int2(input.pos.xy);
    uint pathType = gPathType.Load(int3(ipos, 0)).r;
    if (!(pathType & targetPathType)) {
        return output;
    }

    float2 prevUV = gMotionVector.Load(int3(ipos, 0)).rg;

    bool consistency = checkConsistency(ipos, prevUV);

    float3 color = gCurrentColor.Load(int3(ipos, 0)).rgb;
    float3 prevColor = gColorHistory.Sample(s1, prevUV).rgb;
    float2 prevMoment = gMomentsHistory.Sample(s1, prevUV).rg;
    float prevHistoryLength = gHistoryLength.Load(int3(ipos, 0)).x;

    float historyLength = min(255.0f, (consistency ? (prevHistoryLength + 1.0f) : 1.0f));

    float alpha = consistency? max(1.0 / gMaxAccumulatedFrame, 1.0 / historyLength) : 1.0f;
    const float alphaMoments = consistency? max(1.0 / gMaxAccumulatedFrame, 1.0 / historyLength) : 1.0f;


    // compute first two moments of luminance
    float luminance = luma(color);
    float2 moments = float2(luminance, luminance * luminance);

    // temporal integration of the moments
    moments = lerp(prevMoment, moments, alphaMoments);
    float variance = max(0.f, moments.g - moments.r * moments.r);
    output.color = float4(lerp(prevColor, color, alpha), variance);

    float3 position = gPositionMeshID.Load(int3(ipos, 0)).rgb;
    float meshID = gPositionMeshID.Load(int3(ipos, 0)).w;
    float3 normal = gNormalDepth.Load(int3(ipos, 0)).rgb;
    float depth = gNormalDepth.Load(int3(ipos, 0)).w;

    float3 previousPosition = gPositionMeshIDPrev.Sample(s1, prevUV).rgb;

    // output.color = float4(position, variance);
    /*if (length(position - previousPosition) < 0.1f)
    {
        output.color = float4(1, 0, 0, 0);
    }*/

    
    output.moment = moments;
    output.historyLength = historyLength;

    return output;
}