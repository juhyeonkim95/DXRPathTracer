#include "../Core/BSDF/BSDFLobes.hlsli"

Texture2D gDiffuseRadiance : register(t0);
Texture2D gSpecularRadiance : register(t1);
Texture2D gDiffuseReflectance : register(t2);
Texture2D gSpecularReflectance : register(t3);
Texture2D gEmission : register(t4);
Texture2D gDeltaReflectionRadiance : register(t5);
Texture2D gDeltaTransmissionRadiance : register(t6);
Texture2D gDeltaReflectionReflectance : register(t7);
Texture2D gDeltaTransmissionReflectance : register(t8);
Texture2D gDeltaReflectionEmission : register(t9);
Texture2D gDeltaTransmissionEmission : register(t10);
Texture2D gResidualRadiance : register(t11);
Texture2D<uint> gPathType : register(t12);


// Generates a seed for a random number generator from 2 inputs plus a backoff
uint initRand(uint val0, uint val1, uint backoff = 16)
{
    uint v0 = val0, v1 = val1, s0 = 0;

    [unroll]
    for (uint n = 0; n < backoff; n++)
    {
        s0 += 0x9e3779b9;
        v0 += ((v1 << 4) + 0xa341316c) ^ (v1 + s0) ^ ((v1 >> 5) + 0xc8013ea4);
        v1 += ((v0 << 4) + 0xad90777d) ^ (v0 + s0) ^ ((v0 >> 5) + 0x7e95761e);
    }
    return v0;
}

// Takes our seed, updates it, and returns a pseudorandom float in [0..1]
float nextRand(inout uint s)
{
    s = (1664525u * s + 1013904223u);
    return float(s & 0x00FFFFFF) / float(0x01000000);
}

float3 getRandomColor(uint index)
{
    uint seed = initRand(index, index + 100, 16);
    float r = nextRand(seed);
    float g = nextRand(seed);
    float b = nextRand(seed);
    return float3(r, g, b);
}




SamplerState s1 : register(s0);

struct VS_OUTPUT
{
    float4 pos: SV_POSITION;
    float2 texCoord: TEXCOORD;
};

cbuffer ConstantBuffer : register(b0)
{
	uint enableDiffuseRadiance;
	uint enableDiffuseReflectance;
	uint enableSpecularRadiance;
	uint enableSpecularReflectance;
	uint enableEmission;

	uint enableDeltaReflectionRadiance;
	uint enableDeltaReflectionReflectance;
	uint enableDeltaReflectionEmission;

	uint enableDeltaTransmissionRadiance;
	uint enableDeltaTransmissionReflectance;
	uint enableDeltaTransmissionEmission;

	uint enableResidualRadiance;
};

float4 main(VS_OUTPUT input) : SV_TARGET
{
    const int2 ipos = int2(input.pos.xy);
    float3 diffuseRadiance = gDiffuseRadiance.Load(int3(ipos, 0)).rgb;
    float3 specularRadiance = gSpecularRadiance.Load(int3(ipos, 0)).rgb;
    float3 diffuseReflectance = gDiffuseReflectance.Load(int3(ipos, 0)).rgb;
    float3 specularReflectance = gSpecularReflectance.Load(int3(ipos, 0)).rgb;
    float3 emission = gEmission.Load(int3(ipos, 0)).rgb;

    float3 deltaReflectionRadiance = gDeltaReflectionRadiance.Load(int3(ipos, 0)).rgb;
    float3 deltaTransmissionRadiance = gDeltaTransmissionRadiance.Load(int3(ipos, 0)).rgb;
    float3 deltaReflectionReflectance = gDeltaReflectionReflectance.Load(int3(ipos, 0)).rgb;
    float3 deltaTransmissionReflectance = gDeltaTransmissionReflectance.Load(int3(ipos, 0)).rgb;
    float3 deltaReflectionEmission = gDeltaReflectionEmission.Load(int3(ipos, 0)).rgb;
    float3 deltaTransmissionEmission = gDeltaTransmissionEmission.Load(int3(ipos, 0)).rgb;
    
    float3 residualRadiance = gResidualRadiance.Load(int3(ipos, 0)).rgb;

    float3 color = 0.0f;

    uint pathType = gPathType.Load(int3(ipos, 0));

    //deltaReflectionReflectance = float(bool(pathType & BSDF_LOBE_DELTA_REFLECTION));
    //deltaTransmissionReflectance = float(bool(pathType & BSDF_LOBE_DELTA_TRANSMISSION));


    
    if (enableDiffuseRadiance) {
        float3 diffuse = diffuseRadiance;
        if (enableDiffuseReflectance) {
            diffuse *= diffuseReflectance;
        }
        color += diffuse;
    }
    if (!enableDiffuseRadiance && enableDiffuseReflectance)
    {
        color = diffuseReflectance;
    }

    if (enableSpecularRadiance) {
        float3 specular = specularRadiance;
        if (enableSpecularReflectance) {
            specular *= specularReflectance;
        }
        color += specular;
    }
    if (!enableSpecularRadiance && enableSpecularReflectance)
    {
        color = specularReflectance;
    }

    if (enableEmission)
    {
        color += emission;
    }

    if (pathType & BSDF_LOBE_DELTA_REFLECTION)
    {
        if (enableDeltaReflectionRadiance) {
            float3 deltaReflection = deltaReflectionRadiance;
            if (enableDeltaReflectionReflectance) {
                deltaReflection *= deltaReflectionReflectance;
            }
            color += deltaReflection;
        }
        if (!enableDeltaReflectionRadiance && enableDeltaReflectionReflectance)
        {
            color = deltaReflectionReflectance;
        }
        if (enableDeltaReflectionEmission)
        {
            color += deltaReflectionEmission;
        }
    }

    if (pathType & BSDF_LOBE_DELTA_TRANSMISSION)
    {
        if (enableDeltaTransmissionRadiance) {
            float3 deltaTransmission = deltaTransmissionRadiance;
            if (enableDeltaTransmissionReflectance) {
                deltaTransmission *= deltaTransmissionReflectance;
            }
            color += deltaTransmission;
        }
        if (!enableDeltaTransmissionRadiance && enableDeltaTransmissionReflectance)
        {
            color = deltaTransmissionReflectance;
        }

        if (enableDeltaTransmissionEmission)
        {
            color += deltaTransmissionEmission;
        }
    }
    
    if (enableResidualRadiance)
    {
        color += residualRadiance;
    }

    return float4(color, 1.0f);
}