#ifndef LIGHT_SAMPLING
#define LIGHT_SAMPLING

#include "../Common/CommonStructs.hlsli"

void SampleLight(in float3 p, in LightParameter light, inout uint seed, inout LightSample lightSample) {
    // Currently only rectangular light is supported!

    float r1 = nextRand(seed);
    float r2 = nextRand(seed);

    lightSample.position = light.position.xyz + light.u.xyz * r1 + light.v.xyz * r2;
    lightSample.normal = light.normalAndPdf.xyz;
    lightSample.pdf = light.normalAndPdf.w;
    lightSample.Li = light.emission.xyz;
}
#endif