#include "ShaderStructs.hlsli"
#include "ShaderUtils.hlsli"
#include "Fresnel.hlsli"
#include "Microfacet.hlsli"


float3 evalDiffuseReflectance(in Material material, in RayPayload payload) {
	if (material.diffuseReflectanceTextureID) {
		return g_textures.SampleLevel(g_s0, payload.uv, 0.0f).xyz;
	}
	return material.diffuseReflectance;
}

float eval_roughness(in Material mat, in RayPayload payload)
{
	return mat.roughness;
}

float3 eval_specular_reflectance(in Material mat, in RayPayload payload)
{
	return mat.specularReflectance;
}

float3 eval_specular_transmittance(in Material mat, in RayPayload payload)
{
	return mat.specularTransmittance;
}

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
		bs.weight = evalDiffuseReflectance(mat, si);
		bs.pdf = bs.wo.z * M_1_PIf;
	}
}

namespace dielectric
{
	float Pdf(in Material mat, in RayPayload si, in float3 wo) {
		return 0.0f;
	}

	float3 Eval(in Material mat, in RayPayload si, in float3 wo) {
		return float3(0, 0, 0);
	}

	void Sample(in Material mat, in RayPayload si, inout uint seed, inout BSDFSample bs) {
		float ior = mat.intIOR / mat.extIOR;
		float eta = si.wi.z < 0.0f ? ior : 1 / ior;
		float cos_theta_t;
		const float F = fresnel::DielectricReflectance(eta, abs(si.wi.z), cos_theta_t);
		
		if (nextRand(seed) <= F)
		{
			// Reflect
			bs.wo = float3(-si.wi.x, -si.wi.y, si.wi.z);
			bs.pdf = F;
			bs.weight = mat.specularReflectance;
		}
		else
		{
			// Refract
			bs.wo = float3(-si.wi.x * eta, -si.wi.y * eta, -sign(si.wi.z) * cos_theta_t);
			bs.pdf = 1 - F;
			bs.weight = mat.specularTransmittance * eta * eta;
		}
	}
}

namespace conductor
{
	float Pdf(in Material mat, in RayPayload si, in float3 wo) {
		return 0.0f;
	}

	float3 Eval(in Material mat, in RayPayload si, in float3 wo) {
		return float3(0, 0, 0);
	}

	void Sample(in Material mat, in RayPayload si, inout uint seed, inout BSDFSample bs) {
		const float3 specularReflectance = float3(1, 1, 1);
		if (si.wi.z < 0) {
			bs.pdf = 0.0f;
			bs.weight = 0.0f;
			return;
		}
		bs.wo = float3(-si.wi.x, -si.wi.y, si.wi.z);
		bs.pdf = 0.0f;
		bs.weight = specularReflectance * fresnel::ConductorReflectance(mat.eta, mat.k, si.wi.z);
	}
}

namespace roughconductor
{
	float Pdf(in Material mat, in RayPayload si, in float3 wo) {
		const float3 wi = si.wi;
		if (wi.z <= 0.0f || wo.z <= 0.0f) {
			return 0.0f;
		}
		float roughness = eval_roughness(mat, si);
		float3 specular_reflectance = eval_specular_reflectance(mat, si);

		uint dist = mat.microfacetDistribution;
		float sampleRoughness = eval_roughness(mat, si);
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
		float roughness = eval_roughness(mat, si);
		float3 specular_reflectance = eval_specular_reflectance(mat, si);
		
		uint dist = mat.microfacetDistribution;
		float alpha = microfacet::roughnessToAlpha(dist, roughness);

		float3 hr = normalize(wo + wi);
		float cosThetaM = dot(wi, hr);
		float3 F = fresnel::ConductorReflectance(mat.eta, mat.k, cosThetaM);
		float G = microfacet::G(dist, alpha, wi, wo, hr);
		float D = microfacet::D(dist, alpha, hr);
		float fr = (G * D * 0.25f) / wi.z;

		return specular_reflectance * F * fr;

	}

