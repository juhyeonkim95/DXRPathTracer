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


void PathTrace(in RayDesc ray, inout uint seed, inout PathTraceResult pathResult)
{
    float emissionWeight = 1.0f;
    float3 throughput = float3(1, 1, 1);
    float3 result = float3(0, 0, 0);

    RayPayload payload;
    payload.emission = float3(0, 0, 0);
    payload.attenuation = float3(1, 1, 1);
    payload.done = 0;
    payload.seed = seed;
    payload.direction = ray.Direction;
    payload.normal = float3(0, 0, 0);

    // ---------------------- First intersection ----------------------
    TraceRay(gRtScene, 0 /*rayFlags*/, 0xFF, 0 /* ray index*/, 2, 0, ray, payload);
    pathResult.normal = payload.normal;
    pathResult.depth = payload.t;
    pathResult.instanceIndex = payload.instanceIndex;
    pathResult.position = payload.origin;
    pathResult.direct = float3(0, 0, 0);
    pathResult.reflectance = payload.attenuation;


    // initial candidate generation
    for (int i = 0; i < min(gLightsCount, 32); i++)
    {
        uint lightIndex = (uint) (nextRand(payload.seed) * g_frameData.lightNumber);
        
        LightParameter light = lights[lightIndex];
        SampleLight(payload.origin, light, payload.seed, lightSample);

        const float Ldist = length(lightSample.position - payload.origin);
        const float3 L = normalize(lightSample.position - payload.origin);
        const float nDl = dot(payload.normal, L);
        const float LnDl = -dot(lightSample.normal, L);

        float lightPdf = Ldist * Ldist / LnDl * lightSample.pdf / g_frameData.lightNumber;

        const float scatterPdf = bsdf::Pdf(material, payload, wo);
        const float3 f = bsdf::Eval(material, payload, wo);

        float p_hat = length(f * lightSample.Li / (Ldist * Ldist) );
    }

    uint materialType = g_materialinfo[payload.materialIndex].materialType;
    uint maxDepth = PATHTRACE_MAX_DEPTH;
    switch (materialType) {
    case BSDF_TYPE_DIFFUSE:             maxDepth = PATHTRACE_MAX_DEPTH_DIFFUSE; break;
    case BSDF_TYPE_CONDUCTOR:           maxDepth = PATHTRACE_MAX_DEPTH_SPECULAR; break;
    case BSDF_TYPE_ROUGH_CONDUCTOR:     maxDepth = PATHTRACE_MAX_DEPTH_SPECULAR; break;
    case BSDF_TYPE_DIELECTRIC:          maxDepth = PATHTRACE_MAX_DEPTH_TRANSMITTANCE; break;
    case BSDF_TYPE_ROUGH_DIELECTRIC:    maxDepth = PATHTRACE_MAX_DEPTH_TRANSMITTANCE; break;
    case BSDF_TYPE_PLASTIC:             maxDepth = PATHTRACE_MAX_DEPTH_DIFFUSE; break;
    }

    if (materialType != BSDF_TYPE_DIFFUSE) {
        pathResult.reflectance = float3(1, 1, 1);
    }

#if USE_NEXT_EVENT_ESTIMATION
    LightSample lightSample;
    RayDesc shadowRay;
    shadowRay.TMin = SCENE_T_MIN;
#endif
    uint depth = 1;
    while (true) {
        // ---------------- Intersection with emitters ----------------
        result += emissionWeight * throughput * payload.emission;

        // ---------------- Terminate ray tracing ----------------
        // (1) over max depth
        // (2) ray missed
        // (3) hit emission
        if (payload.done || depth >= maxDepth) {
            if (depth <= 2) {
                pathResult.direct += emissionWeight * throughput * payload.emission;
            }
            break;
        }

        // (4) Russian roulette termination
#if USE_RUSSIAN_ROULETTE
        if (depth >= PATHTRACE_RR_BEGIN_DEPTH) {
            float pcont = max(max(throughput.x, throughput.y), throughput.z);
            pcont = max(pcont, 0.05f);  // Russian roulette minimum.
            if (nextRand(payload.seed) >= pcont)
                break;
            throughput /= pcont;
        }
#endif
        Material material = g_materialinfo[payload.materialIndex];

#if USE_NEXT_EVENT_ESTIMATION
        // --------------------- Emitter sampling ---------------------

        // (1) Select light index (currently uniform sampling) & sample light point within selected light.
        uint lightIndex = (uint) (nextRand(payload.seed) * g_frameData.lightNumber);

        LightParameter light = lights[lightIndex];
        SampleLight(payload.origin, light, payload.seed, lightSample);

        const float Ldist = length(lightSample.position - payload.origin);
        const float3 L = normalize(lightSample.position - payload.origin);
        const float nDl = dot(payload.normal, L);
        const float LnDl = -dot(lightSample.normal, L);

        float lightPdf = Ldist * Ldist / LnDl * lightSample.pdf / g_frameData.lightNumber;

        if (LnDl > 0 && nDl > 0 && (material.materialType & BSDF_TYPE_GLOSSY))
        {
            shadowRay.Origin = payload.origin;
            shadowRay.Direction = L;
            shadowRay.TMax = Ldist - SCENE_T_MIN;
            ShadowPayload shadowPayload;
            shadowPayload.hit = false;
            TraceRay(gRtScene, 0  /*rayFlags*/, 0xFF, 1 /* ray index*/, 0, 1, shadowRay, shadowPayload);
            if (!shadowPayload.hit) {
                float3 wo = ToLocal(payload.tangent, payload.bitangent, payload.normal, L);
                const float scatterPdf = bsdf::Pdf(material, payload, wo);
                const float3 f = bsdf::Eval(material, payload, wo);

                const float weight = powerHeuristic(lightPdf, scatterPdf);

                const float3 L = weight * lightSample.Li * f / lightPdf * throughput;
                result += L;
                if (depth == 1) {
                    pathResult.direct += L;
                }
            }
        }
#endif
        throughput *= payload.attenuation;

        if (dot(throughput, throughput) == 0.0) {
            break;
        }

        ray.Direction = payload.direction;
        ray.Origin = payload.origin;
        float scatterPdf = payload.scatterPdf;
        depth += 1;// material.materialType& BSDF_TRANSMISSION ? 0.5f : 1.0f;

        TraceRay(gRtScene, 0 /*rayFlags*/, 0xFF, 0 /* ray index*/, 2, 0, ray, payload);
#if USE_NEXT_EVENT_ESTIMATION
        if (payload.lightIndex >= 0 && scatterPdf > 0.0f) {
            float lightPdfArea = lights[payload.lightIndex].normalAndPdf.w / g_frameData.lightNumber;
            float lightPdf = (payload.t * payload.t) / dot(payload.normal, -ray.Direction) * lightPdfArea;
            emissionWeight = powerHeuristic(scatterPdf, lightPdf);
        }
#endif
    }
    pathResult.radiance = result;

    return;
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
    float3 direct = float3(0, 0, 0);
    float3 normal = float3(0, 0, 0);
    float3 position = float3(0, 0, 0);
    float3 reflectance = float3(0, 0, 0);

    float depth = 0.0f;
    RayDesc ray;
    ray.TMin = SCENE_T_MIN;
    ray.TMax = SCENE_T_MAX;
    PathTraceResult pathResult;

    for (uint i = 0; i < PATHTRACE_SPP; i++)
    {
        uint seed = initRand(launchIndex.x + launchIndex.y * launchDim.x, g_frameData.totalFrameNumber * PATHTRACE_SPP + i, 16);

        float2 jitter = 0.5f;// float2(nextRand(seed), nextRand(seed));

        float2 d = pixel + jitter * 1.f / dims * 2.f;

        ray.Origin = g_frameData.cameraPosition.xyz;
        ray.Direction = normalize(g_frameData.u.xyz * d.x - g_frameData.v.xyz * d.y + g_frameData.w.xyz);
        PathTrace(ray, seed, pathResult);
        radiance += pathResult.radiance;
        normal += pathResult.normal;
        depth += pathResult.depth;
        position += pathResult.position;
        direct += pathResult.direct;
        reflectance += pathResult.reflectance;
    }
    radiance /= PATHTRACE_SPP;
    normal /= PATHTRACE_SPP;
    depth /= PATHTRACE_SPP;
    position /= PATHTRACE_SPP;
    direct /= PATHTRACE_SPP;
    reflectance /= PATHTRACE_SPP;

    float3 indirect = radiance - direct;
    uint currentMeshID = pathResult.instanceIndex;

#if DO_FILTERING
    gOutputPositionGeomID[launchIndex.xy] = float4(position, currentMeshID);
    gOutputNormal[launchIndex.xy] = float4(normal, depth);

    float3 directIllumination = direct / max(reflectance, float3(0.001, 0.001, 0.001));
    float3 indirectIllumination = indirect / max(reflectance, float3(0.001, 0.001, 0.001));

    gDirectIllumination[launchIndex.xy] = float4(directIllumination, 1.0f);
    gIndirectIllumination[launchIndex.xy] = float4(indirectIllumination, 1.0f);
    gReflectance[launchIndex.xy] = float4(reflectance, 1.0f);

#endif
    if (g_frameData.frameNumber > 1) {
        float3 oldColor = gOutputHDR[launchIndex.xy].xyz;
        float a = 1.0f / (float)(g_frameData.frameNumber);
        radiance = lerp(oldColor, radiance, a);
    }
    gOutputHDR[launchIndex.xy] = float4(radiance, 1.0f);
    radiance = radiance / (radiance + 1);
    radiance = pow(radiance, 1.f / 2.2f);
    gOutput[launchIndex.xy] = float4(radiance, 1.0f);

    //uint geomIDSeed = pathResult.instanceIndex;
    //float r = nextRand(geomIDSeed);
    //float g = nextRand(geomIDSeed);
    //float b = nextRand(geomIDSeed);
    //gOutput[launchIndex.xy] = float4(r,g,b, 1.0f);
    //float zvalue = 1 / (depth + 1);
    //gOutput[launchIndex.xy] = float4(zvalue, zvalue, zvalue, 1.0f);
}

