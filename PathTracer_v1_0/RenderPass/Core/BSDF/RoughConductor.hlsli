#ifndef BSDF_ROUGHCONDUCTOR
#define BSDF_ROUGHCONDUCTOR

#include "../stdafx.hlsli"

namespace roughconductor
{
	float Pdf(in Material mat, in RayPayload si, in float3 wo) {
		const float3 wi = si.wi;
		if (wi.z <= 0.0f || wo.z <= 0.0f) {
			return 0.0f;
		}
		float roughness = EvalRoughness(mat, si);
		float3 specularReflectance = EvalSpecularReflectance(mat, si);

		uint dist = mat.microfacetDistribution;
		float sampleRoughness = EvalRoughness(mat, si);
		float sampleAlpha = microfacet::roughnessToAlpha(dist, sampleRoughness);

		float3 hr = normalize(wo + wi);
		float mPdf = microfacet::Pdf(dist, sampleAlpha, hr);
		float pdf = mPdf * 0.25f / dot(wi, hr);
		return pdf;
	}

	float3 Eval(in Material mat, in RayPayload si, in float3 wo) {
		const float3 wi = si.wi;
		if (wi.z <= 0.0f || wo.z <= 0.0f) {
			return float3(0, 0, 0);
		}
		float roughness = EvalRoughness(mat, si);
		float3 specularReflectance = EvalSpecularReflectance(mat, si);

		uint dist = mat.microfacetDistribution;
		float alpha = microfacet::roughnessToAlpha(dist, roughness);

		float3 hr = normalize(wo + wi);
		float cosThetaM = dot(wi, hr);
		float3 F = fresnel::ConductorReflectance(mat.eta, mat.k, cosThetaM);
		float G = microfacet::G(dist, alpha, wi, wo, hr);
		float D = microfacet::D(dist, alpha, hr);
		float fr = (G * D * 0.25f) / wi.z;

		return specularReflectance * F * fr;

	}

	void Sample(in Material mat, in RayPayload si, inout uint seed, inout BSDFSample bs) {
		const float3 specularReflectance = EvalSpecularReflectance(mat, si);
		const float roughness = EvalRoughness(mat, si);
		const float3 wi = si.wi;

		bs.pdf = 0.0f;
		bs.weight = 0.0f;
		if (si.wi.z < 0)
			return;

		uint dist = mat.microfacetDistribution;

		const float alpha = microfacet::roughnessToAlpha(mat.microfacetDistribution, roughness);
		const float sampleAlpha = microfacet::roughnessToAlpha(mat.microfacetDistribution, roughness);

		float r1 = nextRand(seed);
		float r2 = nextRand(seed);

		const float3 m = microfacet::Sample(dist, sampleAlpha, r1, r2);
		float wiDotM = dot(wi, m);
		float3 wo = 2.0f * wiDotM * m - wi;
		if (wiDotM <= 0.0f || wo.z <= 0.0f)
			return;

		float G = microfacet::G(dist, alpha, wi, wo, m);
		float D = microfacet::D(dist, alpha, m);
		float mPdf = microfacet::Pdf(dist, sampleAlpha, m);
		float pdf = mPdf * 0.25f / wiDotM;
		float weight = wiDotM * G * D / (wi.z * mPdf);
		float3 F = fresnel::ConductorReflectance(mat.eta, mat.k, wiDotM);

		bs.wo = wo;
		bs.pdf = pdf;
		bs.weight = specularReflectance * F * weight;
	}
}
#endif