	void Sample(in Material mat, in RayPayload si, inout uint seed, inout BSDFSample bs) {
		const float3 specularReflectance = float3(1, 1, 1);
		const float roughness = mat.roughness;
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

namespace roughdielectric
{
	float PdfBase(in Material mat, bool sampleR, bool sampleT, in RayPayload si, in float3 wo) {
		const float3 wi = si.wi;

		float ior = mat.intIOR / mat.extIOR;

		uint dist = mat.microfacetDistribution;

		float wiDotN = wi.z;
		float woDotN = wo.z;
		bool reflect = wiDotN * woDotN >= 0.0f;

		float sampleRoughness = (1.2f - 0.2f * sqrt(abs(wiDotN))) * eval_roughness(mat, si);
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
		float alpha = eval_roughness(mat, si);
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
			return eval_specular_reflectance(mat, si) * fr;
		}
		else {
			float fs = abs(wiDotM * woDotM) * (1.0f - F) * G * D / (sqr(eta * wiDotM + woDotM) * abs(wiDotN));
			return eval_specular_transmittance(mat, si) * fs;
		}
	}

	void SampleBase(in Material mat, bool sampleR, bool sampleT, in RayPayload si, inout uint seed, inout BSDFSample bs) {
		const float3 specularReflectance = float3(1, 1, 1);

		const float3 wi = si.wi;
		bs.pdf = 0.0f;
		bs.weight = 0.0f;

		float roughness = mat.roughness;

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
		float weight_multiplier = abs(wiDotM) * G * D / (abs(wiDotN) * pm);
		bs.wo = wo;

		if (reflect) {
			bs.pdf = pm * 0.25f / abs(wiDotM);
			bs.weight = specularReflectance * weight_multiplier;
		}
		else {
			float denom = (eta * wiDotM + woDotM);
			bs.pdf = pm * abs(woDotM) / (denom * denom);
			bs.weight = specularReflectance * weight_multiplier * eta * eta;
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

		float3 diffuse_reflectance = evalDiffuseReflectance(mat, si);

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


namespace bsdf
{
	float Pdf(in Material material, in RayPayload payload, in float3 wo) {
		switch (material.materialType) {
		case BSDF_TYPE_DIFFUSE: return diffuse::Pdf(material, payload, wo); 
		case BSDF_TYPE_CONDUCTOR: return conductor::Pdf(material, payload, wo); 
		case BSDF_TYPE_ROUGH_CONDUCTOR: return roughconductor::Pdf(material, payload, wo);
		case BSDF_TYPE_DIELECTRIC: return dielectric::Pdf(material, payload, wo);
		case BSDF_TYPE_ROUGH_DIELECTRIC: return roughdielectric::Pdf(material, payload, wo);
		//case BSDF_TYPE_PLASTIC: return plastic::Pdf(material, payload, wo);
		}
		return diffuse::Pdf(material, payload, wo);
	}

	float3 Eval(in Material material, in RayPayload payload, in float3 wo) {
		switch (material.materialType) {
		case BSDF_TYPE_DIFFUSE: return diffuse::Eval(material, payload, wo);
		case BSDF_TYPE_CONDUCTOR: return conductor::Eval(material, payload, wo);
		case BSDF_TYPE_ROUGH_CONDUCTOR: return roughconductor::Eval(material, payload, wo);
		case BSDF_TYPE_DIELECTRIC: return dielectric::Eval(material, payload, wo);
		case BSDF_TYPE_ROUGH_DIELECTRIC: return roughdielectric::Eval(material, payload, wo);
		//case BSDF_TYPE_PLASTIC: return plastic::Eval(material, payload, wo);
		}
		return diffuse::Eval(material, payload, wo);
	}

	void Sample(in Material material, in RayPayload payload, inout uint seed, inout BSDFSample bs) {
		switch (material.materialType) {
		case BSDF_TYPE_DIFFUSE: diffuse::Sample(material, payload, seed, bs); break;
		case BSDF_TYPE_CONDUCTOR: conductor::Sample(material, payload, seed, bs); break;
		case BSDF_TYPE_ROUGH_CONDUCTOR: roughconductor::Sample(material, payload, seed, bs); break;
		case BSDF_TYPE_DIELECTRIC: dielectric::Sample(material, payload, seed, bs); break;
		case BSDF_TYPE_ROUGH_DIELECTRIC: roughdielectric::Sample(material, payload, seed, bs); break;
		case BSDF_TYPE_PLASTIC: plastic::Sample(material, payload, seed, bs); break;
		default: diffuse::Sample(material, payload, seed, bs); break;
		}
	}
}