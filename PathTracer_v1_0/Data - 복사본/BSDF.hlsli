#include "ShaderStructs.hlsli"
#include "ShaderUtils.hlsli"
#include "Fresnel.hlsli"

float3 evalDiffuseReflectance(in Material material, in RayPayload payload) {
	if (material.diffuseReflectanceTextureID) {
		return g_textures.SampleLevel(g_s0, payload.uv, 0.0f).xyz;
	}
	return material.diffuseReflectance;
}

namespace diffuse
{
	float Pdf(in Material material, in RayPayload payload, in float3 wo) {
		return max(dot(payload.normal, wo), 0) * M_1_PIf;
	}

	float3 Eval(in Material material, in RayPayload payload, in float3 wo) {
		return payload.attenuation * max(dot(payload.normal, wo), 0) * M_1_PIf;
	}

	void Sample(in Material material, inout RayPayload payload) {
		//if (si.wi.z < 0) {
		//	bs.pdf = 0.0f;
		//	bs.weight = 0.0f;
		//	return;
		//}
		//bs.wo = getCosHemisphereSampleLocal(si.seed);
		//bs.weight = evalDiffuseReflectance(material, si);
		//bs.pdf = bs.wo.z * M_1_PIf;
		payload.direction = getCosHemisphereSampleWorld(payload.seed, payload.normal);
		payload.attenuation = evalDiffuseReflectance(material, payload);
		payload.scatterPdf = dot(payload.normal, payload.direction) * M_1_PIf;
	}
}

namespace dielectric
{
	float Pdf(in Material material, in RayPayload payload, in float3 wo) {
		return 0.0f;
	}

	float3 Eval(in Material material, in RayPayload payload, in float3 wo) {
		return float3(0, 0, 0);
	}

	void Sample(in Material mat, inout RayPayload payload) {
		float nDotV = dot(payload.normal, -payload.direction);
		float ior = mat.intIOR / mat.extIOR;
		float eta = nDotV < 0.0f ? ior : 1 / ior;
		float cos_theta_t;
		const float F = fresnel::DielectricReflectance(eta, abs(nDotV), cos_theta_t);
		
		if (nextRand(payload.seed) <= F)
		{
			// Reflect
			payload.direction = reflect(payload.direction, payload.normal);
			payload.scatterPdf = F;
			payload.attenuation = mat.specularReflectance;
		}
		else
		{
			// Refract
			float3 i = -payload.direction;
			float3 n = payload.normal;
			float3 i1 = i - dot(n, i) * n;
			float3 i2 = dot(n, i) * n;

			payload.direction = (-eta * i1 - sign(nDotV) * n * cos_theta_t);
			//payload.direction = refract(payload.direction, payload.normal, 1.0f / eta);
			//payload.direction = normalize(payload.direction);
			payload.scatterPdf = 1 - F;
			payload.attenuation = mat.specularTransmittance * eta * eta;
		}
	}
}

namespace conductor
{
	float Pdf(in Material material, in RayPayload payload, in float3 wo) {
		return 0.0f;
	}

	float3 Eval(in Material material, in RayPayload payload, in float3 wo) {
		return float3(0, 0, 0);
	}

	void Sample(in Material material, inout RayPayload payload) {
		float nDotV = max(dot(payload.normal, -payload.direction), 0);
		payload.direction = reflect(payload.direction, payload.normal);
		payload.attenuation = fresnel::ConductorReflectance(material.eta, material.k, nDotV);
		payload.scatterPdf = 0.0f;
	}
}

namespace bsdf
{
	float Pdf(in Material material, in RayPayload payload, in float3 wo) {
		switch (material.materialType) {
		case BSDF_TYPE_DIFFUSE: return diffuse::Pdf(material, payload, wo); break;
		case BSDF_TYPE_CONDUCTOR: return conductor::Pdf(material, payload, wo); break;
		case BSDF_TYPE_DIELECTRIC: return dielectric::Pdf(material, payload, wo); break;
		default: return diffuse::Pdf(material, payload, wo); break;
		}
		//return max(dot(payload.normal, wo), 0) * M_1_PIf;
	}

	float3 Eval(in Material material, in RayPayload payload, in float3 wo) {
		return payload.attenuation * max(dot(payload.normal, wo), 0) * M_1_PIf;
	}

	void Sample(in Material material, inout RayPayload payload) {
		switch (material.materialType) {
		case BSDF_TYPE_DIFFUSE: diffuse::Sample(material, payload); break;
		case BSDF_TYPE_CONDUCTOR: conductor::Sample(material, payload); break;
		case BSDF_TYPE_DIELECTRIC: dielectric::Sample(material, payload); break;
		default: diffuse::Sample(material, payload); break;
		}
	}
}