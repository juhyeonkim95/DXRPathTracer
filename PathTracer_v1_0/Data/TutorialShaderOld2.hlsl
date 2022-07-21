/***************************************************************************
# Copyright (c) 2018, NVIDIA CORPORATION. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#  * Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
#  * Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#  * Neither the name of NVIDIA CORPORATION nor the names of its
#    contributors may be used to endorse or promote products derived
#    from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
# PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
# OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
***************************************************************************/
// Utility function to get a vector perpendicular to an input vector 
//    (from "Efficient Construction of Perpendicular Vectors Without Branching")
#include "ShaderUtils.hlsli"
#include "BSDF.hlsli"
#include "SampleLight.hlsli"

float3 PathTrace(in RayDesc ray, inout uint seed)
{
    float emissionWeight = 1.0f;
    float3 throughput = float3(1, 1, 1);
    float3 result = float3(0, 0, 0);

    RayPayload payload;
    payload.emission = float3(0, 0, 0);
    payload.attenuation = float3(1, 1, 1);
    payload.done = 0;
    payload.seed = seed;

    TraceRay(gRtScene, 0 /*rayFlags*/, 0xFF, 0 /* ray index*/, 2, 0, ray, payload);
#if USE_NEXT_EVENT_ESTIMATION
    LightSample lightSample;
    RayDesc shadowRay;
    shadowRay.TMin = SCENE_T_MIN;
#endif
    for (int depth = 1; ; depth++) {
        result += emissionWeight * throughput * payload.emission;
        if (payload.done || depth >= PATHTRACE_MAX_DEPTH) {
            break;
        }
        Material material = g_materialinfo[payload.materialIndex];

#if USE_NEXT_EVENT_ESTIMATION
        
        LightParameter light = lights[0];
        SampleLight(payload.origin, light, payload.seed, lightSample);

        const float Ldist = length(lightSample.position - payload.origin);
        const float3 L = normalize(lightSample.position - payload.origin);
        const float nDl = dot(payload.normal, L);
        const float LnDl = -dot(lightSample.normal, L);

        float lightPdf = Ldist * Ldist / LnDl * lightSample.pdf;

        if (nDl > 0 && LnDl > 0)
        {
            shadowRay.Origin = payload.origin;
            shadowRay.Direction = L;
            shadowRay.TMax = Ldist - SCENE_T_MIN;
            ShadowPayload shadowPayload;
            shadowPayload.hit = false;
            TraceRay(gRtScene, 0  /*rayFlags*/, 0xFF, 1 /* ray index*/, 0, 1, shadowRay, shadowPayload);
            if (!shadowPayload.hit) {
                const float scatterPdf = bsdf::Pdf(material, payload, L); //nDl / PI;
                const float3 f = bsdf::Eval(material, payload, L); //payload.attenuation * nDl / PI;

                const float weight = powerHeuristic(lightPdf, scatterPdf);

                result += weight * lightSample.Li * f / lightPdf * throughput;
            }
        }
#endif
        throughput *= payload.attenuation;

        ray.Direction = payload.direction;
        ray.Origin = payload.origin;
        float scatterPdf = payload.scatterPdf;

        TraceRay(gRtScene, 0 /*rayFlags*/, 0xFF, 0 /* ray index*/, 2, 0, ray, payload);
#if USE_NEXT_EVENT_ESTIMATION
        if (payload.lightIndex == 0) {
            float3 u = lights[0].u.xyz;
            float3 v = lights[0].v.xyz;
            float lightPdfArea = 1 / length(cross(u, v));
            float lightPdf = (payload.t * payload.t) / dot(payload.normal, -ray.Direction) * lightPdfArea;
            emissionWeight = powerHeuristic(scatterPdf, lightPdf);
        }
#endif
    }

    return result;
}

