#ifndef BSDF_DIELECTRIC_HLSLI
#define BSDF_DIELECTRIC_HLSLI

#include "../stdafx.hlsli"

namespace dielectric
{
	float Pdf(in Material mat, in RayPayload si, in float3 wo) {
		// delta function!
		return 0.0f;
	}

	float3 Eval(in Material mat, in RayPayload si, in float3 wo) {
		// delta function!
		return float3(0, 0, 0);
	}

	float4 EvalAndPdf(in Material mat, in RayPayload si, in float3 wo) {
		// delta function!
		return float4(0, 0, 0, 0);
	}

	void Sample(in Material mat, in RayPayload si, inout uint seed, inout BSDFSample bs) {
		float ior = mat.intIOR / mat.extIOR;
		float eta = si.wi.z < 0.0f ? ior : 1 / ior;
		float cosThetaT;
		const float F = fresnel::DielectricReflectance(eta, abs(si.wi.z), cosThetaT);

		bool sampleR = si.requestedLobe & BSDF_LOBE_DELTA_REFLECTION;
		bool sampleT = si.requestedLobe & BSDF_LOBE_DELTA_TRANSMISSION;

		float reflectionProbability = F;
		if (sampleR && sampleT)
			reflectionProbability = F;
		else if (sampleR)
			reflectionProbability = 1.0f;
		else if (sampleT)
			reflectionProbability = 0.0f;

		// Total internal reflection
		if (F == 1.0f)
		{
			reflectionProbability = 1.0f;
		}

		if (nextRand(seed) <= reflectionProbability)
		{
			// Reflect
			bs.wo = float3(-si.wi.x, -si.wi.y, si.wi.z);
			bs.pdf = reflectionProbability;
			bs.weight = si.specularReflectance;
			if (!sampleT)
				bs.weight *= F;
			bs.sampledLobe = BSDF_LOBE_DELTA_REFLECTION;
		}
		else
		{
			// Refract
			bs.wo = float3(-si.wi.x * eta, -si.wi.y * eta, -sign(si.wi.z) * cosThetaT);
			bs.pdf = 1 - reflectionProbability;
			bs.weight = si.specularTransmittance * eta * eta;
			if (!sampleR)
				bs.weight *= (1-F);
			bs.sampledLobe = BSDF_LOBE_DELTA_TRANSMISSION;
		}
	}
}
#endif