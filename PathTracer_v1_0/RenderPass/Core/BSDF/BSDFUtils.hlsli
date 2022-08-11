#ifndef BSDF_UTILS_HLSLI
#define BSDF_UTILS_HLSLI
#include "../Common/MathUtils.hlsli"

static const float DiracAcceptanceThreshold = 1e-3;

bool checkReflectionConstraint(in float3 wi, in float3 wo)
{
    return false;
    // return abs(wi.z * wo.z - wi.x * wo.x - wi.y * wo.y - 1.0f) < DiracAcceptanceThreshold;
}

bool checkRefractionConstraint(in float3 wi, in float3 wo, float eta, float cosThetaT)
{
    return false;
    //float dotP = -wi.x * wo.x * eta - wi.y * wo.y * eta - copySign(cosThetaT, wi.z) * wo.z;
    //return (dotP - 1.0f) < DiracAcceptanceThreshold;
}
#endif