[shader("raygeneration")]
void rayGen()
{
    uint3 launchIndex = DispatchRaysIndex();
    uint3 launchDim = DispatchRaysDimensions();

    float2 crd = float2(launchIndex.xy);
    float2 dims = float2(launchDim.xy);

    float2 pixel = ((crd / dims) * 2.f - 1.f);
    float aspectRatio = dims.x / dims.y;

    float3 radiance = float3(0, 0, 0);

    for (uint i = 0; i < PATHTRACE_SPP; i++)
    {
        uint seed = initRand(launchIndex.x + launchIndex.y * launchDim.x, g_frameData.frameNumber * PATHTRACE_SPP + i, 16);

        float2 jitter = float2(nextRand(seed), nextRand(seed));
        float2 d = pixel + jitter * 1.f / dims * 2.f;

        RayDesc ray;
        ray.Origin = g_frameData.cameraPosition.xyz;
        ray.Direction = normalize(g_frameData.u.xyz * d.x - g_frameData.v.xyz * d.y + g_frameData.w.xyz);

        ray.TMin = SCENE_T_MIN;
        ray.TMax = SCENE_T_MAX;

        radiance += PathTrace(ray, seed);
    }
    radiance /= PATHTRACE_SPP;

    if (g_frameData.frameNumber > 1) {
        float3 oldColor = gOutput[launchIndex.xy].xyz;
        oldColor = pow(oldColor, 2.2f);
        oldColor = oldColor / (1 - oldColor);

        float a = 1.0f / (float)(g_frameData.frameNumber);
        radiance = lerp(oldColor, radiance, a);
    }

    radiance = radiance / (radiance + 1);
    radiance = pow(radiance, 1.f / 2.2f);
    gOutput[launchIndex.xy] = float4(radiance, 1.0f);

}

[shader("miss")]
void miss(inout RayPayload payload)
{
    payload.emission = float3(0, 0, 0);
    payload.done = true;
}


[shader("closesthit")]
void chs(inout RayPayload payload, in BuiltInTriangleIntersectionAttributes attribs)
{
    float3 barycentrics = float3(1.0 - attribs.barycentrics.x - attribs.barycentrics.y, attribs.barycentrics.x, attribs.barycentrics.y);

    GeometryInfo geometryInfo = g_geometryinfo[InstanceID()];
    Material materialInfo = g_materialinfo[geometryInfo.materialIndex];
    bool isEmitter = geometryInfo.lightIndex >= 0;

    const float3 rayDir = WorldRayDirection();
    const float3 rayOrg = WorldRayOrigin();
    const float t = RayTCurrent();

    const uint triangleID = PrimitiveIndex();
    const uint idx0 = g_indices[triangleID * 3 + geometryInfo.indicesOffset + 0];
    const uint idx1 = g_indices[triangleID * 3 + geometryInfo.indicesOffset + 1];
    const uint idx2 = g_indices[triangleID * 3 + geometryInfo.indicesOffset + 2];

    const MeshVertex vtx0 = g_attributes[idx0 + geometryInfo.verticesOffset];
    const MeshVertex vtx1 = g_attributes[idx1 + geometryInfo.verticesOffset];
    const MeshVertex vtx2 = g_attributes[idx2 + geometryInfo.verticesOffset];


    float3 normal = vtx0.normal * barycentrics.x + vtx1.normal * barycentrics.y + vtx2.normal * barycentrics.z;
    normal = normalize(normal);

    float nDotV = dot(normal, -rayDir);
    if (materialInfo.isDoubleSided && !isEmitter) {
        normal = nDotV < 0 ? (-normal) : (normal);
    }

    payload.normal = normal;
    payload.lightIndex = geometryInfo.lightIndex;
    payload.materialIndex = geometryInfo.materialIndex;
    payload.t = t;

    if (isEmitter) {
        payload.done = true;
        payload.emission = nDotV>0?lights[geometryInfo.lightIndex].emission.xyz:float3(0,0,0);
        return;
    }

    float2 uvtemp = vtx0.uv * barycentrics.x + vtx1.uv * barycentrics.y + vtx2.uv * barycentrics.z;
    float2 uv = float2(uvtemp.x, 1-uvtemp.y);

    uint diffuseReflectanceTextureID = materialInfo.diffuseReflectanceTextureID;
    float3 diffuseReflectance = diffuseReflectanceTextureID ? g_textures.SampleLevel(g_s0, uv, 0.0f).xyz : materialInfo.diffuseReflectance;

    float3 hitpoint = rayOrg + rayDir * t;

    payload.origin = hitpoint;
    payload.direction = getCosHemisphereSample(payload.seed, normal);
    payload.scatterPdf = dot(normal, payload.direction) / PI;
    payload.attenuation = diffuseReflectance;
}


[shader("closesthit")]
void shadowChs(inout ShadowPayload payload, in BuiltInTriangleIntersectionAttributes attribs)
{
    payload.hit = true;
}

[shader("miss")]
void shadowMiss(inout ShadowPayload payload)
{
    payload.hit = false;
}