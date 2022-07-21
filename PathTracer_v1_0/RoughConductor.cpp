#include "RoughConductor.h"
#include "loader_utils.h"
#include "Externals/GLM/glm/gtx/string_cast.hpp"


RoughConductor::RoughConductor(XMLElement* e)
{
	this->bsdfType = BSDF_TYPE_ROUGH_CONDUCTOR;
	this->eta = getVec3ByName(e, "eta", vec3(0, 0, 0));
	this->k = getVec3ByName(e, "k", vec3(1, 1, 1));
	this->specularReflectance = getVec3ByName(e, "specularReflectance", vec3(1, 1, 1));
	this->microfacetDistribution = getValueByNameDefault(e, "distribution", "beckmann");
	this->alpha = getFloatByName(e, "alpha", 0.1f);
}

std::string RoughConductor::toString()
{
	return "\n[RoughConductor Material]\n\t[Reflectance]:" + to_string(this->diffuseReflectance);
}