#include "../Core/stdafx.hlsli"
#include "../Core/BSDF/BSDF.hlsli"
#include "../Core/Light/LightSampling.hlsli"
#include "../ReSTIR/ReSTIRInitTemporal.hlsli"
#include "PathTracerConstants.hlsli"

void PrimaryPath(in RayDesc ray, inout PathTraceResult pathResult, inout RayPayload payload)
{
    payload.emission = float3(0, 0, 0);
    payload.attenuation = float3(1, 1, 1);
    payload.done = 0;
    payload.primaryReflectionLobe = 0;
    payload.direction = ray.Direction;
    payload.normal = float3(0, 0, 0);
    payload.requestedLobe = BSDF_LOBE_ALL;

    // ---------------------- First intersection ----------------------
    TraceRay(gRtScene, 0 /*rayFlags*/, 0xFF, 0 /* ray index*/, 2, 0, ray, payload);

    Material material = g_materialinfo[payload.materialIndex];
    payload.depth = 1;

    pathResult.normal = payload.normal;
    pathResult.depth = payload.t;
    pathResult.instanceIndex = payload.instanceIndex;
    pathResult.position = payload.origin;

    // Reflectance
    pathResult.roughness = payload.roughness;
#if OUTPUT_SVGF_RELATED
    pathResult.reflectance = bsdf::getApproximatedReflectance(material, payload);
#endif
    pathResult.diffuseReflectance = bsdf::getApproximatedDiffuseReflectance(material, payload);
    pathResult.specularReflectance = bsdf::getApproximatedSpecularReflectance(material, payload);
    pathResult.deltaReflectionReflectance = 0.0f;
    pathResult.deltaTransmissionReflectance = 0.0f;

    pathResult.primaryPathType = bsdf::getReflectionLobe(material);

    pathResult.deltaReflectionPosition = 0;
    pathResult.deltaReflectionNormal = 0;
    pathResult.deltaReflectionMeshID = 0;
    pathResult.deltaTransmissionPosition = 0;
    pathResult.deltaTransmissionNormal = 0;
    pathResult.deltaTransmissionMeshID = 0;


    // Radiance
    pathResult.directRadiance = float3(0, 0, 0);
    pathResult.diffuseRadiance = float3(0, 0, 0);
    pathResult.specularRadiance = float3(0, 0, 0);
    pathResult.deltaReflectionRadiance = float3(0, 0, 0);
    pathResult.deltaTransmissionRadiance = float3(0, 0, 0);
    pathResult.residualRadiance = float3(0, 0, 0);

    pathResult.diffuseIllumination = float3(0, 0, 0);
    pathResult.specularIllumination = float3(0, 0, 0);

    // Emission
    pathResult.emission = payload.emission;
    pathResult.deltaReflectionEmission = 0.0f;
    pathResult.deltaTransmissionEmission = 0.0f;

    pathResult.indirectReflectance = payload.diffuseReflectance;

    uint materialType = material.materialType;
    switch (materialType) {
    case BSDF_TYPE_DIFFUSE:             pathResult.maxDepth = gPathTracer.maxDepthDiffuse; break;
    case BSDF_TYPE_CONDUCTOR:           pathResult.maxDepth = gPathTracer.maxDepthSpecular; break;
    case BSDF_TYPE_ROUGH_CONDUCTOR:     pathResult.maxDepth = gPathTracer.maxDepthSpecular; break;
    case BSDF_TYPE_DIELECTRIC:          pathResult.maxDepth = gPathTracer.maxDepthTransmittance; break;
    case BSDF_TYPE_ROUGH_DIELECTRIC:    pathResult.maxDepth = gPathTracer.maxDepthTransmittance; break;
    case BSDF_TYPE_PLASTIC:             pathResult.maxDepth = gPathTracer.maxDepthSpecular; break;
    case BSDF_TYPE_ROUGH_PLASTIC:       pathResult.maxDepth = gPathTracer.maxDepthDiffuse; break;
    default:                            pathResult.maxDepth = gPathTracer.maxDepth; break;
    }
}


