#pragma once
#include "Framework.h"
#include "External/tinyxml2.h"
using namespace tinyxml2;

enum BSDF_TYPE : uint32_t
{
	BSDF_TYPE_DIFFUSE = 1 << 0,
	BSDF_TYPE_DIELECTRIC = 1 << 1,
	BSDF_TYPE_ROUGH_DIELECTRIC = 1 << 2,
	BSDF_TYPE_CONDUCTOR = 1 << 3,
	BSDF_TYPE_ROUGH_CONDUCTOR = 1 << 4,
	BSDF_TYPE_PLASTIC = 1 << 5,
	BSDF_TYPE_ROUGH_PLASTIC = 1 << 6
};

class BSDF
{
public:
	virtual std::string toString() { return "Default BSDF";};

	uint32_t bsdfType = BSDF_TYPE_DIFFUSE;

	vec3 diffuseReflectance;
	uint32 diffuseReflectanceTextureID;
	vec3 specularReflectance;
	uint32 specularReflectanceTextureID;
	vec3 specularTransmittance;
	uint32 specularTransmittanceTextureID;

	vec3 conductorReflectance;
	float diffuseFresnel;

	std::string diffuseReflectanceTexturePath;
	std::string refID;
	
	bool isDoubleSided=false;

	vec3 eta;
	vec3 k;
	float intIOR = 1;
	float extIOR = 1;

	std::string microfacetDistribution;
	float alpha;

	uint nonlinear = 0;
};