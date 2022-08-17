#include "RoughDielectric.h"
#include "loader_utils.h"
#include "Externals/GLM/glm/gtx/string_cast.hpp"


RoughDielectric::RoughDielectric(XMLElement* e)
{
	this->bsdfType = BSDF_TYPE_ROUGH_DIELECTRIC;
	this->intIOR = getFloatByName(e, "intIOR", 1.5046f);
	this->extIOR = getFloatByName(e, "extIOR", 1.000277f);
	this->specularReflectance = getVec3ByName(e, "specularReflectance", vec3(1, 1, 1));
	this->specularTransmittance = getVec3ByName(e, "specularTransmittance", vec3(1, 1, 1));
	this->microfacetDistribution = getValueByNameDefault(e, "distribution", "beckmann");
	this->alpha = getFloatByName(e, "alpha", 0.1f);
}

std::string RoughDielectric::toString()
{
	return "\n[RoughDielectric Material]\n\t[Reflectance]:" + to_string(this->diffuseReflectance);
}