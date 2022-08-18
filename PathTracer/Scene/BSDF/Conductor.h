#pragma once
#include "BSDF.h"

class Conductor : public BSDF
{
public:
	Conductor() { diffuseReflectance = vec3(0.5f); }
	Conductor(XMLElement* e);
	std::string toString() override;
};
