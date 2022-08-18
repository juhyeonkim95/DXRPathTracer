#pragma once
#include "BSDF.h"

class Plastic : public BSDF
{
public:
	Plastic() { diffuseReflectance = vec3(0.5f); }
	Plastic(XMLElement* e);
	std::string toString() override;
};
