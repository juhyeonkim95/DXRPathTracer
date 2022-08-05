#ifndef BSDF_PLASTIC
#define BSDF_PLASTIC

#include "../stdafx.hlsli"

namespace plastic
{
	float Pdf(in Material material, in RayPayload payload, in float3 wo) {
		return 0.0f;
	}

	float3 Eval(in Material material, in RayPayload payload, in float3 wo) {
		return float3(0, 0, 0);
	}

	void Sample(in Material mat, in RayPayload si, inout uint seed, inout BSDFSample bs) {
		const float3 wi = si.wi;

		if (wi.z < 0) {
			bs.pdf = 0.0f;
			bs.weight = 0.0f;
			return;
		}

		float3 diffuse_reflectance = EvalDiffuseReflectance(mat, si);

		float ior = mat.intIOR / mat.extIOR;
		float eta = 1 / ior;
		float Fi = fresnel::DielectricReflectance(eta, wi.z);

		float s_mean = 1.0f;
		float d_mean = (diffuse_reflectance.x + diffuse_reflectance.y + diffuse_reflectance.z) / 3.0f;
		float m_specular_sampling_weight = s_mean / (d_mean + s_mean);

		float prob_specular = Fi * m_specular_sampling_weight;
		float prob_diffuse = (1 - Fi) * (1 - m_specular_sampling_weight);

		prob_specular = prob_specular / (prob_specular + prob_diffuse);
		prob_diffuse = 1.f - prob_specular;

		float m_fdr_int = fresnel::DiffuseFresnel(eta);
		if (nextRand(seed) <= prob_specular)
		{
			// Reflect
			bs.wo = float3(-wi.x, -wi.y, wi.z);
			bs.pdf = prob_specular;
			bs.weight = (Fi / prob_specular);
		}
		else
		{
			bs.wo = getCosHemisphereSampleLocal(seed);
			float Fo = fresnel::DielectricReflectance(eta, bs.wo.z);

			float3 value = diffuse_reflectance;
			value = value / (1 - (mat.nonlinear ? value * m_fdr_int : (m_fdr_int)));
			value *= ((1.0f - Fi) * (1.0f - Fo) * eta * eta);
			value = value / prob_diffuse;

			bs.pdf = bs.wo.z * prob_diffuse;
			bs.weight = value;
		}
		return;
	}
}
#endif