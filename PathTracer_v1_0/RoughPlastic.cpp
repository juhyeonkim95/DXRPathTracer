#include "RoughPlastic.h"
#include "loader_utils.h"
#include "Externals/GLM/glm/gtx/string_cast.hpp"


RoughPlastic::RoughPlastic(XMLElement* e)
{
	if (e->FirstChildElement("rgb") == NULL) {
		this->diffuseReflectance = vec3(0.5f);
	}
	else {
		this->diffuseReflectance = loadVec3(e->FirstChildElement("rgb"));
	}

	if (e->FirstChildElement("texture")) {
		const char* texturepath;
		e->FirstChildElement("texture")->FirstChildElement("string")->QueryStringAttribute("value", &texturepath);
		this->diffuseReflectanceTexturePath = texturepath;
	}
}

std::string RoughPlastic::toString()
{
	return "\n[RoughPlastic Material]\n\t[Reflectance]:" + to_string(this->diffuseReflectance);
}