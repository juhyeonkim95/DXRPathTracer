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
    s.W = (pHat == 0) ? 0 : (1 / pHat) * (s.wSum / s.M);
    return;
}

Reservoir getLightSampleReSTIR(in RayPayload payload, in PathTraceResult pathResult)
{
    LightSample lightSample;
    RayDesc shadowRay;
    shadowRay.TMin = SCENE_T_MIN;

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


    // (2) Check visibility
    shadowRay.Origin = payload.origin;
    shadowRay.Direction = L;
    shadowRay.TMax = Ldist - SCENE_T_MIN;
    ShadowPayload shadowPayload;
    shadowPayload.hit = false;

    TraceRay(gRtScene, 0  /*rayFlags*/, 0xFF, 1 /* ray index*/, 0, 1, shadowRay, shadowPayload);
    if (shadowPayload.hit) {
        reservoir.W = 0.0f;
    }

    // (3) Temporal reuse
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

    // Temporal reuse only consistency holds.
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
    newReservoir.M = min(newReservoir.M, gReSTIR.maxHistoryLength);
    gCurrReserviors[linearIndex] = newReservoir;

    return newReservoir;
}