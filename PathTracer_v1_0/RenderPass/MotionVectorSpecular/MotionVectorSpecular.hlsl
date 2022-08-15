#include "../Core/Common/CommonStructs.hlsli"

Texture2D gPositionMeshIDPrev : register(t0);
Texture2D gNormalPrev : register(t1);
Texture2D gPositionMeshID : register(t2);
Texture2D gNormal : register(t3);
Texture2D<float> gHistoryLength : register(t4);
Texture2D<float> gRoughness : register(t5);
Texture2D gDeltaReflectionPositionMeshID : register(t6);
Texture2D gDeltaReflectionPositionMeshIDPrev : register(t7);
Texture2D gDeltaReflectionMotionVector : register(t8);

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

cbuffer MVParameters : register(b1)
{
    float gNormalThreshold;
    float gPositionThreshold;
    float gDisocclusionDepthThreshold;
    float unused;
};

bool isReprojectionTapValid(in float3 currentWorldPos, in float3 previousWorldPos, in float3 currentNormal, float disocclusionThreshold)
{
    // Check if plane distance is acceptable
    float3 posDiff = currentWorldPos - previousWorldPos;
    float maxPlaneDistance = abs(dot(posDiff, currentNormal));

    return maxPlaneDistance < disocclusionThreshold;
}

PS_OUT main(VS_OUTPUT input) : SV_TARGET
{
    const int2 ipos = int2(input.pos.xy);
    
    float3 position = gPositionMeshID.Load(int3(ipos, 0)).rgb;

    float roughness = gRoughness.Load(int3(ipos, 0)).r;

    // Reprojection
    float4 projCoord = mul(float4(position, 1.0f), g_frameData.previousProjView);
    projCoord /= projCoord.w;
    float2 prevPixel = float2(projCoord.x, -projCoord.y);
    prevPixel = (prevPixel + 1) * 0.5;

    // float2 prevPixelDelta = gDeltaReflectionMotionVector.Load(int3(ipos, 0)).rgb;


    float3 previousNormal = gNormalPrev.Sample(s1, prevPixel).rgb;
    float3 normal = gNormal.Load(int3(ipos, 0)).rgb;

    float previousDepth = gNormalPrev.Sample(s1, prevPixel).w;
    float depth = gNormal.Load(int3(ipos, 0)).w;

    float previousMeshID = gPositionMeshIDPrev.Sample(s1, prevPixel).w;
    float meshID = gPositionMeshID.Load(int3(ipos, 0)).w;

    float3 previousPosition = gPositionMeshIDPrev.Sample(s1, prevPixel).rgb;

    bool consistency = true;

    // (1) Check geometry consistency
    float disocclusionThreshold = gDisocclusionDepthThreshold * depth;
    consistency = consistency && isReprojectionTapValid(position, previousPosition, normal, disocclusionThreshold);
    
    consistency = consistency && (length(position - previousPosition) < gPositionThreshold);

    // (2) check normal
    consistency = consistency && (dot(normal, previousNormal) > gNormalThreshold);

    // (3) check material
    consistency = consistency && (meshID == previousMeshID);

    // (4) check outside
    bool outside = (prevPixel.x < 0) || (prevPixel.x > 1) || (prevPixel.y < 0) || (prevPixel.y > 1);
    consistency = consistency && !outside;

    // (7) Specular Lobe
    float3 viewDir = normalize(g_frameData.cameraPosition - position);
    float3 prevViewDir = normalize(g_frameData.previousCameraPosition - position);
    float cosFactor = pow(dot(viewDir, prevViewDir), 1 / (roughness + 0.01));
    consistency = consistency && (cosFactor > 0.8);

    // (5) check param changed
    consistency = consistency && !g_frameData.paramChanged;
    
    // (6) if camera not moved, consistenct
    consistency = consistency || !g_frameData.cameraChanged;
    

    float historyLength = gHistoryLength.Sample(s1, prevPixel).r;
    historyLength = min(255.0f, (consistency ? (historyLength + 1.0f) : 1.0f));

    PS_OUT output;
    output.motionVector = float2((prevPixel.x + float(consistency)) * 0.5, prevPixel.y);
    //output.motionVector = float4((prevPixel.x + float(consistency)) * 0.5, prevPixel.y, 0, 0);
    //output.motionVector = float4(prevPixel.x , prevPixel.y, 0, 1);

    output.historyLength = historyLength;

    return output;
}