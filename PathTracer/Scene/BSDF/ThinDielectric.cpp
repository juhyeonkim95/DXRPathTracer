#include "ThinDielectric.h"
#include "loader_utils.h"
#include "Externals/GLM/glm/gtx/string_cast.hpp"


ThinDielectric::ThinDielectric(XMLElement* e)
{
	if (e->FirstChildElement("rgb") == NULL) {
		this->diffuseReflectance = vec3(0.5f);
	}
	else {
		this->diffuseReflectance = loadVec3(e->FirstChildElement("rgb"));
	}
}

std::string ThinDielectric::toString()
{
	return "\n[ThinDielectric Material]\n\t[Reflectance]:" + to_string(this->diffuseReflectance);
}