void PathTrace(in RayDesc ray, inout uint seed, inout PathTraceResult pathResult, inout RayPayload payload, bool accumulateResult)
{
    float emissionWeight = 1.0f;
    float3 throughput = payload.attenuation;
    float3 result = float3(0, 0, 0);
    
    float3 resultExceptFirst = float3(0, 0, 0);
    float3 throughputExceptFirst = payload.attenuation;

    BSDFSample bs;

    uint depth = payload.depth;
    uint primaryReflectionLobe = payload.primaryReflectionLobe;
    bool useReSTIR = false;

    payload.requestedLobe = BSDF_LOBE_ALL;
    
    Material material = g_materialinfo[payload.materialIndex];
    uint maxDepth = pathResult.maxDepth;

    LightSample lightSample;
    RayDesc shadowRay;
    shadowRay.TMin = SCENE_T_MIN;
    bool deltaTransmissionPath = false;
    bool foundNonDelta = false;

    while (true)
    {
        // ---------------- Intersection with emitters ----------------
        float3 currentRadiance = emissionWeight * throughput * payload.emission;
        result += currentRadiance;
        resultExceptFirst += emissionWeight * throughputExceptFirst * payload.emission;

        material = g_materialinfo[payload.materialIndex];
        
        // ---------------- Terminate ray tracing ----------------
        // (1) over max depth
        // (2) ray missed
        // (3) hit emission
        if (payload.done || depth >= maxDepth) {
            if (depth == 1) {
                pathResult.emission = payload.emission;
            }
#if OUTPUT_SVGF_RELATED
            else if (depth == 2) {
                pathResult.directRadiance += currentRadiance;
            }
#endif
            break;
        }

        // (4) Russian roulette termination
#if USE_RUSSIAN_ROULETTE
        if (depth >= PATHTRACE_RR_BEGIN_DEPTH) {
            float pcont = max(max(throughput.x, throughput.y), throughput.z);
            pcont = max(pcont, 0.05f);  // Russian roulette minimum.
            if (nextRand(seed) >= pcont)
                break;
            throughput /= pcont;
        }
#endif


#if USE_NEXT_EVENT_ESTIMATION
        // ------------------------------------------------------------
        // --------------------- Emitter sampling ---------------------
        // ------------------------------------------------------------

        // Check whether use ReSTIR
        useReSTIR = gReSTIR.enableReSTIR && (depth == 1) && (material.materialType & BSDF_TYPE_GLOSSY);
        bool useNEE = (material.materialType & BSDF_TYPE_GLOSSY);
        float3 Li = 0.0f;
        float weight = 0.0f;
        float3 wo = 0.0f;

        // Emitter sampling Case 1
        // --------------------- ReSTIR ---------------------
        if (useReSTIR)
        {
            Reservoir newReservoir = getLightSampleReSTIR(payload, pathResult);
            if (newReservoir.W > 0)
            {
                float Ldist = length(newReservoir.lightSample.position - payload.origin);
                float3 L = normalize(newReservoir.lightSample.position - payload.origin);

                shadowRay.Origin = payload.origin;
                shadowRay.Direction = L;
                shadowRay.TMax = Ldist - SCENE_T_MIN;
                ShadowPayload shadowPayload;
                shadowPayload.hit = false;
                // TraceRay(gRtScene, 0  /*rayFlags*/, 0xFF, 1 /* ray index*/, 0, 1, shadowRay, shadowPayload);
                if (!shadowPayload.hit) {
                    wo = ToLocal(payload.tangent, payload.bitangent, payload.normal, L);
                    const float nDl = dot(payload.normal, L);
                    const float LnDl = -dot(newReservoir.lightSample.normal, L);

                    Li = newReservoir.lightSample.Li;
                    weight = max(LnDl, 0) / (Ldist * Ldist) * newReservoir.W;
                }
            }
        }

        // Emitter sampling Case 2
        // -------------- Random light sampling -------------
        else if (useNEE) {
            uint lightIndex = (uint) (nextRand(seed) * g_frameData.lightNumber);

            LightParameter light = lights[lightIndex];
            SampleLight(payload.origin, light, seed, lightSample);

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
                    wo = ToLocal(payload.tangent, payload.bitangent, payload.normal, L);
                    const float scatterPdf = bsdf::Pdf(material, payload, wo);

                    Li = lightSample.Li;
                    weight = powerHeuristic(lightPdf, scatterPdf) / lightPdf;
                }
            }
        }
        if (weight > 0)
        {
            if (depth == 1)
            {
                const float3 fDiffuse = bsdf::EvalDiffuse(material, payload, wo);
                const float3 fSpecular = bsdf::EvalSpecular(material, payload, wo);

                const float3 LCommon = weight * Li * throughput;
                const float3 LDiffuse = LCommon * fDiffuse;
                const float3 LSpecular = LCommon * fSpecular;

                const float3 L = LDiffuse + LSpecular;
                result += L;

                pathResult.diffuseRadiance += LDiffuse;
                pathResult.specularRadiance += LSpecular;
#if OUTPUT_SVGF_RELATED
                pathResult.directRadiance += L;
#endif
                const float3 LDiffuseIllum = LCommon * wo.z * M_1_PIf;
                pathResult.diffuseIllumination += LDiffuseIllum;
            }
            else {
                const float3 f = bsdf::Eval(material, payload, wo);
                const float3 L = weight * Li * f * throughput;
                result += L;
                resultExceptFirst += weight * Li * f * throughputExceptFirst;
            }
        }

