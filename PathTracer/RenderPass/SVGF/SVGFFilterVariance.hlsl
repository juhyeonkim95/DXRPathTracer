#include "SVGFCommon.hlsli"
Texture2D gColorVariance : register(t0);
Texture2D gNormalDepth : register(t1);
Texture2D gPositionMeshID : register(t2);
Texture2D gMoments : register(t3);
Texture2D gHistoryLength : register(t4);
Texture2D gDepthDerivative : register(t5);

SamplerState s1 : register(s0);

struct VS_OUTPUT
{
    float4 pos: SV_POSITION;
    float2 texCoord: TEXCOORD;
};

float4 main(VS_OUTPUT input) : SV_Target
{
    int2 ipos = int2(input.pos.xy);

    float h = gHistoryLength.Load(int3(ipos, 0)).r;

    if (h < 4.0) {
        float3 pPosition = gPositionMeshID.Load(int3(ipos, 0)).rgb;
        float3 pNormal = gNormalDepth.Load(int3(ipos, 0)).rgb;
        float pDepth = gNormalDepth.Load(int3(ipos, 0)).w;

        float3 pColor = gColorVariance.Load(int3(ipos, 0)).rgb;
        float pLuminance = luma(pColor);

        float weights = 0.0f;
        float3 sumColor = 0.0f;
        float2 sumMoments = 0.0f;

        float2 depthDerivative = gDepthDerivative.Load(int3(ipos, 0)).rg;
        float fwidthZ = max(abs(depthDerivative.x), abs(depthDerivative.y));
        float customSigmaZ = sigmaZ * fwidthZ * 3.0f;

        const int support = 3;
        for (int offsetx = -support; offsetx <= support; offsetx++) {
            for (int offsety = -support; offsety <= support; offsety++) {
                const int2 ipos2 = ipos + int2(offsetx, offsety);
                const bool outside = (ipos2.x < 0) || (ipos2.x >= screenSize.x) || (ipos2.y < 0) || (ipos2.y >= screenSize.y);

                if (!outside) 
                {
                    float3 qPosition = gPositionMeshID.Load(int3(ipos2, 0)).rgb;
                    float3 qNormal = gNormalDepth.Load(int3(ipos2, 0)).rgb;
                    float qDepth = gNormalDepth.Load(int3(ipos2, 0)).w;
                    float3 qColor = gColorVariance.Load(int3(ipos2, 0)).rgb;
                    float2 qMoments = gMoments.Load(int3(ipos2, 0)).rg;

                    float qLuminance = luma(qColor);

                    // float w = calculateWeight(pDepth, qDepth, customSigmaZ * length(float2(offsetx, offsety)), pNormal, qNormal, pLuminance, qLuminance, sigmaL);
                    // float w = calculateWeight(pDepth, qDepth, sigmaZ * abs(dot(depthDerivative, float2(offsetx, offsety))), pNormal, qNormal, pLuminance, qLuminance, sigmaL);
                    float w = calculateWeightPosition(pPosition, qPosition, sigmaZ, pNormal, qNormal, pLuminance, qLuminance, sigmaL);


                    weights += w;

                    sumColor += w * qColor;
                    sumMoments += w * qMoments;
                }
                
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
        return gColorVariance.Load(int3(ipos, 0));
    }
}