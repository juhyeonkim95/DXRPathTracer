#include "ShaderUtils.hlsli"
#include "BSDF.hlsli"
#include "SampleLight.hlsli"

RWStructuredBuffer<Reservoir> gPrevReserviors : register(u9);
RWStructuredBuffer<Reservoir> gCurrReserviors : register(u10);

void UpdateReservoir(inout Reservoir r, in LightSample lightSample, float weight, inout uint seed)
{
    weight = max(weight, 1e-10);
    r.wSum += weight;
    r.M += 1;
    if (nextRand(seed) <= weight / r.wSum)
    {
        r.lightSample.position = lightSample.position;
        r.lightSample.Li = lightSample.Li;
        r.lightSample.normal = lightSample.normal;
        r.lightSample.lightIndex = lightSample.lightIndex;
    }
    return;
}

float getPHat(in LightSample lightSample, in RayPayload payload)
{
    Material material = g_materialinfo[payload.materialIndex];

    const float Ldist = length(lightSample.position - payload.origin);
    const float3 L = normalize(lightSample.position - payload.origin);
    const float nDl = dot(payload.normal, L);
    const float LnDl = -dot(lightSample.normal, L);
    if (LnDl <= 0.0f) {
        return 0.0f;
    }
    float3 wo = ToLocal(payload.tangent, payload.bitangent, payload.normal, L);
    const float3 f = bsdf::Eval(material, payload, wo);
    float p_hat = length(f * lightSample.Li) * LnDl / (Ldist * Ldist);
    return p_hat;
}


void CopyReservoirs(inout Reservoir s, in Reservoir r)
{
    s.M = r.M;
    s.W = r.W;
    s.wSum = r.wSum;
    s.lightSample.position = r.lightSample.position;
    s.lightSample.Li = r.lightSample.Li;
    s.lightSample.normal = r.lightSample.normal;
    s.lightSample.lightIndex = r.lightSample.lightIndex;

    return;
}