#endif
        // ------------------------------------------------------------
        // --------------------- BSDF sampling ------------------------
        // ------------------------------------------------------------
        bsdf::Sample(material, payload, seed, bs);

        payload.direction = bs.wo.x * payload.tangent + bs.wo.y * payload.bitangent + bs.wo.z * payload.normal;
        // payload.attenuation = bs.weight;
        payload.scatterPdf = bs.pdf;
        payload.sampledLobe = bs.sampledLobe;

        foundNonDelta |= bsdf::getReflectionLobe(material) & BSDF_LOBE_NON_DELTA;

        if (depth == 1)
        {
            primaryReflectionLobe = bs.sampledLobe;
            deltaTransmissionPath = (bs.sampledLobe == BSDF_LOBE_DELTA_TRANSMISSION);
        }
        else {
            if (!foundNonDelta)
            {
                bool totalInternalReflection = (bs.sampledLobe == BSDF_LOBE_DELTA_REFLECTION) && (bs.pdf == 1.0f);
                bool isThisPathDeltaTransmission = (material.materialType == BSDF_TYPE_DIELECTRIC) && ((bs.sampledLobe == BSDF_LOBE_DELTA_TRANSMISSION) || totalInternalReflection);
                deltaTransmissionPath &= isThisPathDeltaTransmission;
            }
        }

        throughput *= bs.weight;
        if (depth >= 2) {
            throughputExceptFirst *= bs.weight;
        }

        if (dot(throughput, throughput) == 0.0) {
            break;
        }

        pathResult.radiance = result;
        ray.Direction = payload.direction;
        ray.Origin = payload.origin;
        depth += 1;

        TraceRay(gRtScene, 0 /*rayFlags*/, 0xFF, 0 /* ray index*/, 2, 0, ray, payload);
#if USE_NEXT_EVENT_ESTIMATION
        // Handle if BSDF sampled ray hits lights source.
        float scatterPdf = payload.scatterPdf;
        if (useReSTIR)
        {
            emissionWeight = 0.0f;
        }
        else if (payload.lightIndex >= 0 && scatterPdf > 0.0f) {
            float lightPdfArea = lights[payload.lightIndex].normalAndPdf.w / g_frameData.lightNumber;
            float lightPdf = (payload.t * payload.t) / dot(payload.normal, -ray.Direction) * lightPdfArea;
            emissionWeight = powerHeuristic(scatterPdf, lightPdf);
        }
        else {
            emissionWeight = 1.0f;
        }
#endif
    }

    if (accumulateResult)
    {
        if (primaryReflectionLobe == BSDF_LOBE_DIFFUSE_REFLECTION)
        {
            pathResult.diffuseRadiance += result;
            pathResult.diffuseIllumination += resultExceptFirst;
        }
        else if (primaryReflectionLobe == BSDF_LOBE_GLOSSY_REFLECTION)
        {
            pathResult.specularRadiance += result;
            pathResult.specularIllumination += resultExceptFirst;
        }
        else if (primaryReflectionLobe == BSDF_LOBE_DELTA_REFLECTION)
        {
            pathResult.deltaReflectionRadiance = result;
        }
        else if(primaryReflectionLobe == BSDF_LOBE_DELTA_TRANSMISSION){
            if (deltaTransmissionPath) {
                pathResult.deltaTransmissionRadiance = result;
            }
            else {
                pathResult.residualRadiance = result;
            }
        }
    }

    pathResult.radiance = result;
    return;
}



