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
		float roughness = si.roughness;
		float sampleRoughness = roughness;

		uint dist = mat.microfacetDistribution;
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
		float roughness = si.roughness;
		float3 specularReflectance = si.specularReflectance;

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

	float4 EvalAndPdf(in Material mat, in RayPayload si, in float3 wo) {
		const float3 wi = si.wi;
		if (wi.z <= 0.0f || wo.z <= 0.0f) {
			return float4(0, 0, 0, 0);
		}
		uint dist = mat.microfacetDistribution;
		float roughness = si.roughness;
		float sampleRoughness = roughness;
		float alpha = microfacet::roughnessToAlpha(dist, roughness);
		float sampleAlpha = microfacet::roughnessToAlpha(dist, sampleRoughness);

		float3 hr = normalize(wo + wi);
		float cosThetaM = dot(wi, hr);
		float3 F = fresnel::ConductorReflectance(mat.eta, mat.k, cosThetaM);
		float G = microfacet::G(dist, alpha, wi, wo, hr);
		float D = microfacet::D(dist, alpha, hr);
		float fr = (G * D * 0.25f) / wi.z;

		float3 f = si.specularReflectance * F * fr;

		float mPdf = microfacet::Pdf(dist, sampleAlpha, hr);
		float pdf = mPdf * 0.25f / dot(wi, hr);

		return float4(f, pdf);
	}

	void Sample(in Material mat, in RayPayload si, inout uint seed, inout BSDFSample bs) {
		bs.pdf = 0.0f;
		bs.weight = 0.0f;
		if (si.wi.z <= 0.0f)
			return;
		
		const float roughness = si.roughness;
		const float3 wi = si.wi;

		uint dist = mat.microfacetDistribution;

		const float alpha = microfacet::roughnessToAlpha(mat.microfacetDistribution, roughness);
		const float sampleAlpha = alpha;

		const float3 m = microfacet::Sample(dist, sampleAlpha, nextRand(seed), nextRand(seed));
		const float wiDotM = dot(wi, m);

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
		bs.weight = si.specularReflectance * F * weight;
		bs.sampledLobe = BSDF_LOBE_GLOSSY_REFLECTION;
	}
}
#endif