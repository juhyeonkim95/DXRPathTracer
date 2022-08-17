#ifndef BSDF_ROUGH_PLASTIC_HLSLI
#define BSDF_ROUGH_PLASTIC_HLSLI

#include "../stdafx.hlsli"
#include "roughdielectric.hlsli"
#include "plastic.hlsli"

namespace roughplastic
{
	float Pdf(in Material mat, in RayPayload si, in float3 wo, bool sampleR, bool sampleT) {
		const float3 wi = si.wi;
		if (wi.z <= 0 || wo.z <= 0)
		{
			return 0;
		}

		if (!sampleR && !sampleT)
			return 0;

		float specularPdf = sampleR ? roughdielectric::PdfBase(mat, true, false, si, wo) : 0.0f;
		float diffusePdf = sampleT ? wo.z * M_1_PIf : 0.0f;
		
		if (sampleT && sampleR)
		{
			float ior = mat.intIOR / mat.extIOR;
			float eta = 1 / ior;
			float Fi = fresnel::DielectricReflectance(eta, wi.z);
			float probSpecular = plastic::getProbSpecular(mat, si.diffuseReflectance, si.specularReflectance, Fi);


			diffusePdf *= (1.0 - probSpecular);
			specularPdf *= probSpecular;
		}
		
		return diffusePdf + specularPdf;
	}

	float3 Eval(in Material mat, in RayPayload si, in float3 wo, bool sampleR, bool sampleT) {
		const float3 wi = si.wi;
		if (wi.z <= 0 || wo.z <= 0) 
		{
			return float3(0, 0, 0);
		}

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
			float diffuseFresnel = mat.diffuseFresnel; // fresnel::DiffuseFresnel(eta);
			float3 temp = (mat.nonlinear ? diffuseReflectance * diffuseFresnel : float3(diffuseFresnel, diffuseFresnel, diffuseFresnel));
			diffuseR = ((1.0f - Fi) * (1.0f - Fo) * eta * eta) * (diffuseReflectance / (1.0f - temp));
			diffuseR *= wo.z * M_1_PIf;
		}
		return glossyR + diffuseR;
	}

	float4 EvalAndPdf(in Material mat, in RayPayload si, in float3 wo, bool sampleR, bool sampleT) {
		const float3 wi = si.wi;
		if (wi.z <= 0 || wo.z <= 0)
		{
			return float4(0, 0, 0, 0);
		}

		if (!sampleR && !sampleT)
			return float4(0, 0, 0, 0);

		float3 specularBSDF = 0.0f;
		float specularPdf = 0.0f;
		if (sampleR)
		{
			float4 specularBSDFPdf = roughdielectric::EvalAndPdfBase(mat, true, false, si, wo);
			specularBSDF = specularBSDFPdf.xyz;
			specularPdf = specularBSDFPdf.w;
		}

		float3 diffuseBSDF = 0.0f;
		float diffusePdf = 0.0f;
		if (sampleT)
		{
			float ior = mat.intIOR / mat.extIOR;
			float eta = 1 / ior;
			float Fi = fresnel::DielectricReflectance(eta, wi.z);
			float Fo = fresnel::DielectricReflectance(eta, wo.z);

			float3 diffuseReflectance = si.diffuseReflectance;
			float diffuseFresnel = mat.diffuseFresnel; // fresnel::DiffuseFresnel(eta);
			float3 temp = (mat.nonlinear ? diffuseReflectance * diffuseFresnel : float3(diffuseFresnel, diffuseFresnel, diffuseFresnel));
			diffuseBSDF = ((1.0f - Fi) * (1.0f - Fo) * eta * eta) * (diffuseReflectance / (1.0f - temp));
			diffuseBSDF *= wo.z * M_1_PIf;
			diffusePdf = wo.z * M_1_PIf;

			if (sampleR)
			{
				float probSpecular = plastic::getProbSpecular(mat, si.diffuseReflectance, si.specularReflectance, Fi);
				diffusePdf *= (1.0 - probSpecular);
				specularPdf *= probSpecular;
			}
		}

		return float4(diffuseBSDF + specularBSDF, diffusePdf + specularPdf);
	}


	void Sample(in Material mat, in RayPayload si, inout uint seed, out BSDFSample bs) {
		const float3 wi = si.wi;

		if (wi.z < 0) {
			bs.pdf = 0.0f;
			bs.weight = 0.0f;
			return;
		}

		bool sampleR = si.requestedLobe & BSDF_LOBE_GLOSSY_REFLECTION;
		bool sampleT = si.requestedLobe & BSDF_LOBE_DIFFUSE_REFLECTION;

		if (!sampleR && !sampleT)
			return;

		const float3 diffuseReflectance = si.diffuseReflectance;
		const float3 specularReflectance = si.specularReflectance;

		float ior = mat.intIOR / mat.extIOR;
		float eta = 1 / ior;
		float Fi = fresnel::DielectricReflectance(eta, wi.z);
		float probSpecular = plastic::getProbSpecular(mat, diffuseReflectance, specularReflectance, Fi);

		float diffuseFresnel = mat.diffuseFresnel; //fresnel::DiffuseFresnel(eta);

		if ((sampleR && (nextRand(seed) <= probSpecular)) || !sampleT)
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

			bs.sampledLobe = BSDF_LOBE_GLOSSY_REFLECTION;
		}
		else
		{
			// Refract

			bs.wo = getCosHemisphereSampleLocal(seed);
			float Fo = fresnel::DielectricReflectance(eta, bs.wo.z);
			float3 temp = (mat.nonlinear ? diffuseReflectance * diffuseFresnel : float3(diffuseFresnel, diffuseFresnel, diffuseFresnel));

			bs.weight = ((1.0f - Fi) * (1.0f - Fo) * eta * eta) * (diffuseReflectance / (1.0f - temp));
			bs.pdf = bs.wo.z * M_1_PIf;
			if (sampleR) 
			{
				float3 brdfSubstrate = bs.weight * bs.pdf;
				float  pdfSubstrate = bs.pdf * (1.0f - probSpecular);
				float4 brdfPdfSpecular = roughdielectric::EvalAndPdfBase(mat, true, false, si, bs.wo);
				float3 brdfSpecular = brdfPdfSpecular.xyz;
				float pdfSpecular = brdfPdfSpecular.w;

				//float3 brdfSpecular = roughdielectric::EvalBase(mat, true, false, si, bs.wo);
				//float pdfSpecular = roughdielectric::PdfBase(mat, true, false, si, bs.wo);
				pdfSpecular *= probSpecular;

				bs.weight = (brdfSpecular + brdfSubstrate) / (pdfSpecular + pdfSubstrate);
				bs.pdf = pdfSpecular + pdfSubstrate;
			}

			bs.sampledLobe = BSDF_LOBE_DIFFUSE_REFLECTION;
		}
		return;
	}
}
#endif