[shader("miss")]
void missEnv(inout RayPayload payload)
{
    float3 direction = payload.direction;
    float phi = atan2(-direction.x, direction.z);
    float theta = acos(-direction.y);
    float u = (phi + M_PIf) * (0.5f * M_1_PIf);
    float v = theta * M_1_PIf;
    payload.emission = g_envtexture.SampleLevel(g_s0, float2(u, 1 - v), 0.0f).xyz;
    payload.done = true;
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
    normal = mul(normal, (float3x3)ObjectToWorld3x4());
    normal = normalize(normal);

    float nDotV = dot(normal, -rayDir);
    if (materialInfo.isDoubleSided && !isEmitter) {
        normal = nDotV < 0 ? (-normal) : (normal);
    }
    nDotV = dot(normal, -rayDir);

    float3 hitpoint = rayOrg + rayDir * t;

    payload.normal = normal;
    payload.lightIndex = geometryInfo.lightIndex;
    payload.materialIndex = geometryInfo.materialIndex;
    payload.instanceIndex = InstanceID();
    payload.t = t;
    payload.origin = hitpoint;

    if (isEmitter) {
        payload.done = true;
        payload.emission = nDotV > 0 ? lights[geometryInfo.lightIndex].emission.xyz : float3(0, 0, 0);
        return;
    }
    //if((nDotV <= 0) && (materialInfo.materialType != BSDF_TYPE_DIELECTRIC) ){
    //    payload.done = true;
    //    payload.emission = float3(0, 0, 0);
    //    return;
    //}

    float2 uvtemp = vtx0.uv * barycentrics.x + vtx1.uv * barycentrics.y + vtx2.uv * barycentrics.z;
    payload.uv = float2(uvtemp.x, 1 - uvtemp.y);

    //uint diffuseReflectanceTextureID = materialInfo.diffuseReflectanceTextureID;
    //float3 diffuseReflectance = diffuseReflectanceTextureID ? g_textures.SampleLevel(g_s0, payload.uv, 0.0f).xyz : materialInfo.diffuseReflectance;



    payload.bitangent = getBinormal(normal);
    payload.tangent = cross(payload.bitangent, normal);
    float3 wi = -rayDir;
    payload.wi = float3(dot(payload.tangent, wi), dot(payload.bitangent, wi), dot(payload.normal, wi));

    BSDFSample bs;

    bsdf::Sample(materialInfo, payload, payload.seed, bs);

    payload.direction = bs.wo.x * payload.tangent + bs.wo.y * payload.bitangent + bs.wo.z * payload.normal;
    payload.attenuation = bs.weight;
    payload.scatterPdf = bs.pdf;
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