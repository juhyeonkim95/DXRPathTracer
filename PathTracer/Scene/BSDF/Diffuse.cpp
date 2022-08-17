#include "Diffuse.h"
#include "loader_utils.h"
#include "Externals/GLM/glm/gtx/string_cast.hpp"
#include <iostream>
#include <stdexcept>

Diffuse::Diffuse(XMLElement* e)
{
	XMLElement* reflectance = findElementByName(e, "reflectance");
	
	if (reflectance == NULL) {
		this->diffuseReflectance = vec3(0.5f);
	}
	else {
		const char* valueType = reflectance->Value();

		if (strcmp(valueType, "rgb") == 0) {
			this->diffuseReflectance = loadVec3(reflectance);
		}
		else if(strcmp(valueType, "texture") == 0) {
			const char* textureType;
			reflectance->QueryStringAttribute("type", &textureType);
			
			if (strcmp(textureType, "bitmap") == 0) {
				this->diffuseReflectanceTexturePath = getValueByName(reflectance, "filename");
			}
		}
	}
}

std::string Diffuse::toString()
{	
	return "\n[Diffuse Material]\n\t[Reflectance]:" + to_string(this->diffuseReflectance) + "\n\t[Texture]" + this->diffuseReflectanceTexturePath;
}