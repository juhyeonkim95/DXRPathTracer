#pragma once
#include "BSDF.h"

class RoughConductor : public BSDF
{
public:
	RoughConductor() { diffuseReflectance = vec3(0.5f); }
	RoughConductor(XMLElement* e);
	std::string toString() override;
};
