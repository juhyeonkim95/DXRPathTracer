#include "Diffuse.hlsli"
#include "Conductor.hlsli"
#include "RoughConductor.hlsli"
#include "Dielectric.hlsli"
#include "RoughDielectric.hlsli"
#include "Plastic.hlsli"

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
	BSDF_REFLECTION = BSDF_TYPE_DIFFUSE | BSDF_TYPE_ROUGH_DIELECTRIC | BSDF_TYPE_CONDUCTOR | BSDF_TYPE_PLASTIC | BSDF_TYPE_ROUGH_PLASTIC,
	BSDF_TRANSMISSION = BSDF_TYPE_DIELECTRIC | BSDF_TYPE_ROUGH_DIELECTRIC
};

namespace bsdf
{
	float Pdf(in Material material, in RayPayload payload, in float3 wo) {
		switch (material.materialType) {
		case BSDF_TYPE_DIFFUSE: return diffuse::Pdf(material, payload, wo); 
		case BSDF_TYPE_CONDUCTOR: return conductor::Pdf(material, payload, wo); 
		case BSDF_TYPE_ROUGH_CONDUCTOR: return roughconductor::Pdf(material, payload, wo);
		case BSDF_TYPE_DIELECTRIC: return dielectric::Pdf(material, payload, wo);
		case BSDF_TYPE_ROUGH_DIELECTRIC: return roughdielectric::Pdf(material, payload, wo);
		// case BSDF_TYPE_PLASTIC: return plastic::Pdf(material, payload, wo);
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
		// case BSDF_TYPE_PLASTIC: return plastic::Eval(material, payload, wo);
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