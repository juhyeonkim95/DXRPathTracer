#include "Conductor.h"
#include "loader_utils.h"
#include "Externals/GLM/glm/gtx/string_cast.hpp"


Conductor::Conductor(XMLElement* e)
{
	this->bsdfType = BSDF_TYPE_CONDUCTOR;
	this->eta = getVec3ByName(e, "eta", vec3(0, 0, 0));
	this->k = getVec3ByName(e, "k", vec3(1, 1, 1));
	this->specularReflectance = getVec3ByName(e, "specularReflectance", vec3(1, 1, 1));
}

std::string Conductor::toString()
{
	return "\n[Conductor Material]\n\t[Reflectance]:" + to_string(this->diffuseReflectance);
}