#ifndef BSDF_ROUGHDIELECTRIC
#define BSDF_ROUGHDIELECTRIC

#include "../stdafx.hlsli"

namespace roughdielectric
{
	float PdfBase(in Material mat, bool sampleR, bool sampleT, in RayPayload si, in float3 wo) {
		const float3 wi = si.wi;

		float ior = mat.intIOR / mat.extIOR;

		uint dist = mat.microfacetDistribution;

		float wiDotN = wi.z;
		float woDotN = wo.z;
		bool reflect = wiDotN * woDotN >= 0.0f;

		float sampleRoughness = (1.2f - 0.2f * sqrt(abs(wiDotN))) * EvalRoughness(mat, si);
		float sampleAlpha = microfacet::roughnessToAlpha(dist, sampleRoughness);


		float eta = wiDotN < 0.0f ? ior : 1.0f / ior;
		float3 m;
		if (reflect)
			m = sgnE(wiDotN) * normalize(wi + wo);
		else
			m = -normalize(wi * eta + wo);

		float wiDotM = dot(wi, m);
		float woDotM = dot(wo, m);
		float F = fresnel::DielectricReflectance(1.0f / ior, wiDotM);
		float pm = microfacet::Pdf(dist, sampleAlpha, m);

		float pdf;
		if (reflect) {
			pdf = pm * 0.25f / abs(wiDotM);
		}
		else {
			pdf = pm * abs(woDotM) / sqr(eta * wiDotM + woDotM);
		}
		if (sampleR && sampleT) {
			if (reflect)
				pdf *= F;
			else
				pdf *= 1.0f - F;
		}
		return pdf;
	}

	float Pdf(in Material mat, in RayPayload si, in float3 wo) {
		return PdfBase(mat, true, true, si, wo);
	}

	float3 Eval(in Material mat, in RayPayload si, in float3 wo) {
		const float3 wi = si.wi;
		float ior = mat.intIOR / mat.extIOR;
		float alpha = EvalRoughness(mat, si);
		uint dist = mat.microfacetDistribution;

		float wiDotN = wi.z;
		float woDotN = wo.z;
		bool reflect = wiDotN * woDotN >= 0.0f;

		float eta = wiDotN < 0.0f ? ior : 1.0f / ior;
		float3 m;
		if (reflect)
			m = sgnE(wiDotN) * normalize(wi + wo);
		else
			m = -normalize(wi * eta + wo);
		float wiDotM = dot(wi, m);
		float woDotM = dot(wo, m);
		float F = fresnel::DielectricReflectance(1.0f / ior, wiDotM);
		float G = microfacet::G(dist, alpha, wi, wo, m);
		float D = microfacet::D(dist, alpha, m);

		if (reflect) {
			float fr = (F * G * D * 0.25f) / abs(wiDotN);
			return EvalSpecularReflectance(mat, si) * fr;
		}
		else {
			float fs = abs(wiDotM * woDotM) * (1.0f - F) * G * D / (sqr(eta * wiDotM + woDotM) * abs(wiDotN));
			return EvalSpecularTransmittance(mat, si) * fs;
		}
	}

	void SampleBase(in Material mat, bool sampleR, bool sampleT, in RayPayload si, inout uint seed, inout BSDFSample bs) {
		float roughness = EvalRoughness(mat, si);
		float3 specularReflectance = EvalSpecularReflectance(mat, si);
		float3 specularTransmittance = EvalSpecularTransmittance(mat, si);

		const float3 wi = si.wi;
		bs.pdf = 0.0f;
		bs.weight = 0.0f;

		float wiDotN = wi.z;

		float ior = mat.intIOR / mat.extIOR;
		float eta = si.wi.z < 0.0f ? ior : 1 / ior;

		float r1 = nextRand(seed);
		float r2 = nextRand(seed);
		uint dist = mat.microfacetDistribution;

		float sampleRoughness = (1.2f - 0.2f * sqrt(abs(wiDotN))) * roughness;
		float alpha = microfacet::roughnessToAlpha(dist, roughness);
		float sampleAlpha = microfacet::roughnessToAlpha(dist, sampleRoughness);

		float3 m = microfacet::Sample(dist, sampleAlpha, r1, r2);
		float pm = microfacet::Pdf(dist, sampleAlpha, m);

		float wiDotM = dot(wi, m);

		float cosThetaT;
		const float F = fresnel::DielectricReflectance(eta, abs(si.wi.z), cosThetaT);

		float etaM = wiDotM < 0.0f ? ior : 1.0f / ior;


		float3 wo;
		bool reflect;
		if (sampleR && sampleT) {
			reflect = nextRand(seed) < F;
		}
		else if (sampleT) {
			reflect = false;
		}
		else if (sampleR) {
			reflect = true;
		}
		else {
			return;
		}

		if (reflect)
		{
			wo = 2.0f * wiDotM * m - wi;
		}
		else
		{
			wo = (etaM * wiDotM - (wiDotM > 0 ? 1 : -1) * cosThetaT) * m - etaM * wi;
		}

		float woDotN = wo.z;
		bool reflected = wiDotN * woDotN > 0.0f;
		if (reflected != reflect)
			return;

		float woDotM = dot(wo, m);
		float G = microfacet::G(dist, alpha, wi, wo, m);
		float D = microfacet::D(dist, alpha, m);
		float weightMultiplier = abs(wiDotM) * G * D / (abs(wiDotN) * pm);
		bs.wo = wo;

		if (reflect) {
			bs.pdf = pm * 0.25f / abs(wiDotM);
			bs.weight = specularReflectance * weightMultiplier;
		}
		else {
			float denom = (eta * wiDotM + woDotM);
			bs.pdf = pm * abs(woDotM) / (denom * denom);
			bs.weight = specularTransmittance * weightMultiplier * eta * eta;
		}

		if (sampleR && sampleT) {
			if (reflect)
				bs.pdf *= F;
			else
				bs.pdf *= 1.0f - F;
		}
		else {
			if (reflect)
				bs.weight *= F;
			else
				bs.weight *= 1.0f - F;
		}


		return;
	}

	void Sample(in Material mat, in RayPayload si, inout uint seed, inout BSDFSample bs) {
		SampleBase(mat, true, true, si, seed, bs);
		return;
	}
}
#endif