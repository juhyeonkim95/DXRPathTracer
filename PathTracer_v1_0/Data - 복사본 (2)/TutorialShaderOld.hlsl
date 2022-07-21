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
#include "ShaderStructs.hlsli"

float3 PathTrace(in RayDesc ray, inout uint seed)
{
    float3 result = float3(0, 0, 0);

    RayPayload payload;
    payload.radiance = float3(0, 0, 0);
    payload.attenuation = float3(1, 1, 1);
    payload.done = 0;
    payload.seed = seed;
    payload.depth = 0;
    payload.result = float3(0, 0, 0);

    for (int depth = 0; depth < PATHTRACE_MAX_DEPTH; depth++)
    {
        TraceRay(gRtScene, 0 /*rayFlags*/, 0xFF, 0 /* ray index*/, 2, 0, ray, payload);

        if (payload.done) {
            break;
        }

        ray.Origin = payload.origin;
        ray.Direction = payload.direction;
        payload.depth++;
    }

    result = payload.result;
    return result;
}

[shader("raygeneration")]
void rayGen()
{
    uint3 launchIndex = DispatchRaysIndex();
    uint3 launchDim = DispatchRaysDimensions();

    float2 crd = float2(launchIndex.xy);
    float2 dims = float2(launchDim.xy);

    float2 pixel = ((crd/dims) * 2.f - 1.f);
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

        ray.TMin = 1e-3;
        ray.TMax = 100000;

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
    payload.radiance = float3(0, 0, 0);
    payload.done = true;
}


[shader("closesthit")]
void chs(inout RayPayload payload, in BuiltInTriangleIntersectionAttributes attribs)
{
    float3 barycentrics = float3(1.0 - attribs.barycentrics.x - attribs.barycentrics.y, attribs.barycentrics.x, attribs.barycentrics.y);

    GeometryInfo geometryInfo = g_geometryinfo[InstanceID()];
    Material materialInfo = g_materialinfo[geometryInfo.materialIndex];
    bool isEmitter = geometryInfo.lightIndex >= 0;
    LightParameter lightParameter = lights[max(geometryInfo.lightIndex, 0)];

    float3 emission = lightParameter.emission.xyz;

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
    
    if (isEmitter) {
        if (nDotV < 0) {
            payload.done = true;
            return;
        }
        if (payload.depth == 0 || geometryInfo.lightIndex != 0) {
            payload.result += emission * payload.attenuation;
        }
        else {
#if USE_NEXT_EVENT_ESTIMATION
            // payload.result += payload.attenuation * emission;
            float3 u = lights[0].u.xyz;
            float3 v = lights[0].v.xyz;
            float lightPdfArea = 1 / length(cross(u, v));
            float lightPdf = (t * t) / dot(normal, -rayDir) * lightPdfArea;
            float weight = powerHeuristic(payload.scatterPdf, lightPdf);
            payload.result += weight * emission * payload.attenuation;
#else 
            payload.result += emission * payload.attenuation;
#endif
        }
        payload.done = true;
        return;
    }
    


    float2 uv = vtx0.uv * barycentrics.x + vtx1.uv * barycentrics.y + vtx2.uv * barycentrics.z;
    uint diffuseReflectanceTextureID = materialInfo.diffuseReflectanceTextureID;
    float3 albedo = diffuseReflectanceTextureID ? g_textures.SampleLevel(g_s0, uv, 0.0f).xyz : materialInfo.diffuseReflectance;

    float3 hitpoint = rayOrg + rayDir * t;
    
#if USE_NEXT_EVENT_ESTIMATION
    uint numLight = g_frameData.lightNumber;
    uint lightIndex = (uint) ((numLight) * nextRand(payload.seed));

    float r1 = nextRand(payload.seed);
    float r2 = nextRand(payload.seed);

    float3 p = lights[0].position.xyz;
    float3 u = lights[0].u.xyz;
    float3 v = lights[0].v.xyz;
    float3 Li = lights[0].emission.xyz;


    LightSample lightSample;
    lightSample.position = p + u * r1 + v * r2;
    lightSample.normal = normalize(cross(u, v));
    lightSample.pdf = 1 / length(cross(u, v));

    const float Ldist = length(lightSample.position - hitpoint);
    const float3 L = normalize(lightSample.position - hitpoint);
    const float nDl = dot(normal, L);
    const float LnDl = dot(-lightSample.normal, L);

    const float A = length(cross(u, v));
    const float lightPdfArea = 1 / A;
    float lightPdf = Ldist * Ldist / LnDl * lightPdfArea;


    if (nDl > 0 && LnDl > 0) 
    {
        RayDesc ray;
        ray.Origin = hitpoint;
        ray.Direction = L;
        ray.TMin = 0.001;
        ray.TMax = Ldist - 0.001;
        ShadowPayload shadowPayload;
        shadowPayload.hit = false;
        TraceRay(gRtScene, 0  /*rayFlags*/, 0xFF, 1 /* ray index*/, 0, 1, ray, shadowPayload);
        if (!shadowPayload.hit) {
            const float scatterPdf = nDl / M_PIf;
            const float3 f = albedo * nDl / M_PIf;

            const float weight = powerHeuristic(lightPdf, scatterPdf);
            
            payload.result += weight * Li * f / lightPdf * payload.attenuation;
        }
    }
#endif
    payload.origin = hitpoint;
    payload.direction = getCosHemisphereSample(payload.seed, normal);
    payload.attenuation *= albedo;
    payload.scatterPdf = dot(normal, payload.direction) / M_PIf;
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