void CombineReservoirs(in RayPayload payload, in Reservoir r1, in Reservoir r2, inout Reservoir s)
{
    UpdateReservoir(s, r1.lightSample, getPHat(r1.lightSample, payload) * r1.W * r1.M, payload.seed);
    UpdateReservoir(s, r2.lightSample, getPHat(r2.lightSample, payload) * r2.W * r2.M, payload.seed);
    s.M = r1.M + r2.M;
    float pHat = getPHat(s.lightSample, payload);
    s.W = (pHat == 0) ? 0: (1 / pHat) * (s.wSum / s.M);
    return;
}

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



    if (payload.done)
    {
        pathResult.radiance = payload.emission;
        return;
    }

    Material material = g_materialinfo[payload.materialIndex];

    LightSample lightSample;

    RayDesc shadowRay;
    shadowRay.TMin = SCENE_T_MIN;



    // float4 reservoir = float4(0.0f);
    bool doNEE = material.materialType & BSDF_TYPE_DIFFUSE || material.materialType & BSDF_TYPE_ROUGH_CONDUCTOR;
    
    
    if (gReSTIR.enableReSTIR)
    {
        Reservoir reservoir;
        reservoir.M = 0;
        reservoir.wSum = 0;
        reservoir.W = 0;
        uint M = min(g_frameData.lightNumber, gReSTIR.lightCandidateCount);

        // (1) Initial candidates generation
        for (int i = 0; i < M; i++)
        {
            uint lightIndex = (uint) (nextRand(payload.seed) * g_frameData.lightNumber);
            lightIndex = min(lightIndex, g_frameData.lightNumber - 1);

            LightParameter light = lights[lightIndex];
            SampleLight(payload.origin, light, payload.seed, lightSample);
            lightSample.lightIndex = lightIndex;

            float p = lightSample.pdf / ((float)g_frameData.lightNumber);
            float pHat = getPHat(lightSample, payload);

            UpdateReservoir(reservoir, lightSample, pHat / p, payload.seed);
        }

        float pHat = getPHat(reservoir.lightSample, payload);
        reservoir.W = (pHat == 0) ? 0 : (1.0f / pHat) * (reservoir.wSum / reservoir.M);

        float Ldist = length(reservoir.lightSample.position - payload.origin);
        float3 L = normalize(reservoir.lightSample.position - payload.origin);

        shadowRay.Origin = payload.origin;
        shadowRay.Direction = L;
        shadowRay.TMax = Ldist - SCENE_T_MIN;
        ShadowPayload shadowPayload;
        shadowPayload.hit = false;

        TraceRay(gRtScene, 0  /*rayFlags*/, 0xFF, 1 /* ray index*/, 0, 1, shadowRay, shadowPayload);
        if (shadowPayload.hit) {
            reservoir.W = 0.0f;
        }

        uint3 launchDim = DispatchRaysDimensions();
        uint3 launchIndex = DispatchRaysIndex();
        float2 dims = float2(launchDim.xy);

        float4 projCoord = mul(float4(pathResult.position, 1.0f), g_frameData.previousProjView);
        projCoord /= projCoord.w;
        float2 prevPixel = float2(projCoord.x, -projCoord.y);
        prevPixel = (prevPixel + 1) * 0.5;

        uint prevPixelCoordX = uint((prevPixel.x * dims.x));// clamp(0, uint((prevPixel.x * dims.x)), dims.x - 1);
        uint prevPixelCoordY = uint((prevPixel.y * dims.y));// clamp(0, uint((prevPixel.y * dims.y)), dims.y - 1);
        uint2 prevPixelCoord = uint2(prevPixelCoordX, prevPixelCoordY);

        float3 prevPosition = gOutputPositionGeomIDPrev[prevPixelCoord].rgb;
        float3 prevNormal = gOutputNormalPrev[prevPixelCoord].rgb;

        bool consistency = (gOutputPositionGeomIDPrev[prevPixelCoord].a == pathResult.instanceIndex) && dot(prevNormal, pathResult.normal) > gReSTIR.normalThreshold;

        uint linearIndex = launchIndex.x + launchIndex.y * launchDim.x;

        Reservoir newReservoir;
        newReservoir.M = 0;
        newReservoir.wSum = 0;
        newReservoir.W = 0;

        bool inside = prevPixelCoordX >= 0 && prevPixelCoordX < dims.x&& prevPixelCoordY >= 0 && prevPixelCoordY < dims.y;

        bool doTemporalReuse = (gReSTIR.resamplingMode == ReSTIR_MODE_TEMPORAL_REUSE || gReSTIR.resamplingMode == ReSTIR_MODE_SPATIOTEMPORAL_REUSE) && inside && consistency && (g_frameData.totalFrameNumber > 1);

        if (doTemporalReuse)
        {
            uint linearIndexPrev = prevPixelCoordX + prevPixelCoordY * launchDim.x;
            Reservoir previousReservoir = gPrevReserviors[linearIndexPrev];
            CombineReservoirs(payload, reservoir, previousReservoir, newReservoir);
        }
        else {
            newReservoir = reservoir;
        }

        newReservoir.M = lightSamplesCount;
        pHat = getPHat(newReservoir.lightSample, payload);
        newReservoir.W = (pHat == 0) ? 0 : (1 / pHat) * (newReservoir.wSum / newReservoir.M);



        newReservoir.M = min(newReservoir.M, gReSTIR.maxHistoryLength);

        if (newReservoir.W > 0)
        {
            Ldist = length(newReservoir.lightSample.position - payload.origin);
            L = normalize(newReservoir.lightSample.position - payload.origin);

            shadowRay.Origin = payload.origin;
            shadowRay.Direction = L;
            shadowRay.TMax = Ldist - SCENE_T_MIN;
            shadowPayload.hit = false;
            TraceRay(gRtScene, 0  /*rayFlags*/, 0xFF, 1 /* ray index*/, 0, 1, shadowRay, shadowPayload);
            if (!shadowPayload.hit) {
                float3 wo = ToLocal(payload.tangent, payload.bitangent, payload.normal, L);
                const float3 f = bsdf::Eval(material, payload, wo);
                const float nDl = dot(payload.normal, L);
                const float LnDl = -dot(lightSample.normal, L);

                const float3 L = newReservoir.lightSample.Li * f * max(LnDl, 0) / (Ldist * Ldist) * newReservoir.W;
                result += L;
            }
        }
        gCurrReserviors[linearIndex] = newReservoir;
    }
    else {
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
                //const float scatterPdf = bsdf::Pdf(material, payload, wo);
                const float3 f = bsdf::Eval(material, payload, wo);

                //const float weight = powerHeuristic(lightPdf, scatterPdf);

                const float3 L = lightSample.Li * f / lightPdf * throughput;
                result += L;
            }
        }
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