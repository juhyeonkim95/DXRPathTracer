#ifndef BSDF_CONDUCTOR
#define BSDF_CONDUCTOR

#include "../stdafx.hlsli"

namespace conductor
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
		if (si.wi.z <= 0.0f) {
			bs.pdf = 0.0f;
			bs.weight = 0.0f;
			return;
		}
		bs.wo = float3(-si.wi.x, -si.wi.y, si.wi.z);
		bs.pdf = 0.0f;
		bs.weight = si.specularReflectance * fresnel::ConductorReflectance(mat.eta, mat.k, si.wi.z);
		bs.sampledLobe = BSDF_LOBE_DELTA_REFLECTION;
	}
}
#endif