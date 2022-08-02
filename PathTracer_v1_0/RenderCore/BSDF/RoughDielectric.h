#pragma once
#include "BSDF.h"

class RoughDielectric : public BSDF
{
public:
	RoughDielectric() { diffuseReflectance = vec3(0.5f); }
	RoughDielectric(XMLElement* e);
	std::string toString() override;
};
