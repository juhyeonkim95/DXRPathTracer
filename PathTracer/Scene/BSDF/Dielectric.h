#pragma once
#include "BSDF.h"

class Dielectric : public BSDF
{
public:
	Dielectric() { diffuseReflectance = vec3(0.5f); }
	Dielectric(XMLElement* e);
	std::string toString() override;
};
