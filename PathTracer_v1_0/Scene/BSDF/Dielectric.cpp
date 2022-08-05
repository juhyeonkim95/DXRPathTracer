#include "Dielectric.h"
#include "loader_utils.h"
#include "Externals/GLM/glm/gtx/string_cast.hpp"


Dielectric::Dielectric(XMLElement* e)
{
	this->bsdfType = BSDF_TYPE_DIELECTRIC;
	this->intIOR = getFloatByName(e, "intIOR", 1.5046f);
	this->extIOR = getFloatByName(e, "extIOR", 1.000277f);
	this->specularReflectance = getVec3ByName(e, "specularReflectance", vec3(1, 1, 1));
	this->specularTransmittance = getVec3ByName(e, "specularTransmittance", vec3(1, 1, 1));
}

std::string Dielectric::toString()
{
	return "\n[Dielectric Material]\n\t[intIOR]: " + to_string(this->intIOR);
}