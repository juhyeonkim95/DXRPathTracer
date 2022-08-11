#ifndef BSDF_DIELECTRIC
#define BSDF_DIELECTRIC

#include "../stdafx.hlsli"

namespace dielectric
{
	float Pdf(in Material mat, in RayPayload si, in float3 wo) {
		return 0.0f;
	}

	float3 Eval(in Material mat, in RayPayload si, in float3 wo) {
		return float3(0, 0, 0);
	}

	float4 EvalAndPdf(in Material mat, in RayPayload si, in float3 wo) {
		return float4(0, 0, 0, 0);
	}

	void Sample(in Material mat, in RayPayload si, inout uint seed, inout BSDFSample bs) {
		float ior = mat.intIOR / mat.extIOR;
		float eta = si.wi.z < 0.0f ? ior : 1 / ior;
		float cosThetaT;
		const float F = fresnel::DielectricReflectance(eta, abs(si.wi.z), cosThetaT);

		if (nextRand(seed) <= F)
		{
			// Reflect
			bs.wo = float3(-si.wi.x, -si.wi.y, si.wi.z);
			bs.pdf = F;
			bs.weight = EvalSpecularReflectance(mat, si);
		}
		else
		{
			// Refract
			bs.wo = float3(-si.wi.x * eta, -si.wi.y * eta, -sign(si.wi.z) * cosThetaT);
			bs.pdf = 1 - F;
			bs.weight = EvalSpecularTransmittance(mat, si) * eta * eta;
		}
	}
}
#endif