void PathTraceDeltaReflectance(in RayDesc ray, inout uint seed, inout PathTraceResult pathResult, inout RayPayload payload)
{
    Material material = g_materialinfo[payload.materialIndex];
    uint materialLobe = bsdf::getReflectionLobe(material);
    bool traceDeltaReflection = (materialLobe & BSDF_LOBE_DELTA_REFLECTION) || (materialLobe & BSDF_LOBE_GLOSSY_REFLECTION);
    if (!traceDeltaReflection)
        return;

    // Only delta reflection
    payload.requestedLobe = BSDF_LOBE_DELTA_REFLECTION;

    BSDFSample bs;

    while (true)
    {
        // ------------------------------------------------------------
        // --------------------- BSDF sampling ------------------------
        // ------------------------------------------------------------
        bsdf::Sample(material, payload, seed, bs);
        payload.direction = reflect(payload.direction, payload.normal);

        // payload.direction = bs.wo.x * payload.tangent + bs.wo.y * payload.bitangent + bs.wo.z * payload.normal;
        //payload.attenuation = bs.weight;
        //payload.scatterPdf = bs.pdf;
        //payload.sampledLobe = bs.sampledLobe;

        ray.Direction = payload.direction;
        ray.Origin = payload.origin;
        payload.attenuation *= bs.weight;
        payload.depth += 1;

        TraceRay(gRtScene, 0 /*rayFlags*/, 0xFF, 0 /* ray index*/, 2, 0, ray, payload);

        material = g_materialinfo[payload.materialIndex];

        bool nonDelta = (bsdf::getReflectionLobe(material) & BSDF_LOBE_NON_DELTA);

        // TODO : 2 or full?
        if (nonDelta || (payload.depth >= 2) || payload.done)
        {
            pathResult.deltaReflectionReflectance = bsdf::getApproximatedReflectance(material, payload);
            if (payload.done && (materialLobe & BSDF_LOBE_DELTA_REFLECTION)) {
                pathResult.deltaReflectionEmission = payload.attenuation*payload.emission;
            }
            pathResult.deltaReflectionPosition = payload.origin;
            pathResult.deltaReflectionNormal = payload.normal;
            pathResult.deltaReflectionMeshID = payload.instanceIndex;

            break;
        }
    }

    // TODO : trace full path?
    //payload.requestedLobe = BSDF_LOBE_ALL;
    //PathTrace(ray, seed, pathResult, payload, false);
    //pathResult.deltaReflectionRadiance = pathResult.radiance;
}


void PathTraceDeltaTransmission(in RayDesc ray, inout uint seed, inout PathTraceResult pathResult, inout RayPayload payload)
{
    Material material = g_materialinfo[payload.materialIndex];
    if (!(bsdf::getReflectionLobe(material) & BSDF_LOBE_DELTA_TRANSMISSION))
        return;

    // Only delta reflection
    payload.requestedLobe = BSDF_LOBE_DELTA_TRANSMISSION;

    BSDFSample bs;

    while (true)
    {
        // ------------------------------------------------------------
        // --------------------- BSDF sampling ------------------------
        // ------------------------------------------------------------
        bsdf::Sample(material, payload, seed, bs);

        payload.direction = bs.wo.x * payload.tangent + bs.wo.y * payload.bitangent + bs.wo.z * payload.normal;

        ray.Direction = payload.direction;
        ray.Origin = payload.origin;
        payload.attenuation *= bs.weight;
        payload.depth += 1;

        TraceRay(gRtScene, 0 /*rayFlags*/, 0xFF, 0 /* ray index*/, 2, 0, ray, payload);

        material = g_materialinfo[payload.materialIndex];

        bool nonDelta = (bsdf::getReflectionLobe(material) & BSDF_LOBE_NON_DELTA);

        if (nonDelta || (payload.depth >= gPathTracer.maxDepthTransmittance) || payload.done)
        {
            pathResult.deltaTransmissionReflectance = bsdf::getApproximatedReflectance(material, payload);
            if (payload.done) {
                pathResult.deltaTransmissionEmission = payload.attenuation * payload.emission;
            }
            pathResult.deltaTransmissionPosition = payload.origin;
            pathResult.deltaTransmissionNormal = payload.normal;
            pathResult.deltaTransmissionMeshID = payload.instanceIndex;

            break;
        }
    }
    // TODO : trace full path?
    //payload.requestedLobe = BSDF_LOBE_ALL;
    //PathTrace(ray, seed, pathResult, payload, false);
    //pathResult.deltaTransmissionRadiance = pathResult.radiance;
}

