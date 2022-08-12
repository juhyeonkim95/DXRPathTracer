#include "NRDDeltaReflectionCommon.hlsli"
Texture2D gColorVariance : register(t0);
Texture2D gNormal : register(t1);
Texture2D gPositionMeshID : register(t2);
Texture2D gDepthDerivative : register(t3);

SamplerState s1 : register(s0);

static const int support = 2;

static const float kernelWeights[3] = { 1.0, 2.0 / 3.0, 1.0 / 6.0 };
static const float gaussKernel[9] = { 1.0 / 16.0, 1.0 / 8.0, 1.0 / 16.0, 1.0 / 8.0, 1.0 / 4.0, 1.0 / 8.0, 1.0 / 16.0, 1.0 / 8.0, 1.0 / 16.0 };



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

    float4 pPositionMeshId = gPositionMeshID.Load(int3(ipos, 0));
    float3 pPosition = pPositionMeshId.rgb;
    float pMeshID = pPositionMeshId.a;

    float3 pNormal = gNormal.Load(int3(ipos, 0)).rgb;
    float pDepth = gNormal.Load(int3(ipos, 0)).w;

    float3 pColor = gColorVariance.Load(int3(ipos, 0)).rgb;
    float pVariance = gColorVariance.Load(int3(ipos, 0)).a;
    float pLuminance = luma(pColor);

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

    float customSigmaL = (sigmaL * sqrt(var) + epsilon);
    float customSigmaZ = sigmaZ * step * max(fwidthZ, 1e-8);

    float3 c = pColor;
    float v = pVariance;
    float weights = 1.0f;


    for (int offsetx = -support; offsetx <= support; offsetx++) {
        for (int offsety = -support; offsety <= support; offsety++) {
            int2 ipos2 = ipos + int2(offsetx, offsety) * step;

            const bool outside = (ipos2.x < 0) || (ipos2.x >= screenSize.x) || (ipos2.y < 0) || (ipos2.y >= screenSize.y);

            if (!outside && (offsetx != 0 || offsety != 0)) {
                float4 qPositionMeshId = gPositionMeshID.Load(int3(ipos2, 0));
                float qMeshID = qPositionMeshId.a;

                float3 qPosition = qPositionMeshId.rgb;
                float3 qNormal = gNormal.Load(int3(ipos2, 0)).rgb;
                float qDepth = gNormal.Load(int3(ipos2, 0)).w;

                float3 qColor = gColorVariance.Load(int3(ipos2, 0)).rgb;
                float qVariance = gColorVariance.Load(int3(ipos2, 0)).a;
                float qLuminance = luma(qColor);

                // float w = calculateWeight(pDepth, qDepth, customSigmaZ * length(float2(offsetx, offsety)), pNormal, qNormal, pLuminance, qLuminance, customSigmaL);
                // float w = calculateWeight(pDepth, qDepth, step * sigmaZ * abs(dot(depthDerivative, float2(offsetx, offsety))), pNormal, qNormal, pLuminance, qLuminance, customSigmaL);
                float w = calculateWeightPosition(pPosition, qPosition, sigmaZ , pNormal, qNormal, pLuminance, qLuminance, customSigmaL);

                float weight = kernelWeights[abs(offsety)] * kernelWeights[abs(offsetx)] * w;

                c += weight * qColor;
                v += weight * weight * qVariance;
                weights += weight;
            }
        }
    }

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