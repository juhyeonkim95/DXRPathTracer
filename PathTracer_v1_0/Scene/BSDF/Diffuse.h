#pragma once
#include "BSDF.h"

class Diffuse : public BSDF
{
public:
	Diffuse() { diffuseReflectance = vec3(0.5f); }
	Diffuse(XMLElement* e);
	std::string toString() override;
};
