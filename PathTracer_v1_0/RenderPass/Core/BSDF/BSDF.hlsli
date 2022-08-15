#include "Diffuse.hlsli"
#include "Conductor.hlsli"
#include "RoughConductor.hlsli"
#include "Dielectric.hlsli"
#include "RoughDielectric.hlsli"
#include "Plastic.hlsli"
#include "RoughPlastic.hlsli"

enum BSDF_TYPE : uint
{
	BSDF_TYPE_DIFFUSE = 1 << 0,
	BSDF_TYPE_DIELECTRIC = 1 << 1,
	BSDF_TYPE_ROUGH_DIELECTRIC = 1 << 2,
	BSDF_TYPE_CONDUCTOR = 1 << 3,
	BSDF_TYPE_ROUGH_CONDUCTOR = 1 << 4,
	BSDF_TYPE_PLASTIC = 1 << 5,
	BSDF_TYPE_ROUGH_PLASTIC = 1 << 6,

	BSDF_TYPE_GLOSSY = BSDF_TYPE_DIFFUSE | BSDF_TYPE_ROUGH_DIELECTRIC | BSDF_TYPE_ROUGH_CONDUCTOR | BSDF_TYPE_PLASTIC | BSDF_TYPE_ROUGH_PLASTIC,
	BSDF_TYPE_DELTA = BSDF_TYPE_DIELECTRIC | BSDF_TYPE_CONDUCTOR,
	BSDF_TYPE_REFLECTION = BSDF_TYPE_DIFFUSE | BSDF_TYPE_ROUGH_DIELECTRIC | BSDF_TYPE_CONDUCTOR | BSDF_TYPE_PLASTIC | BSDF_TYPE_ROUGH_PLASTIC,
	BSDF_TYPE_TRANSMISSION = BSDF_TYPE_DIELECTRIC | BSDF_TYPE_ROUGH_DIELECTRIC
};



namespace bsdf
{
	float3 getApproximatedReflectance(in Material material, in RayPayload payload)
	{
		uint materialType = g_materialinfo[payload.materialIndex].materialType;

		switch (materialType) {
		case BSDF_TYPE_DIFFUSE:             return payload.diffuseReflectance;
		case BSDF_TYPE_CONDUCTOR:           return payload.specularReflectance * material.conductorReflectance;
		case BSDF_TYPE_ROUGH_CONDUCTOR:     return payload.specularReflectance * material.conductorReflectance;
		case BSDF_TYPE_DIELECTRIC:          return payload.specularReflectance * float3(1, 1, 1);
		case BSDF_TYPE_ROUGH_DIELECTRIC:    return payload.specularReflectance * float3(1, 1, 1);
		case BSDF_TYPE_PLASTIC:             return plastic::Eval(material, payload, float3(0, 0, 1), false, true) * M_PIf;
		case BSDF_TYPE_ROUGH_PLASTIC:       return roughplastic::Eval(material, payload, float3(0, 0, 1), false, true) * M_PIf;
		default:                            return payload.diffuseReflectance;
		}
		return payload.diffuseReflectance;
	}

	float3 getApproximatedDiffuseReflectance(in Material material, in RayPayload payload)
	{
		uint materialType = material.materialType;

		switch (materialType) {
		case BSDF_TYPE_DIFFUSE:             return payload.diffuseReflectance;
		case BSDF_TYPE_CONDUCTOR:           return float3(0, 0, 0);
		case BSDF_TYPE_ROUGH_CONDUCTOR:     return float3(0, 0, 0);
		case BSDF_TYPE_DIELECTRIC:          return float3(0, 0, 0);
		case BSDF_TYPE_ROUGH_DIELECTRIC:    return float3(0, 0, 0);
		case BSDF_TYPE_PLASTIC:             return plastic::Eval(material, payload, float3(0, 0, 1), false, true) * M_PIf;
		case BSDF_TYPE_ROUGH_PLASTIC:       return roughplastic::Eval(material, payload, float3(0, 0, 1), false, true) * M_PIf;
		default:                            return payload.diffuseReflectance;
		}
		return payload.diffuseReflectance;
	}

	float3 getApproximatedSpecularReflectance(in Material material, in RayPayload payload)
	{
		uint materialType = material.materialType;

		switch (materialType) {
		case BSDF_TYPE_DIFFUSE:             return float3(0, 0, 0);
		case BSDF_TYPE_CONDUCTOR:           return payload.specularReflectance * material.conductorReflectance;
		case BSDF_TYPE_ROUGH_CONDUCTOR:     return payload.specularReflectance * material.conductorReflectance;
		case BSDF_TYPE_DIELECTRIC:          return payload.specularReflectance;
		case BSDF_TYPE_ROUGH_DIELECTRIC:    return payload.specularReflectance;
		case BSDF_TYPE_PLASTIC:             return payload.specularReflectance;
		case BSDF_TYPE_ROUGH_PLASTIC:       return payload.specularReflectance;
		default:                            return payload.specularReflectance;
		}
		return payload.specularReflectance;
	}

	uint getReflectionLobe(in Material material) {
		switch (material.materialType) {
		case BSDF_TYPE_DIFFUSE:					return BSDF_LOBE_DIFFUSE_REFLECTION;
		case BSDF_TYPE_CONDUCTOR:				return BSDF_LOBE_DELTA_REFLECTION;
		case BSDF_TYPE_ROUGH_CONDUCTOR:			return BSDF_LOBE_GLOSSY_REFLECTION;
		case BSDF_TYPE_DIELECTRIC:				return BSDF_LOBE_DELTA_REFLECTION | BSDF_LOBE_DELTA_TRANSMISSION;
		case BSDF_TYPE_ROUGH_DIELECTRIC:		return BSDF_LOBE_GLOSSY_REFLECTION | BSDF_LOBE_GLOSSY_TRANSMISSION;
		case BSDF_TYPE_PLASTIC:					return BSDF_LOBE_DELTA_REFLECTION | BSDF_LOBE_DIFFUSE_REFLECTION;
		case BSDF_TYPE_ROUGH_PLASTIC:			return BSDF_LOBE_GLOSSY_REFLECTION | BSDF_LOBE_DIFFUSE_REFLECTION;
		}
		return BSDF_LOBE_DIFFUSE_REFLECTION;
	}

