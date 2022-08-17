#ifndef BSDF_PLASTIC_HLSLI
#define BSDF_PLASTIC_HLSLI

#include "../stdafx.hlsli"
#include "BSDFUtils.hlsli"

namespace plastic
{
	float getProbSpecular(in Material mat, in float3 diffuseReflectance, in float3 specularReflectance, float Fi)
	{
		float specularMean = (specularReflectance.x + specularReflectance.y + specularReflectance.z) / 3.0f;
		float diffuseMean = (diffuseReflectance.x + diffuseReflectance.y + diffuseReflectance.z) / 3.0f;
		float specularSamplingWeight = specularMean / (diffuseMean + specularMean);

		float probSpecular = Fi * specularSamplingWeight;
		float probDiffuse = (1 - Fi) * (1 - specularSamplingWeight);

		probSpecular = probSpecular / (probSpecular + probDiffuse);
		return probSpecular;
	}

	float Pdf(in Material mat, in RayPayload si, in float3 wo, bool sampleR, bool sampleT) {
		const float3 wi = si.wi;

		if (wo.z < 0 || wi.z < 0)
		{
			return 0.0f;
		}
		if (sampleR && sampleT) {
			float ior = mat.intIOR / mat.extIOR;
			float eta = 1 / ior;
			float Fi = fresnel::DielectricReflectance(eta, wi.z);

			float probSpecular = getProbSpecular(mat, si.diffuseReflectance, si.specularReflectance, Fi);

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

	float3 Eval(in Material mat, in RayPayload si, in float3 wo, bool sampleR, bool sampleT) {
		const float3 wi = si.wi;

		if (wo.z < 0 || wi.z < 0) 
		{
			return float3(0, 0, 0);
		}

		float ior = mat.intIOR / mat.extIOR;
		float eta = 1 / ior;
		float Fi = fresnel::DielectricReflectance(eta, wi.z);
		float Fo = fresnel::DielectricReflectance(eta, wo.z);

		if (sampleR && checkReflectionConstraint(wi, wo))
		{
			return float3(Fi, Fi, Fi) * si.specularReflectance;
		}
		else if(sampleT)
		{
			float3 diffuseReflectance = si.diffuseReflectance;

			float diffuseFresnel = mat.diffuseFresnel;// fresnel::DiffuseFresnel(eta);
			float3 value = diffuseReflectance;
			value = value / (1 - (mat.nonlinear ? value * diffuseFresnel : (diffuseFresnel)));
			value *= ((1.0f - Fi) * (1.0f - Fo) * eta * eta * wo.z * M_1_PIf);

			return value;
		}
		return float3(0, 0, 0);
	}

	float4 EvalAndPdf(in Material mat, in RayPayload si, in float3 wo, bool sampleR, bool sampleT) {
		const float3 wi = si.wi;

		if (wo.z < 0 || wi.z < 0)
		{
			return float4(0, 0, 0, 0);
		}

		float ior = mat.intIOR / mat.extIOR;
		float eta = 1 / ior;
		float Fi = fresnel::DielectricReflectance(eta, wi.z);
		// float Fo = fresnel::DielectricReflectance(eta, wo.z);

		float3 bsdf;
		if (sampleR && checkReflectionConstraint(wi, wo))
		{
			bsdf = float3(Fi, Fi, Fi) * si.specularReflectance;
		}
		else if (sampleT)
		{
			float3 diffuseReflectance = si.diffuseReflectance;
			float Fo = fresnel::DielectricReflectance(eta, wo.z);
			float diffuseFresnel = mat.diffuseFresnel;// fresnel::DiffuseFresnel(eta);
			float3 value = diffuseReflectance;
			value = value / (1 - (mat.nonlinear ? value * diffuseFresnel : (diffuseFresnel)));
			value *= ((1.0f - Fi) * (1.0f - Fo) * eta * eta * wo.z * M_1_PIf);

			bsdf = value;
		}
		float pdf;
		if (sampleR && sampleT) {
			
			float probSpecular = getProbSpecular(mat, si.diffuseReflectance, si.specularReflectance, Fi);

			if (checkReflectionConstraint(wi, wo)) {
				pdf = probSpecular;
			}
			else
			{
				pdf = wo.z * M_1_PIf * (1.0f - probSpecular);
			}
		}
		else if (sampleT) {
			pdf = wo.z * M_1_PIf;
		}
		else if (sampleR)
		{
			pdf = checkReflectionConstraint(wi, wo) ? 1.0f : 0.0f;
		}
		return float4(bsdf, pdf);
	}




	void Sample(in Material mat, in RayPayload si, inout uint seed, out BSDFSample bs) {
		const float3 wi = si.wi;

		if (wi.z < 0) {
			bs.pdf = 0.0f;
			bs.weight = 0.0f;
			return;
		}

		bool sampleR = si.requestedLobe & BSDF_LOBE_DELTA_REFLECTION;
		bool sampleT = si.requestedLobe & BSDF_LOBE_DIFFUSE_REFLECTION;

		const float3 diffuseReflectance = si.diffuseReflectance;
		const float3 specularReflectance = si.specularReflectance;

		float ior = mat.intIOR / mat.extIOR;
		float eta = 1 / ior;
		float Fi = fresnel::DielectricReflectance(eta, wi.z);

		float probSpecular;
		if (sampleR && sampleT)
			probSpecular = getProbSpecular(mat, diffuseReflectance, specularReflectance, Fi);
		else if (sampleR)
			probSpecular = 1.0f;
		else if (sampleT)
			probSpecular = 0.0f;
		else
			return;

		float probDiffuse = 1.f - probSpecular;

		float diffuseFresnel = mat.diffuseFresnel;// fresnel::DiffuseFresnel(eta);
		if (sampleR && nextRand(seed) <= probSpecular)
		{
			// Reflect
			bs.wo = float3(-wi.x, -wi.y, wi.z);
			bs.pdf = probSpecular;
			bs.weight = (Fi / probSpecular) * specularReflectance;
			bs.sampledLobe = BSDF_LOBE_DELTA_REFLECTION;
		}
		else
		{
			// Refract
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