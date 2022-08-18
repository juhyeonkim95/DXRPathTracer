#include "RELAXSingleCommon.hlsli"
Texture2D gColorVariance[4] : register(t0);
Texture2D gPositionMeshID[3] : register(t4);
Texture2D gNormalDepth[3] : register(t7);

Texture2D gDepthDerivative : register(t10);
Texture2D<uint> gPathType : register(t11);
Texture2D<float> gRoughness : register(t12);

SamplerState s1 : register(s0);

static const int support = 2;

static const float kernelWeights[3] = { 1.0, 2.0 / 3.0, 1.0 / 6.0 };
static const float gaussKernel[9] = { 1.0 / 16.0, 1.0 / 8.0, 1.0 / 16.0, 1.0 / 8.0, 1.0 / 4.0, 1.0 / 8.0, 1.0 / 16.0, 1.0 / 8.0, 1.0 / 16.0 };

struct PS_OUT
{
    float4 color1: SV_Target0;
    float4 color2: SV_Target1;
    float4 color3: SV_Target2;
    float4 color4: SV_Target3;
};

bool eps_equal(float a, float b) {
    return round(a) == round(b);
}

struct VS_OUTPUT
{
    float4 pos: SV_POSITION;
    float2 texCoord: TEXCOORD;
};

float4 wavelet(int i, int j, in int2 ipos)
{
    // int2 ipos = int2(input.pos.xy);

    uint pPathType = gPathType.Load(int3(ipos, 0)).r;

    float4 pPositionMeshId = gPositionMeshID[j].Load(int3(ipos, 0));
    float3 pPosition = pPositionMeshId.rgb;
    float pMeshID = pPositionMeshId.a;

    float3 pNormal = gNormalDepth[j].Load(int3(ipos, 0)).rgb;
    float pDepth = gNormalDepth[j].Load(int3(ipos, 0)).w;

    float3 pColor = gColorVariance[i].Load(int3(ipos, 0)).rgb;
    float pVariance = gColorVariance[i].Load(int3(ipos, 0)).a;
    float pLuminance = luma(pColor);

    int step = stepSize;


    float var = 0.0;
    for (int y0 = -1; y0 <= 1; y0++) {
        for (int x0 = -1; x0 <= 1; x0++) {
            const int2 p = ipos + int2(x0, y0);
            var += gaussKernel[x0 + 3 * y0 + 4] * gColorVariance[i].Load(int3(p, 0)).a;
        }
    }

    float2 depthDerivative = gDepthDerivative.Load(int3(ipos, 0)).rg;
    float fwidthZ = max(abs(depthDerivative.x), abs(depthDerivative.y));

    float customSigmaL = (sigmaL * sqrt(var) + epsilon);
    float customSigmaZ = sigmaZ * step * max(fwidthZ, 1e-8);

    float3 c = pColor;
    float v = pVariance;
    float weights = 1.0f;

    //#ifdef RELAXSingle_SPECULAR
    //    float pRoughness = gRoughness.Load(int3(ipos, 0)).r;
    //    float3 pViewDir = normalize(g_frameData.cameraPosition - pPosition);
    //#endif

    for (int offsetx = -support; offsetx <= support; offsetx++) {
        for (int offsety = -support; offsety <= support; offsety++) {
            int2 ipos2 = ipos + int2(offsetx, offsety) * step;
            const bool outside = (ipos2.x < 0) || (ipos2.x >= screenSize.x) || (ipos2.y < 0) || (ipos2.y >= screenSize.y);

            uint qPathType = gPathType.Load(int3(ipos2, 0)).r;
            const bool pathTypeValid = (qPathType == pPathType);

            if (!outside && (offsetx != 0 || offsety != 0) && pathTypeValid) {
                float4 qPositionMeshId = gPositionMeshID[j].Load(int3(ipos2, 0));
                float qMeshID = qPositionMeshId.a;

                float3 qPosition = qPositionMeshId.rgb;
                float3 qNormal = gNormalDepth[j].Load(int3(ipos2, 0)).rgb;
                float qDepth = gNormalDepth[j].Load(int3(ipos2, 0)).w;

                float3 qColor = gColorVariance[i].Load(int3(ipos2, 0)).rgb;
                float qVariance = gColorVariance[i].Load(int3(ipos2, 0)).a;
                float qLuminance = luma(qColor);

                // float w = calculateWeight(pDepth, qDepth, customSigmaZ * length(float2(offsetx, offsety)), pNormal, qNormal, pLuminance, qLuminance, customSigmaL);
                // float w = calculateWeight(pDepth, qDepth, step * sigmaZ * abs(dot(depthDerivative, float2(offsetx, offsety))), pNormal, qNormal, pLuminance, qLuminance, customSigmaL);
                float w = calculateWeightPosition(pPosition, qPosition, sigmaZ, pNormal, qNormal, pLuminance, qLuminance, customSigmaL);

                //#ifdef RELAXSingle_SPECULAR
                //                float3 qViewDir = normalize(g_frameData.cameraPosition - qPosition);
                //                float cosFactor = pow(dot(pViewDir, qViewDir), 1 / (pRoughness + 0.01));
                //                w *= cosFactor;
                //#endif

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
        cvnext = gColorVariance[i].Load(int3(ipos, 0));
    }
    return cvnext;
}

PS_OUT main(VS_OUTPUT input) : SV_TARGET
{
    PS_OUT output;
    const int2 ipos = int2(input.pos.xy);
    uint pathType = gPathType.Load(int3(ipos, 0)).r;
    if ((pathType & BSDF_LOBE_DIFFUSE_REFLECTION)) {
        output.color1 = wavelet(0, 0, ipos);
    }
    else {
        output.color1 = 0.0f;
    }
    if ((pathType & BSDF_LOBE_GLOSSY_REFLECTION)) {
        output.color2 = wavelet(1, 0, ipos);
    }
    else {
        output.color2 = 0.0f;
    }
    if ((pathType & BSDF_LOBE_DELTA_REFLECTION)) {
        output.color3 = wavelet(2, 1, ipos);
    }
    else {
        output.color3 = 0.0f;
    }
    if ((pathType & BSDF_LOBE_DELTA_TRANSMISSION)) {
        output.color4 = wavelet(3, 2, ipos);
    }
    else {
        output.color4 = 0.0f;
    }

    return output;
}