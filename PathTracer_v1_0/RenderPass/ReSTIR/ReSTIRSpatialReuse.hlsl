RWStructuredBuffer<Reservoir> gCurrReserviors : register(u9);
RWStructuredBuffer<Reservoir> gSpatialReserviors : register(u10);

[shader("raygeneration")]
void rayGen()
{
	// Get our pixel's position on the screen
	uint2 launchIndex = DispatchRaysIndex().xy;
	uint2 launchDim = DispatchRaysDimensions().xy;

    int range = 5;
    int lightSamplesCount = 0;
    for (int i = 0; i < 10; i++) {
        uint neighborPixelOffsetX = int(nextRand(payload.seed) * range * 2) - range;
        uint neighborPixelOffsetY = int(nextRand(payload.seed) * range * 2) - range;
        uint neighborPixelX = max(0, min(dims.x - 1, launchIndex.x + neighborPixelOffsetX));
        uint neighborPixelY = max(0, min(dims.y - 1, launchIndex.y + neighborPixelOffsetY));

        uint linearIndexNeigh = neighborPixelX + neighborPixelY * launchDim.x;
        Reservoir r = gCurrReserviors[linearIndexNeigh];

        UpdateReservoir(newReservoir, r.lightSample, getPHat(r.lightSample, payload) * r.W * r.M, payload.seed);
        lightSamplesCount += r.M;
    }
}