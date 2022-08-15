#include "../Core/Common/CommonStructs.hlsli"
#include "../Core/BSDF/BSDFLobes.hlsli"

Texture2D gPositionMeshIDPrev : register(t0);
Texture2D gNormalPrev : register(t1);
Texture2D gPositionMeshID : register(t2);
Texture2D gNormal : register(t3);
Texture2D gDeltaTransmissionPositionMeshID : register(t4);
Texture2D gDeltaTransmissionPositionMeshIDPrev : register(t5);
Texture2D<uint> gPathType : register(t6);


SamplerState s1 : register(s0);


struct VS_OUTPUT
{
    float4 pos: SV_POSITION;
    float2 texCoord: TEXCOORD;
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

    float3 normal = gNormal.Load(int3(ipos, 0)).rgb;
    float depth = gNormal.Load(int3(ipos, 0)).w;
    float meshID = gPositionMeshID.Load(int3(ipos, 0)).w;

    float3 position = gPositionMeshID.Load(int3(ipos, 0)).rgb;
    float3 deltaPosition = gDeltaTransmissionPositionMeshID.Load(int3(ipos, 0)).rgb;


    float3 expectPosition = position;
    float3 deltaPositionFromEstimatedPixel;
    float2 prevPixel;
    for (int i = 0; i < 5; i++) {
        // Reprojection
        float4 projCoord = mul(float4(expectPosition, 1.0f), g_frameData.previousProjView);
        projCoord /= projCoord.w;
        prevPixel = float2(projCoord.x, -projCoord.y);
        prevPixel = (prevPixel + 1) * 0.5;

        deltaPositionFromEstimatedPixel = gDeltaTransmissionPositionMeshIDPrev.Sample(s1, prevPixel).rgb;
        
        float3 estimationDifference = deltaPosition - deltaPositionFromEstimatedPixel;

        if (length(estimationDifference) < 0.1f) {
            break;
        }

        expectPosition += estimationDifference * 0.2f;
    }
    return prevPixel;

    ////bool consistency = true;

    //// (1) Check geometry consistency
    //// float gDisocclusionDepthThreshold = 0.02f;
    //// float disocclusionThreshold = gDisocclusionDepthThreshold * depth;
    //// consistency = consistency && isReprojectionTapValid(position, previousPosition, normal, disocclusionThreshold);
    //consistency = consistency && length(deltaPosition - deltaPositionFromEstimatedPixel) < 0.1f;

    //// consistency = consistency && length(position - previousPosition) < 0.1f;

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