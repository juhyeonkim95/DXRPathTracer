#ifndef BSDF_ROUGH_PLASTIC
#define BSDF_ROUGH_PLASTIC

#include "../stdafx.hlsli"
#include "roughdielectric.hlsli"
#include "plastic.hlsli"

namespace roughplastic
{
	float Pdf(in Material mat, in RayPayload si, in float3 wo) {
		const float3 wi = si.wi;
		if (wi.z <= 0 || wo.z <= 0)
		{
			return 0;
		}
		bool sampleR = true;
		bool sampleT = true;
		if (!sampleR && !sampleT)
			return 0;

		float specularPdf = sampleR ? roughdielectric::PdfBase(mat, true, false, si, wo) : 0.0f;
		float diffusePdf = sampleT ? wo.z * M_1_PIf : 0.0f;
		
		if (sampleT && sampleR)
		{
			float3 diffuseReflectance = si.diffuseReflectance;
			float ior = mat.intIOR / mat.extIOR;
			float eta = 1 / ior;
			float Fi = fresnel::DielectricReflectance(eta, wi.z);
			float probSpecular = plastic::getProbSpecular(mat, diffuseReflectance, Fi);


			diffusePdf *= (1.0 - probSpecular);
			specularPdf *= probSpecular;
		}
		
		return diffusePdf + specularPdf;
	}

	float3 Eval(in Material mat, in RayPayload si, in float3 wo) {
		const float3 wi = si.wi;
		if (wi.z <= 0 || wo.z <= 0) 
		{
			return float3(0, 0, 0);
		}
		bool sampleR = true;
		bool sampleT = true;
		if (!sampleR && !sampleT)
			return float3(0, 0, 0);

		float3 glossyR = 0.0f;
		if(sampleR)
			glossyR = roughdielectric::EvalBase(mat, true, false, si, wo);
		
		float3 diffuseR = 0.0f;
		if (sampleT) 
		{
			float ior = mat.intIOR / mat.extIOR;
			float eta = 1 / ior;
			float Fi = fresnel::DielectricReflectance(eta, wi.z);
			float Fo = fresnel::DielectricReflectance(eta, wo.z);

			float3 diffuseReflectance = si.diffuseReflectance;
			float diffuseFresnel = fresnel::DiffuseFresnel(eta);
			float3 temp = (mat.nonlinear ? diffuseReflectance * diffuseFresnel : float3(diffuseFresnel, diffuseFresnel, diffuseFresnel));
			diffuseR = ((1.0f - Fi) * (1.0f - Fo) * eta * eta) * (diffuseReflectance / (1.0f - temp));
			diffuseR *= wo.z * M_1_PIf;
		}
		return glossyR + diffuseR;
	}

	void Sample(in Material mat, in RayPayload si, inout uint seed, inout BSDFSample bs) {
		const float3 wi = si.wi;

		if (wi.z < 0) {
			bs.pdf = 0.0f;
			bs.weight = 0.0f;
			return;
		}

		bool sampleR = true;
		bool sampleT = true;
		if (!sampleR && !sampleT)
			return;

		float3 diffuseReflectance = si.diffuseReflectance;
		float ior = mat.intIOR / mat.extIOR;
		float eta = 1 / ior;
		float Fi = fresnel::DielectricReflectance(eta, wi.z);
		float probSpecular = plastic::getProbSpecular(mat, diffuseReflectance, Fi);

		float diffuseFresnel = fresnel::DiffuseFresnel(eta);

		if (sampleR && nextRand(seed) <= probSpecular || !sampleT)
		{
			// Reflect
			roughdielectric::SampleBase(mat, true, false, si, seed, bs);
			if (bs.pdf == 0) {
				return;
			}

			if (sampleT) 
			{
				float Fo = fresnel::DielectricReflectance(eta, bs.wo.z);

				float3 temp = (mat.nonlinear ? diffuseReflectance * diffuseFresnel : float3(diffuseFresnel, diffuseFresnel, diffuseFresnel));

				float3 brdfSubstrate = ((1.0f - Fi) * (1.0f - Fo) * eta * eta)
					* (diffuseReflectance / (1.0f - temp)) * M_1_PIf * bs.wo.z;
				float3 brdfSpecular = bs.weight * bs.pdf;
				float pdfSubstrate = bs.wo.z * M_1_PIf * (1.0f - probSpecular);
				float pdfSpecular = bs.pdf * probSpecular;

				bs.weight = (brdfSpecular + brdfSubstrate) / (pdfSpecular + pdfSubstrate);
				bs.pdf = pdfSpecular + pdfSubstrate;
			}
		}
		else
		{
			bs.wo = getCosHemisphereSampleLocal(seed);
			float Fo = fresnel::DielectricReflectance(eta, bs.wo.z);
			float3 temp = (mat.nonlinear ? diffuseReflectance * diffuseFresnel : float3(diffuseFresnel, diffuseFresnel, diffuseFresnel));
			bs.weight = ((1.0f - Fi) * (1.0f - Fo) * eta * eta) * (diffuseReflectance / (1.0f - temp));
			bs.pdf = bs.wo.z * M_1_PIf;
			if (sampleR) 
			{
				float3 brdfSubstrate = bs.weight * bs.pdf;
				float  pdfSubstrate = bs.pdf * (1.0f - probSpecular);
				float3 brdfSpecular = roughdielectric::EvalBase(mat, true, false, si, bs.wo);
				float pdfSpecular = roughdielectric::PdfBase(mat, true, false, si, bs.wo);
				pdfSpecular *= probSpecular;

				bs.weight = (brdfSpecular + brdfSubstrate) / (pdfSpecular + pdfSubstrate);
				bs.pdf = pdfSpecular + pdfSubstrate;
			}
		}
		return;
	}
}
#endif