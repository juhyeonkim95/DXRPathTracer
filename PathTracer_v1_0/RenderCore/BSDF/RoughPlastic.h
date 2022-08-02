#pragma once
#include "BSDF.h"

class RoughPlastic : public BSDF
{
public:
	RoughPlastic() { diffuseReflectance = vec3(0.5f); }
	RoughPlastic(XMLElement* e);
	std::string toString() override;
};
