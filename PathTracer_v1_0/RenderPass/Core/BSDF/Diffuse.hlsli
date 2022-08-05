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
		// TODO : change this!!
		return si.attenuation * wo.z * M_1_PIf;
	}

	void Sample(in Material mat, in RayPayload si, inout uint seed, inout BSDFSample bs) {
		if (si.wi.z < 0) {
			bs.pdf = 0.0f;
			bs.weight = 0.0f;
			return;
		}
		bs.wo = getCosHemisphereSampleLocal(seed);
		bs.weight = EvalDiffuseReflectance(mat, si);
		bs.pdf = bs.wo.z * M_1_PIf;
	}
}
#endif