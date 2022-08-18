#include "../Core/Common/CommonStructs.hlsli"
#include "../Core/BSDF/BSDFLobes.hlsli"

Texture2D gPositionMeshIDPrev : register(t0);
Texture2D gNormalDepthPrev : register(t1);
Texture2D gPositionMeshID : register(t2);
Texture2D gNormalDepth : register(t3);
Texture2D gDeltaReflectionPositionMeshID : register(t4);
Texture2D gDeltaReflectionPositionMeshIDPrev : register(t5);
Texture2D<uint> gPathType : register(t6);


SamplerState s1 : register(s0);


struct VS_OUTPUT
{
    float4 pos: SV_POSITION;
    float2 texCoord: TEXCOORD;
};

struct PS_OUT
{
    float2 motionVector: SV_Target0;
    float historyLength : SV_Target1;
};

cbuffer ConstantBuffer : register(b0)
{
    PerFrameData g_frameData;
};


float2 main(VS_OUTPUT input) : SV_TARGET
{
    const int2 ipos = int2(input.pos.xy);

    uint pathType = gPathType.Load(int3(ipos, 0)).r;

    if (!(pathType & BSDF_LOBE_DELTA_REFLECTION)) {
        return float2(0, 0);
    }

    float3 normal = gNormalDepth.Load(int3(ipos, 0)).rgb;
    float depth = gNormalDepth.Load(int3(ipos, 0)).w;
    float meshID = gPositionMeshID.Load(int3(ipos, 0)).w;

    float3 position = gPositionMeshID.Load(int3(ipos, 0)).rgb;
    float3 reflectedPosition = gDeltaReflectionPositionMeshID.Load(int3(ipos, 0)).rgb;

    float3 prevCameraPos = g_frameData.previousCameraPosition;

    float3 v = prevCameraPos - position;
    float3 vN = dot(v, normal) * normal;
    float3 vNPerpen = v - vN;
    float3 prevCameraPosReflected = position + vNPerpen - vN;

    float3 q1 = prevCameraPosReflected;
    float3 q2 = reflectedPosition;
    float3 p = position;

    float k = dot((p - q1), normal) / dot((q2 - q1), normal);
    float3 p2 = q1 + k * (q2 - q1);
    if (g_frameData.renderMode == 1) {
        p2 = position;
    }

    // Reprojection
    float4 projCoord = mul(float4(p2, 1.0f), g_frameData.previousProjView);
    projCoord /= projCoord.w;
    float2 prevPixel = float2(projCoord.x, -projCoord.y);
    prevPixel = (prevPixel + 1) * 0.5;

    return prevPixel;
}