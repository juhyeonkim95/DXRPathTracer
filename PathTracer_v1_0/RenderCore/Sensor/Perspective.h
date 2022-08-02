#pragma once
#include "Sensor.h"

class Perspective : public Sensor
{
public:
	Perspective(XMLElement *e);
	void update() override;
private:
	mat4 transform;
};