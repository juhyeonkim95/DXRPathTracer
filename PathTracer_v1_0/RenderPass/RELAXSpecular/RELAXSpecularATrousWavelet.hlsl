#include "RELAXSpecularCommon.hlsli"
#include "../Core/Common/CommonStructs.hlsli"

Texture2D gColorVariance : register(t0);
Texture2D gNormal : register(t1);
Texture2D gPositionMeshID : register(t2);
Texture2D gDepthDerivative : register(t3);
Texture2D<uint> gPathType : register(t4);
Texture2D gDeltaReflectionNormal : register(t5);
Texture2D gDeltaReflectionPositionMeshID : register(t6);
Texture2D<float> gRoughness : register(t7);

SamplerState s1 : register(s0);

static const int support = 2;

static const float kernelWeights[3] = { 1.0, 2.0 / 3.0, 1.0 / 6.0 };
static const float gaussKernel[9] = { 1.0 / 16.0, 1.0 / 8.0, 1.0 / 16.0, 1.0 / 8.0, 1.0 / 4.0, 1.0 / 8.0, 1.0 / 16.0, 1.0 / 8.0, 1.0 / 16.0 };


cbuffer PerFrameBuffer : register(b1)
{
    PerFrameData g_frameData;
};

bool eps_equal(float a, float b) {
    return round(a) == round(b);
}

struct VS_OUTPUT
{
    float4 pos: SV_POSITION;
    float2 texCoord: TEXCOORD;
};

float4 main(VS_OUTPUT input) : SV_TARGET
{
    int2 ipos = int2(input.pos.xy);

    uint pPathType = gPathType.Load(int3(ipos, 0)).r;
    if (!(pPathType & targetPathType)) {
        return float4(0, 0, 0, 0);
    }

    float4 pPositionMeshId = gPositionMeshID.Load(int3(ipos, 0));
    float3 pPosition = pPositionMeshId.rgb;
    float pMeshID = pPositionMeshId.a;

    float3 pNormal = gNormal.Load(int3(ipos, 0)).rgb;
    float pDepth = gNormal.Load(int3(ipos, 0)).w;

    float3 pColor = gColorVariance.Load(int3(ipos, 0)).rgb;
    float pVariance = gColorVariance.Load(int3(ipos, 0)).a;
    float pLuminance = luma(pColor);

    float3 pDeltaReflectionPosition = gDeltaReflectionPositionMeshID.Load(int3(ipos, 0)).rgb;
    float pDeltaReflectionMeshID = gDeltaReflectionPositionMeshID.Load(int3(ipos, 0)).a;
    float3 pDeltaReflectionNormal= gDeltaReflectionNormal.Load(int3(ipos, 0)).rgb;
    
    float pRoughness = gRoughness.Load(int3(ipos, 0)).r;


    int step = stepSize;


    float var = 0.0;
    for (int y0 = -1; y0 <= 1; y0++) {
        for (int x0 = -1; x0 <= 1; x0++) {
            const int2 p = ipos + int2(x0, y0);
            var += gaussKernel[x0 + 3 * y0 + 4] * gColorVariance.Load(int3(p, 0)).a;
        }
    }

    float2 depthDerivative = gDepthDerivative.Load(int3(ipos, 0)).rg;
    float fwidthZ = max(abs(depthDerivative.x), abs(depthDerivative.y));
    
    float pReflectionDistance = length(pDeltaReflectionPosition - pPosition);

    float customSigmaL = (sigmaL * sqrt(var) + epsilon) * pReflectionDistance;

    float customSigmaZ = sigmaZ;// *max(fwidthZ, 1e-8);

    float3 c = pColor;
    float v = pVariance;
    float weights = 1.0f;

    float3 pViewDir = normalize(g_frameData.cameraPosition - pPosition);

    for (int offsetx = -support; offsetx <= support; offsetx++) {
        for (int offsety = -support; offsety <= support; offsety++) {
            int2 ipos2 = ipos + int2(offsetx, offsety) * step;
            const bool outside = (ipos2.x < 0) || (ipos2.x >= screenSize.x) || (ipos2.y < 0) || (ipos2.y >= screenSize.y);
            
            uint qPathType = gPathType.Load(int3(ipos2, 0)).r;
            const bool pathTypeValid = (qPathType & targetPathType);
            float4 qPositionMeshId = gPositionMeshID.Load(int3(ipos2, 0));
            float qMeshID = qPositionMeshId.a;

            float3 qDeltaReflectionPosition = gDeltaReflectionPositionMeshID.Load(int3(ipos2, 0)).rgb;
            float qDeltaReflectionMeshID = gDeltaReflectionPositionMeshID.Load(int3(ipos2, 0)).a;

            if (!outside && (offsetx != 0 || offsety != 0) && pathTypeValid && (qMeshID == pMeshID)) {
                
                float3 qPosition = qPositionMeshId.rgb;
                float3 qNormal = gNormal.Load(int3(ipos2, 0)).rgb;
                float qDepth = gNormal.Load(int3(ipos2, 0)).w;

                float3 qColor = gColorVariance.Load(int3(ipos2, 0)).rgb;
                float qVariance = gColorVariance.Load(int3(ipos2, 0)).a;
                float qLuminance = luma(qColor);

                float3 qDeltaReflectionNormal = gDeltaReflectionNormal.Load(int3(ipos2, 0)).rgb;

                float cosDifference = g_frameData.cameraPosition ;
                float3 qViewDir = normalize(g_frameData.cameraPosition - qPosition);
                float cosFactor = pow(dot(pViewDir, qViewDir), 1 / (pRoughness + 0.01));

                //bool reflectedMeshIDDifferent = (qDeltaReflectionMeshID == pDeltaReflectionMeshID);


                // float w = calculateWeight(pDepth, qDepth, customSigmaZ * length(float2(offsetx, offsety)), pNormal, qNormal, pLuminance, qLuminance, customSigmaL);
                // float w = calculateWeight(pDepth, qDepth, step * sigmaZ * abs(dot(depthDerivative, float2(offsetx, offsety))), pNormal, qNormal, pLuminance, qLuminance, customSigmaL);
                float w = calculateWeightPosition(pPosition, qPosition, customSigmaZ, pNormal, qNormal, pLuminance, qLuminance, customSigmaL);
                //float wDelta = calculateWeightPosition(pDeltaReflectionPosition, qDeltaReflectionPosition, sigmaZ, pDeltaReflectionNormal, qDeltaReflectionNormal, 0, 0, customSigmaL);
                //w *= lerp(wDelta, 1, clamp(pRoughness * roughnessMultiplier, 0, 1));
                // w = lerp(wDelta, w, clamp(pRoughness* roughnessMultiplier, 0, 1));
                w *= cosFactor;
                //w *= reflectedMeshIDDifferent ? 0 : 1.0f;

                float weight = kernelWeights[abs(offsety)] * kernelWeights[abs(offsetx)] * w;

                c += weight * qColor;
                v += weight * weight * qVariance;
                weights += weight;
            }
        }
    }

    //return float4(pReflectionDistance, 0, 0, 1);

    float4 cvnext;
    if (weights > epsilon) {
        cvnext.rgb = c / weights;
        cvnext.a = v / (weights * weights);
    }
    else {
        cvnext = gColorVariance.Load(int3(ipos, 0));
    }

    return cvnext;
}