[shader("raygeneration")]
void rayGen()
{
    uint3 launchIndex = DispatchRaysIndex();
    const int2 ipos = launchIndex.xy;

    uint3 launchDim = DispatchRaysDimensions();

    float2 crd = float2(ipos);
    float2 dims = float2(launchDim.xy);

    float2 pixel = ((crd / dims) * 2.f - 1.f);
    float aspectRatio = dims.x / dims.y;

    float depth = 0.0f;
    RayDesc ray;
    ray.TMin = SCENE_T_MIN;
    ray.TMax = SCENE_T_MAX;
    PathTraceResult pathResult;
    
    // Only 1 SPP !!!

    uint seed = initRand(launchIndex.x + launchIndex.y * launchDim.x, g_frameData.totalFrameNumber, 16);

    float2 jitter = 0.5f; // float2(nextRand(seed), nextRand(seed));

    float2 d = pixel + jitter * 1.f / dims * 2.f;

    ray.Origin = g_frameData.cameraPosition.xyz;
    ray.Direction = normalize(g_frameData.u.xyz * d.x - g_frameData.v.xyz * d.y + g_frameData.w.xyz);
    RayPayload payloadPrimary;

    PrimaryPath(ray, pathResult, payloadPrimary);
    Material material = g_materialinfo[payloadPrimary.materialIndex];
    uint materialReflectionLobe = bsdf::getReflectionLobe(material);

#if OUTPUT_ADDITIONAL_PROPERTIES
    if (materialReflectionLobe & (BSDF_LOBE_DELTA_REFLECTION)) {
        RayPayload payload = payloadPrimary;
        PathTraceDeltaReflectance(ray, seed, pathResult, payload);
        // ******************
        // Noise free outputs
        // ******************
        gDeltaReflectionPositionMeshID[ipos] = float4(pathResult.deltaReflectionPosition, pathResult.deltaReflectionMeshID);
        gDeltaReflectionNormal[ipos] = float4(pathResult.deltaReflectionNormal, 0.0f);
        gDeltaReflectionReflectance[ipos] = float4(pathResult.deltaReflectionReflectance, 1.0f);
        gDeltaReflectionEmission[ipos] = float4(pathResult.deltaReflectionEmission, 1.0f);
        
    }
    if (materialReflectionLobe & BSDF_LOBE_DELTA_TRANSMISSION) {
        RayPayload payload = payloadPrimary;
        PathTraceDeltaTransmission(ray, seed, pathResult, payload);
        // ******************
        // Noise free outputs
        // ******************
        gDeltaTransmissionPositionMeshID[ipos] = float4(pathResult.deltaTransmissionPosition, pathResult.deltaTransmissionMeshID);
        gDeltaTransmissionNormal[ipos] = float4(pathResult.deltaTransmissionNormal, 0.0f);
        gDeltaTransmissionReflectance[ipos] = float4(pathResult.deltaTransmissionReflectance, 1.0f);
        gDeltaTransmissionEmission[ipos] = float4(pathResult.deltaTransmissionEmission, 1.0f);
    }
#endif

    PathTrace(ray, seed, pathResult, payloadPrimary, true);

#if OUTPUT_ADDITIONAL_PROPERTIES

    // ******************
    // Noise free outputs
    // ******************
    gPositionMeshID[ipos] = float4(pathResult.position, pathResult.instanceIndex);
    gNormalDepth[ipos] = float4(pathResult.normal, pathResult.depth);
    gDiffuseReflectance[ipos] = float4(pathResult.diffuseReflectance, 1.0f);
    gSpecularReflectance[ipos] = float4(pathResult.specularReflectance, 1.0f);
    gEmission[ipos] = float4(pathResult.emission, 1.0f);

    gPrimaryPathType[ipos] = uint(pathResult.primaryPathType);
    gRoughness[ipos] = pathResult.roughness;

    // Minimize UAV writing!
    // ******************
    // Noise free outputs
    // ******************
    //gDeltaReflectionPositionMeshID[ipos] = float4(pathResult.deltaReflectionPosition, pathResult.deltaReflectionMeshID);
    //gDeltaReflectionNormal[ipos] = float4(pathResult.deltaReflectionNormal, 0.0f);
    //gDeltaReflectionReflectance[ipos] = float4(pathResult.deltaReflectionReflectance, 1.0f);
    //gDeltaReflectionEmission[ipos] = float4(pathResult.deltaReflectionEmission, 1.0f);

    // ******************
    // Noise free outputs
    // ******************
    //gDeltaTransmissionPositionMeshID[ipos] = float4(pathResult.deltaTransmissionPosition, pathResult.deltaTransmissionMeshID);
    //gDeltaTransmissionNormal[ipos] = float4(pathResult.deltaTransmissionNormal, 0.0f);
    //gDeltaTransmissionReflectance[ipos] = float4(pathResult.deltaTransmissionReflectance, 1.0f);
    //gDeltaTransmissionEmission[ipos] = float4(pathResult.deltaTransmissionEmission, 1.0f);


    // ******************
    // Noisy outputs
    // ******************

    // (1) Diffuse
    gDiffuseRadiance[ipos] = float4(pathResult.diffuseIllumination, 1.0f);  // already demodulated

    // (2) Specular
    float3 specularIllumination = pathResult.specularRadiance / max(pathResult.specularReflectance, kMinReflectance);
    gSpecularRadiance[ipos] = float4(specularIllumination, 1.0f);

    // (3) Delta Reflection
    float3 deltaReflectionIllumination = max((pathResult.deltaReflectionRadiance - pathResult.deltaReflectionEmission), float3(0, 0, 0)) / max(pathResult.deltaReflectionReflectance, kMinReflectance);
    gDeltaReflectionRadiance[ipos] = float4(deltaReflectionIllumination, 1.0f);

    // (4) Delta Transmission
    float3 deltaTransmissionIllumination = max((pathResult.deltaTransmissionRadiance - pathResult.deltaTransmissionEmission), float3(0, 0, 0)) / max(pathResult.deltaTransmissionReflectance, kMinReflectance);
    gDeltaTransmissionRadiance[ipos] = float4(deltaTransmissionIllumination, 1.0f);

    // (5) Residual
    gResidualRadiance[ipos] = float4(pathResult.residualRadiance, 1.0f);

#if OUTPUT_SVGF_RELATED
    gReflectance[ipos] = float4(pathResult.reflectance, 1.0f);
    // SVGF
    float3 indirectRadiance = pathResult.radiance - pathResult.emission - pathResult.directRadiance;
    float3 directIllumination = pathResult.directRadiance / max(pathResult.reflectance, kMinReflectance);
    float3 indirectIllumination = indirectRadiance / max(pathResult.reflectance, kMinReflectance);
    
    gDirectIllumination[ipos] = float4(directIllumination, 1.0f);
    gIndirectIllumination[ipos] = float4(indirectIllumination, 1.0f);
#endif

    // Reprojection
    float4 projCoord = mul(float4(pathResult.position, 1.0f), g_frameData.previousProjView);
    projCoord /= projCoord.w;
    float2 prevPixel = float2(projCoord.x, -projCoord.y);
    prevPixel = (prevPixel + 1) * 0.5;

    gMotionVector[ipos] = prevPixel;

#endif
    float3 radiance = pathResult.radiance;
    if (g_frameData.frameNumber > 1 && gPathTracer.accumulateFrames) {
        float3 oldColor = gOutputHDR[ipos].xyz;
        float a = 1.0f / (float)(g_frameData.frameNumber);
        radiance = lerp(oldColor, pathResult.radiance, a);
    }
    gOutputHDR[ipos] = float4(radiance, 1.0f);
    radiance = radiance / (radiance + 1);
    radiance = pow(radiance, 1.f / 2.2f);
    gOutput[ipos] = float4(radiance, 1.0f);
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
    payload.t = 1e5;
}

[shader("miss")]
void miss(inout RayPayload payload)
{
    payload.emission = float3(0, 0, 0);
    payload.t = 1e5;
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

    float2 uvtemp = vtx0.uv * barycentrics.x + vtx1.uv * barycentrics.y + vtx2.uv * barycentrics.z;
    payload.uv = float2(uvtemp.x, 1 - uvtemp.y);

    payload.diffuseReflectance = materialInfo.diffuseReflectanceTextureID ? g_textures.SampleLevel(g_s0, payload.uv, 0.0f).xyz : materialInfo.diffuseReflectance;

    // TODO : add fetch from texture
    payload.specularReflectance = materialInfo.specularReflectance;
    payload.specularTransmittance = materialInfo.specularTransmittance;
    payload.roughness = materialInfo.roughness;


    payload.bitangent = getBinormal(normal);
    payload.tangent = cross(payload.bitangent, normal);
    float3 wi = -rayDir;
    payload.wi = float3(dot(payload.tangent, wi), dot(payload.bitangent, wi), dot(payload.normal, wi));
    
    if (isEmitter) {
        payload.done = true;
        payload.emission = nDotV > 0 ? lights[geometryInfo.lightIndex].emission.xyz : float3(0, 0, 0);
        return;
    }
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