	float Pdf(in Material material, in RayPayload payload, in float3 wo) {
		switch (material.materialType) {
		case BSDF_TYPE_DIFFUSE: return diffuse::Pdf(material, payload, wo); 
		case BSDF_TYPE_CONDUCTOR: return conductor::Pdf(material, payload, wo); 
		case BSDF_TYPE_ROUGH_CONDUCTOR: return roughconductor::Pdf(material, payload, wo);
		case BSDF_TYPE_DIELECTRIC: return dielectric::Pdf(material, payload, wo);
		case BSDF_TYPE_ROUGH_DIELECTRIC: return roughdielectric::Pdf(material, payload, wo);
		case BSDF_TYPE_PLASTIC: return plastic::Pdf(material, payload, wo, true, true);
		case BSDF_TYPE_ROUGH_PLASTIC: return roughplastic::Pdf(material, payload, wo, true, true);
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
		case BSDF_TYPE_PLASTIC: return plastic::Eval(material, payload, wo, true, true);
		case BSDF_TYPE_ROUGH_PLASTIC: return roughplastic::Eval(material, payload, wo, true, true);
		}
		return diffuse::Eval(material, payload, wo);
	}

	float3 EvalDiffuse(in Material material, in RayPayload payload, in float3 wo) {
		// Eval Diffuse Component Only
		switch (material.materialType) {
		case BSDF_TYPE_DIFFUSE: return diffuse::Eval(material, payload, wo);
		case BSDF_TYPE_PLASTIC: return plastic::Eval(material, payload, wo, false, true);
		case BSDF_TYPE_ROUGH_PLASTIC: return roughplastic::Eval(material, payload, wo, false, true);
		}
		return float3(0, 0, 0);
	}

	float3 EvalSpecular(in Material material, in RayPayload payload, in float3 wo) {
		// Eval Specular (non-Delta) Component Only
		switch (material.materialType) {
		case BSDF_TYPE_ROUGH_CONDUCTOR: return roughconductor::Eval(material, payload, wo);
		case BSDF_TYPE_ROUGH_DIELECTRIC: return roughdielectric::Eval(material, payload, wo);
		case BSDF_TYPE_PLASTIC: return plastic::Eval(material, payload, wo, true, false);
		case BSDF_TYPE_ROUGH_PLASTIC: return roughplastic::Eval(material, payload, wo, true, false);
		}
		return float3(0, 0, 0);
	}

	float3 EvalSpecularDemodulated(in Material material, in RayPayload payload, in float3 wo) {
		// Eval Specular (non-Delta) Component Only
		switch (material.materialType) {
		case BSDF_TYPE_ROUGH_CONDUCTOR: return roughconductor::EvalDemodulated(material, payload, wo);
		case BSDF_TYPE_ROUGH_PLASTIC: return roughplastic::Eval(material, payload, wo, true, false);
		}
		return float3(0, 0, 0);
	}

	// TODO : Eval BSDF & PDF at the same time?
	//float4 EvalAndPdf(in Material material, in RayPayload payload, in float3 wo) {
	//	switch (material.materialType) {
	//	case BSDF_TYPE_DIFFUSE: return diffuse::EvalAndPdf(material, payload, wo);
	//	case BSDF_TYPE_CONDUCTOR: return conductor::EvalAndPdf(material, payload, wo);
	//	case BSDF_TYPE_ROUGH_CONDUCTOR: return roughconductor::EvalAndPdf(material, payload, wo);
	//	case BSDF_TYPE_DIELECTRIC: return dielectric::EvalAndPdf(material, payload, wo);
	//	case BSDF_TYPE_ROUGH_DIELECTRIC: return roughdielectric::EvalAndPdf(material, payload, wo);
	//	case BSDF_TYPE_PLASTIC: return plastic::EvalAndPdf(material, payload, wo);
	//	case BSDF_TYPE_ROUGH_PLASTIC: return roughplastic::EvalAndPdf(material, payload, wo);
	//	}
	//	return diffuse::EvalAndPdf(material, payload, wo);
	//}

	void Sample(in Material material, in RayPayload payload, inout uint seed, inout BSDFSample bs) {
		switch (material.materialType) {
		case BSDF_TYPE_DIFFUSE: diffuse::Sample(material, payload, seed, bs); break;
		case BSDF_TYPE_CONDUCTOR: conductor::Sample(material, payload, seed, bs); break;
		case BSDF_TYPE_ROUGH_CONDUCTOR: roughconductor::Sample(material, payload, seed, bs); break;
		case BSDF_TYPE_DIELECTRIC: dielectric::Sample(material, payload, seed, bs); break;
		case BSDF_TYPE_ROUGH_DIELECTRIC: roughdielectric::Sample(material, payload, seed, bs); break;
		case BSDF_TYPE_PLASTIC: plastic::Sample(material, payload, seed, bs); break;
		case BSDF_TYPE_ROUGH_PLASTIC: roughplastic::Sample(material, payload, seed, bs); break;
		default: diffuse::Sample(material, payload, seed, bs); break;
		}
	}
}