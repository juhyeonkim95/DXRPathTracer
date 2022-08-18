#include "RELAXSingleCommon.hlsli"

Texture2D gColorHistory[4] :            register(t0);
Texture2D gCurrentColor[4] :            register(t4);
Texture2D gMomentsAndLengthHistory[4] :  register(t8);

Texture2D<float2> gMotionVector[3] :    register(t12);

Texture2D gPositionMeshID[3] :          register(t15);
Texture2D gPositionMeshIDPrev[3] :      register(t18);
Texture2D gNormalDepth[3] :             register(t21);
Texture2D gNormalDepthPrev[3] :         register(t24);
Texture2D<uint> gPathType :             register(t27);
Texture2D<float> gRoughness :           register(t28);


SamplerState s1 : register(s0);



struct VS_OUTPUT
{
    float4 pos:     SV_POSITION;
    float2 texCoord: TEXCOORD;
};

struct PS_OUT
{
    float4 color1: SV_Target0;
    float4 momentAndHistory1: SV_Target1;
    float4 color2: SV_Target2;
    float4 momentAndHistory2: SV_Target3;
    float4 color3: SV_Target4;
    float4 momentAndHistory3: SV_Target5;
    float4 color4: SV_Target6;
    float4 momentAndHistory4: SV_Target7;
};

bool isReprojectionTapValid(in float3 currentWorldPos, in float3 previousWorldPos, in float3 currentNormal, float disocclusionThreshold)
{
    // Check if plane distance is acceptable
    float3 posDiff = currentWorldPos - previousWorldPos;
    float maxPlaneDistance = abs(dot(posDiff, currentNormal));

    return maxPlaneDistance < disocclusionThreshold;
}

bool checkConsistency(int j, in int2 ipos, in float2 prevPixel, bool checkRoughness)
{
    float3 position = gPositionMeshID[j].Load(int3(ipos, 0)).rgb;
    float meshID = gPositionMeshID[j].Load(int3(ipos, 0)).w;
    float3 normal = gNormalDepth[j].Load(int3(ipos, 0)).rgb;
    float depth = gNormalDepth[j].Load(int3(ipos, 0)).w;

    float3 previousPosition = gPositionMeshIDPrev[j].Sample(s1, prevPixel).rgb;
    float previousMeshID = gPositionMeshIDPrev[j].Sample(s1, prevPixel).w;
    float3 previousNormal = gNormalDepthPrev[j].Sample(s1, prevPixel).rgb;
    float previousDepth = gNormalDepthPrev[j].Sample(s1, prevPixel).w;

    bool consistency = true;

    // (1) Check geometry consistency
    float disocclusionThreshold = gDisocclusionDepthThreshold * depth;
    // consistency = consistency && (depth == 0 || isReprojectionTapValid(position, previousPosition, normal, disocclusionThreshold));

    consistency = consistency && (length(position - previousPosition) < gPositionThreshold);

    // (2) check normal
    consistency = consistency && (dot(normal, previousNormal) > gNormalDepthThreshold);

    // (3) check material
    consistency = consistency && (meshID == previousMeshID);

    // (4) check outside
    bool outside = (prevPixel.x < 0) || (prevPixel.x > 1) || (prevPixel.y < 0) || (prevPixel.y > 1);
    consistency = consistency && !outside;

    if (checkRoughness) 
    {
        float roughness = gRoughness.Load(int3(ipos, 0)).r;
        // Specular Lobe
        float3 viewDir = normalize(g_frameData.cameraPosition - position);
        float3 prevViewDir = normalize(g_frameData.previousCameraPosition - position);
        float cosFactor = pow(dot(viewDir, prevViewDir), 1 / (roughness + 0.01));
        consistency = consistency && (cosFactor > 0.8);
    }
    
    // (5) check param changed
    consistency = consistency && !g_frameData.paramChanged;

    // (6) if camera not moved, consistent
    consistency = consistency || !g_frameData.cameraChanged;

    return consistency;
}

