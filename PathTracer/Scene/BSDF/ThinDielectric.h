#pragma once
#include "BSDF.h"

class ThinDielectric : public BSDF
{
public:
	ThinDielectric() { diffuseReflectance = vec3(0.5f); }
	ThinDielectric(XMLElement* e);
	std::string toString() override;
};
