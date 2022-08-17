#include "../Core/Common/CommonStructs.hlsli"
#include "../Core/BSDF/BSDFLobes.hlsli"

Texture2D gPositionMeshIDPrev : register(t0);
Texture2D gNormalDepthPrev : register(t1);
Texture2D gPositionMeshID : register(t2);
Texture2D gNormalDepth : register(t3);
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

float2 main(VS_OUTPUT input) : SV_TARGET
{
    const int2 ipos = int2(input.pos.xy);
    uint pathType = gPathType.Load(int3(ipos, 0)).r;

    if (!(pathType & BSDF_LOBE_DELTA_TRANSMISSION)) {
        return float2(0, 0);
    }

    float3 normal = gNormalDepth.Load(int3(ipos, 0)).rgb;
    float depth = gNormalDepth.Load(int3(ipos, 0)).w;
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
}