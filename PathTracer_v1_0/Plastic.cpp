#include "Plastic.h"
#include "loader_utils.h"
#include "Externals/GLM/glm/gtx/string_cast.hpp"


Plastic::Plastic(XMLElement* e)
{
	this->bsdfType = BSDF_TYPE_PLASTIC;

	XMLElement* reflectance = findElementByName(e, "diffuseReflectance");

	if (reflectance == NULL) {
		this->diffuseReflectance = vec3(0.5f);
	}
	else {
		const char* valueType = reflectance->Value();

		if (strcmp(valueType, "rgb") == 0) {
			this->diffuseReflectance = loadVec3(reflectance);
		}
		else if (strcmp(valueType, "texture") == 0) {
			const char* textureType;
			reflectance->QueryStringAttribute("type", &textureType);

			if (strcmp(textureType, "bitmap") == 0) {
				this->diffuseReflectanceTexturePath = getValueByName(reflectance, "filename");
			}
		}
	}

	this->intIOR = getFloatByName(e, "intIOR", 1.5046f);
	this->extIOR = getFloatByName(e, "extIOR", 1.000277f);
	this->specularReflectance = getVec3ByName(e, "specularReflectance", vec3(1, 1, 1));
}

std::string Plastic::toString()
{
	return "\n[Plastic Material]\n\t[Reflectance]:" + to_string(this->diffuseReflectance);
}