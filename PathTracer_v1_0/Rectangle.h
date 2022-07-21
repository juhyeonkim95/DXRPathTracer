#pragma once
#include "Shape.h"
class Rectangle :public Shape
{
public:
	Rectangle(XMLElement* e) : Shape(e) {};
	std::string toString() override;
};