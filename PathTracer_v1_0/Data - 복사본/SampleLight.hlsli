#include "ShaderStructs.hlsli"

void SampleLight(in float3 p, in LightParameter light, inout uint seed, inout LightSample lightSample) {
    float r1 = nextRand(seed);
    float r2 = nextRand(seed);

    float3 o = light.position.xyz;
    float3 u = light.u.xyz;
    float3 v = light.v.xyz;

    lightSample.position = o + u * r1 + v * r2;
    lightSample.normal = light.normalAndPdf.xyz;
    lightSample.pdf = light.normalAndPdf.w;
    lightSample.Li = light.emission.xyz;
}