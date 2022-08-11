#ifndef BSDF_DIFFUSE
#define BSDF_DIFFUSE

#include "../stdafx.hlsli"

namespace diffuse
{
	float Pdf(in Material mat, in RayPayload si, in float3 wo) {
		if (si.wi.z <= 0.0f || wo.z <= 0.0f)
			return 0.0f;
		return wo.z * M_1_PIf;
	}

	float3 Eval(in Material mat, in RayPayload si, in float3 wo) {
		if (si.wi.z <= 0.0f || wo.z <= 0.0f)
			return float3(0, 0, 0);
		return si.diffuseReflectance * wo.z * M_1_PIf;
	}

	float4 EvalAndPdf(in Material mat, in RayPayload si, in float3 wo) {
		if (si.wi.z <= 0.0f || wo.z <= 0.0f)
			return float4(0, 0, 0, 0);
		const float pdf = wo.z * M_1_PIf;
		const float3 f = si.diffuseReflectance * pdf;
		return float4(f, pdf);
	}

	void Sample(in Material mat, in RayPayload si, inout uint seed, inout BSDFSample bs) {
		if (si.wi.z <= 0.0f) {
			bs.pdf = 0.0f;
			bs.weight = 0.0f;
			return;
		}
		bs.wo = getCosHemisphereSampleLocal(seed);
		bs.weight = si.diffuseReflectance;
		bs.pdf = bs.wo.z * M_1_PIf;
	}
}
#endif