#ifndef BSDF_PLASTIC
#define BSDF_PLASTIC

#include "../stdafx.hlsli"
#include "BSDFUtils.hlsli"

namespace plastic
{
	float getProbSpecular(in Material mat, in float3 diffuseReflectance, float Fi)
	{
		float s_mean = 1.0f;
		float d_mean = (diffuseReflectance.x + diffuseReflectance.y + diffuseReflectance.z) / 3.0f;
		float m_specular_sampling_weight = s_mean / (d_mean + s_mean);

		float probSpecular = Fi * m_specular_sampling_weight;
		float probDiffuse = (1 - Fi) * (1 - m_specular_sampling_weight);

		probSpecular = probSpecular / (probSpecular + probDiffuse);
		return probSpecular;
	}

	float Pdf(in Material mat, in RayPayload si, in float3 wo) {
		const float3 wi = si.wi;

		if (wo.z < 0 || wi.z < 0)
		{
			return 0.0f;
		}
		bool sampleR = true;
		bool sampleT = true;
		
		if (sampleR && sampleT) {
			float3 diffuseReflectance = si.diffuseReflectance;

			float ior = mat.intIOR / mat.extIOR;
			float eta = 1 / ior;
			float Fi = fresnel::DielectricReflectance(eta, wi.z);

			float probSpecular = getProbSpecular(mat, diffuseReflectance, Fi);


			if (checkReflectionConstraint(wi, wo)) {
				return probSpecular;
			}
			else
			{
				return wo.z * M_1_PIf * (1.0f - probSpecular);
			}
		}
		else if (sampleT) {
			return wo.z * M_1_PIf;
		}
		else if (sampleR)
		{
			return checkReflectionConstraint(wi, wo) ? 1.0f : 0.0f;
		}
		return 0.0f;
	}

	float3 Eval(in Material mat, in RayPayload si, in float3 wo, bool evalR, bool evalT) {
		const float3 wi = si.wi;

		if (wo.z < 0 || wi.z < 0) 
		{
			return float3(0, 0, 0);
		}

		float ior = mat.intIOR / mat.extIOR;
		float eta = 1 / ior;
		float Fi = fresnel::DielectricReflectance(eta, wi.z);
		float Fo = fresnel::DielectricReflectance(eta, wo.z);

		if (evalR && checkReflectionConstraint(wi, wo)) 
		{
			return float3(Fi, Fi, Fi);
		}
		else if(evalT)
		{
			float3 diffuseReflectance = si.diffuseReflectance;

			float diffuseFresnel = fresnel::DiffuseFresnel(eta);
			float3 value = diffuseReflectance;
			value = value / (1 - (mat.nonlinear ? value * diffuseFresnel : (diffuseFresnel)));
			value *= ((1.0f - Fi) * (1.0f - Fo) * eta * eta * wo.z * M_1_PIf);

			return value;
		}
		return float3(0, 0, 0);
	}



	void Sample(in Material mat, in RayPayload si, inout uint seed, inout BSDFSample bs) {
		const float3 wi = si.wi;

		if (wi.z < 0) {
			bs.pdf = 0.0f;
			bs.weight = 0.0f;
			return;
		}

		bool sampleR = si.requestedLobe & BSDF_LOBE_DELTA_REFLECTION;
		bool sampleT = si.requestedLobe & BSDF_LOBE_DIFFUSE_REFLECTION;

		float3 diffuseReflectance = si.diffuseReflectance;

		float ior = mat.intIOR / mat.extIOR;
		float eta = 1 / ior;
		float Fi = fresnel::DielectricReflectance(eta, wi.z);

		float probSpecular;
		if (sampleR && sampleT)
			probSpecular = getProbSpecular(mat, diffuseReflectance, Fi);
		else if (sampleR)
			probSpecular = 1.0f;
		else if (sampleT)
			probSpecular = 0.0f;
		else
			return;

		float probDiffuse = 1.f - probSpecular;

		float diffuseFresnel = fresnel::DiffuseFresnel(eta);
		if (sampleR && nextRand(seed) <= probSpecular)
		{
			// Reflect
			bs.wo = float3(-wi.x, -wi.y, wi.z);
			bs.pdf = probSpecular;
			bs.weight = (Fi / probSpecular);
			bs.sampledLobe = BSDF_LOBE_DELTA_REFLECTION;
		}
		else
		{
			bs.wo = getCosHemisphereSampleLocal(seed);
			float Fo = fresnel::DielectricReflectance(eta, bs.wo.z);

			float3 value = diffuseReflectance;
			value = value / (1 - (mat.nonlinear ? value * diffuseFresnel : (diffuseFresnel)));
			value *= ((1.0f - Fi) * (1.0f - Fo) * eta * eta);
			value = value / probDiffuse;

			bs.pdf = bs.wo.z * probDiffuse;
			bs.weight = value;
			bs.sampledLobe = BSDF_LOBE_DIFFUSE_REFLECTION;
		}
		return;
	}
}
#endif