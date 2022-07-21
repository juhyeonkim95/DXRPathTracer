#pragma once
#include "Framework.h"
#include "tinyxml2.h"
#include "Emitter.h"
using namespace tinyxml2;
class Shape
{
public:
	static int anonymousMaterialCount;
	Shape(XMLElement* e);
	virtual std::string toString() { return "Default Shape"; };
	mat4 transform;
	std::string materialID;
};