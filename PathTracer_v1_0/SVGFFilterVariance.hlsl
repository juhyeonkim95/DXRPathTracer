#include "SVGFCommon.hlsli"
Texture2D gColorVariance : register(t0);
Texture2D gNormal : register(t1);
Texture2D gPositionMeshID : register(t2);
Texture2D gMoments : register(t3);
Texture2D gHistoryLength : register(t4);

SamplerState s1 : register(s0);

struct VS_OUTPUT
{
    float4 pos: SV_POSITION;
    float2 texCoord: TEXCOORD;
};

float4 main(VS_OUTPUT input) : SV_Target
{
    float2 uv = input.texCoord;

    float h = gHistoryLength.Sample(s1, uv).x;

    if (h < 4.0) {
        float3 pPosition = gPositionMeshID.Sample(s1, input.texCoord).rgb;
        float3 pNormal = gNormal.Sample(s1, input.texCoord).rgb;
        float3 pColor = gColorVariance.Sample(s1, input.texCoord).rgb;
        float pLuminance = luma(pColor);

        float weights = 0.0f;
        float3 sumColor = 0.0f;
        float2 sumMoments = 0.0f;

        const int support = 3;
        for (int offsetx = -support; offsetx <= support; offsetx++) {
            for (int offsety = -support; offsety <= support; offsety++) {
                float2 loc = input.texCoord + texelSize * float2(offsetx, offsety);

                float3 qPosition = gPositionMeshID.Sample(s1, loc).rgb;
                float3 qNormal = gNormal.Sample(s1, loc).rgb;
                float3 qColor = gColorVariance.Sample(s1, loc).rgb;
                float2 qMoments = gMoments.Sample(s1, loc).rg;

                float qLuminance = luma(qColor);

                float w = calculateWeight(pPosition, qPosition, pNormal, qNormal, pLuminance, qLuminance, sigmaL);
                weights += w;

                sumColor += w * qColor;
                sumMoments += w * qMoments;
            }
        }

        weights = max(weights, 1e-6f);
        sumColor /= weights;
        sumMoments /= weights;

        float variance = sumMoments.g - sumMoments.r * sumMoments.r;

        variance *= 4.0f / h;

        return float4(sumColor, variance);
    }
    else {
        return gColorVariance.Sample(s1, input.texCoord);
    }
}