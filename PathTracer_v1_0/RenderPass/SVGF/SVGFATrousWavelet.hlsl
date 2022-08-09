#include "SVGFCommon.hlsli"
Texture2D gColorVariance : register(t0);
Texture2D gNormal : register(t1);
Texture2D gPositionMeshID : register(t2);
Texture2D gDepthDerivative : register(t3);

SamplerState s1 : register(s0);

static const int support = 2;

//static const float h[25] = { 1.0 / 256.0, 1.0 / 64.0, 3.0 / 128.0, 1.0 / 64.0, 1.0 / 256.0,
//    1.0 / 64.0, 1.0 / 16.0, 3.0 / 32.0, 1.0 / 16.0, 1.0 / 64.0,
//    3.0 / 128.0, 3.0 / 32.0, 9.0 / 64.0, 3.0 / 32.0, 3.0 / 128.0,
//    1.0 / 64.0, 1.0 / 16.0, 3.0 / 32.0, 1.0 / 16.0, 1.0 / 64.0,
//    1.0 / 256.0, 1.0 / 64.0, 3.0 / 128.0, 1.0 / 64.0, 1.0 / 256.0 };

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
    float4 pPositionMeshId = gPositionMeshID.Sample(s1, input.texCoord);
    float3 pPosition = pPositionMeshId.rgb;
    float pMeshID = pPositionMeshId.a;

    float3 pNormal = gNormal.Sample(s1, input.texCoord).rgb;
    float pDepth = gNormal.Sample(s1, input.texCoord).w;

    float3 pColor = gColorVariance.Sample(s1, input.texCoord).rgb;
    float pVariance = gColorVariance.Sample(s1, input.texCoord).a;
    float pLuminance = luma(pColor);

    int step = stepSize;


    float var = 0.0;
    for (int y0 = -1; y0 <= 1; y0++) {
        for (int x0 = -1; x0 <= 1; x0++) {
            var += gaussKernel[x0 + 3 * y0 + 4] * gColorVariance.Sample(s1, input.texCoord + float2(x0, y0) * texelSize).a;
        }
    }

    float2 depthDerivative = gDepthDerivative.Sample(s1, input.texCoord).rg;
    float fwidthZ = max(abs(depthDerivative.x), abs(depthDerivative.y));

    float customSigmaL = (sigmaL * sqrt(var) + epsilon);
    float customSigmaZ = sigmaZ * step * max(fwidthZ, 1e-8);

    float3 c = pColor;
    float v = pVariance;
    float weights = 1.0f;

    int2 ipos = input.pos.xy;

    for (int offsetx = -support; offsetx <= support; offsetx++) {
        for (int offsety = -support; offsety <= support; offsety++) {
            int2 pos = ipos + int2(offsetx * step, offsety * step);
            float2 loc = float2(pos.x * texelSize.x, pos.y * texelSize.y);
            //float2 loc = input.texCoord + texelSize * float2(offsetx, offsety) * step;
            const bool outside = (loc.x < 0) || (loc.x > 1) || (loc.y < 0) || (loc.y > 1);

            
            if (!outside && (offsetx != 0 || offsety != 0)) {
                float4 qPositionMeshId = gPositionMeshID.Sample(s1, loc);
                float qMeshID = qPositionMeshId.a;

                float3 qPosition = qPositionMeshId.rgb;
                float3 qNormal = gNormal.Sample(s1, loc).rgb;
                float qDepth = gNormal.Sample(s1, loc).w;

                float3 qColor = gColorVariance.Sample(s1, loc).rgb;
                float qVariance = gColorVariance.Sample(s1, loc).a;
                float qLuminance = luma(qColor);

                // float w = calculateWeight(pDepth, qDepth, customSigmaZ * length(float2(offsetx, offsety)), pNormal, qNormal, pLuminance, qLuminance, customSigmaL);
                // float w = calculateWeight(pDepth, qDepth, step * sigmaZ * abs(dot(depthDerivative, float2(offsetx, offsety))), pNormal, qNormal, pLuminance, qLuminance, customSigmaL);
                float w = calculateWeightPosition(pPosition, qPosition, sigmaZ , pNormal, qNormal, pLuminance, qLuminance, customSigmaL);
                // w = 1.0f;
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
        cvnext = gColorVariance.Sample(s1, input.texCoord);
    }
    return cvnext;
}