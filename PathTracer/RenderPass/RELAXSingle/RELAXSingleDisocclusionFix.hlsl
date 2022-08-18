#include "RELAXSingleCommon.hlsli"
Texture2D gColorVariance[4] : register(t0);
Texture2D gMomentsAndLengthHistory[4] : register(t4);
Texture2D gPositionMeshID[3] : register(t8);
Texture2D gNormalDepth[3] : register(t11);
Texture2D gDepthDerivative : register(t14);
Texture2D<uint> gPathType : register(t15);

SamplerState s1 : register(s0);

struct VS_OUTPUT
{
    float4 pos: SV_POSITION;
    float2 texCoord: TEXCOORD;
};

struct PS_OUT
{
    float4 color1: SV_Target0;
    float4 color2: SV_Target1;
    float4 color3: SV_Target2;
    float4 color4: SV_Target3;
};

float4 disocclusionFix(int i, int j, in int2 ipos) 
{
    float h = gMomentsAndLengthHistory[i].Load(int3(ipos, 0)).w;

    if (h < 4.0) {
        uint pPathType = gPathType.Load(int3(ipos, 0)).r;
        //if (!(pPathType & targetPathType)) {
        //    return float4(0, 0, 0, 0);
        //}

        float3 pPosition = gPositionMeshID[j].Load(int3(ipos, 0)).rgb;
        float3 pNormal = gNormalDepth[j].Load(int3(ipos, 0)).rgb;
        float pDepth = gNormalDepth[j].Load(int3(ipos, 0)).w;

        float3 pColor = gColorVariance[i].Load(int3(ipos, 0)).rgb;
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

                uint qPathType = gPathType.Load(int3(ipos2, 0)).r;
                const bool pathTypeValid = (qPathType & targetPathType);

                if (!outside && pathTypeValid)
                {
                    float3 qPosition = gPositionMeshID[j].Load(int3(ipos2, 0)).rgb;
                    float3 qNormal = gNormalDepth[j].Load(int3(ipos2, 0)).rgb;
                    float qDepth = gNormalDepth[j].Load(int3(ipos2, 0)).w;
                    float3 qColor = gColorVariance[i].Load(int3(ipos2, 0)).rgb;
                    float2 qMoments = gMomentsAndLengthHistory[i].Load(int3(ipos2, 0)).rg;

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
        return gColorVariance[i].Load(int3(ipos, 0));
    }
}

PS_OUT main(VS_OUTPUT input) : SV_Target
{
    PS_OUT output;
    const int2 ipos = int2(input.pos.xy);
    uint pathType = gPathType.Load(int3(ipos, 0)).r;
    if ((pathType & BSDF_LOBE_DIFFUSE_REFLECTION)) {
        output.color1 = disocclusionFix(0, 0, ipos);
    }
    else {
        output.color1 = 0.0f;
    }
    if ((pathType & BSDF_LOBE_GLOSSY_REFLECTION)) {
        output.color2 = disocclusionFix(1, 0, ipos);
    }
    else {
        output.color2 = 0.0f;
    }
    if ((pathType & BSDF_LOBE_DELTA_REFLECTION)) {
        output.color3 = disocclusionFix(2, 1, ipos);
    }
    else {
        output.color3 = 0.0f;
    }
    if ((pathType & BSDF_LOBE_DELTA_TRANSMISSION)) {
        output.color4 = disocclusionFix(3, 2, ipos);
    }
    else {
        output.color4 = 0.0f;
    }
    return output;
}