void temporalAccumulation(int i, int j, in uint pathType, in int2 ipos, out float4 outColor, out float4 outMomentAndHistory, bool checkRoughness)
{
    float2 prevUV = gMotionVector[j].Load(int3(ipos, 0)).rg;

    bool consistency = checkConsistency(j, ipos, prevUV, checkRoughness);

    float3 color = gCurrentColor[i].Load(int3(ipos, 0)).rgb;
    float3 prevColor = gColorHistory[i].Sample(s1, prevUV).rgb;
    float2 prevMoment = gMomentsAndLengthHistory[i].Sample(s1, prevUV).rg;
    float prevHistoryLength = gMomentsAndLengthHistory[i].Sample(s1, prevUV).a;

    float historyLength = min(255.0f, (consistency ? (prevHistoryLength + 1.0f) : 1.0f));

    float alpha = consistency ? max(1.0 / gMaxAccumulatedFrame, 1.0 / historyLength) : 1.0f;
    float alphaMoments = consistency ? max(1.0 / gMaxAccumulatedFrame, 1.0 / historyLength) : 1.0f;

    // compute first two moments of luminance
    float luminance = luma(color);
    float2 moments = float2(luminance, luminance * luminance);

    // temporal integration of the moments
    moments = lerp(prevMoment, moments, alphaMoments);
    float variance = max(0.f, moments.g - moments.r * moments.r);
    outColor = float4(lerp(prevColor, color, alpha), variance);
    outMomentAndHistory = float4(moments.x, moments.y, 0, historyLength);
}

PS_OUT main(VS_OUTPUT input) : SV_Target
{
    PS_OUT output;
    const int2 ipos = int2(input.pos.xy);
    uint pathType = gPathType.Load(int3(ipos, 0)).r;

    if ((pathType & BSDF_LOBE_DIFFUSE_REFLECTION)) {
        temporalAccumulation(0, 0, BSDF_LOBE_DIFFUSE_REFLECTION, ipos, output.color1, output.momentAndHistory1, false);
    }
    else {
        output.color1 = 0.0f;
        output.momentAndHistory1 = 0.0f;
    }

    if ((pathType & BSDF_LOBE_GLOSSY_REFLECTION)) {
        temporalAccumulation(1, 0, BSDF_LOBE_GLOSSY_REFLECTION, ipos, output.color2, output.momentAndHistory2, true);
    }
    else {
        output.color2 = 0.0f;
        output.momentAndHistory2 = 0.0f;
    }

    if ((pathType & BSDF_LOBE_DELTA_REFLECTION)) {
        temporalAccumulation(2, 1, BSDF_LOBE_DELTA_REFLECTION, ipos, output.color3, output.momentAndHistory3, false);
    }
    else {
        output.color3 = 0.0f;
        output.momentAndHistory3 = 0.0f;
    }

    if ((pathType & BSDF_LOBE_DELTA_TRANSMISSION)) {
        temporalAccumulation(3, 2, BSDF_LOBE_DELTA_TRANSMISSION, ipos, output.color4, output.momentAndHistory4, false);
    }
    else {
        output.color4 = 0.0f;
        output.momentAndHistory4 = 0.0f;
    }

    //if ((pathType & BSDF_LOBE_GLOSSY_REFLECTION)) {
    //    output.color2 = float4(1, 0, 0, 0);
    //    output.momentAndHistory2 = float4(1, 0, 0, 0);
    //    // temporalAccumulation(1, 0, BSDF_LOBE_GLOSSY_REFLECTION, ipos, output.color2, output.momentAndHistory2);
    //}
    //if ((pathType & BSDF_LOBE_DELTA_REFLECTION)) {
    //    temporalAccumulation(2, 1, BSDF_LOBE_DELTA_REFLECTION, ipos, output.color3, output.momentAndHistory3);
    //}
    //if ((pathType & BSDF_LOBE_DELTA_TRANSMISSION)) {
    //    temporalAccumulation(3, 2, BSDF_LOBE_DELTA_TRANSMISSION, ipos, output.color4, output.momentAndHistory4);
    //}

    return output;
}