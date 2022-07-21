#pragma once
#include "Framework.h"
#include "tinyxml2.h"
using namespace tinyxml2;

class Sensor
{
public:
	Sensor() {}
	virtual void update(){}
	vec3 u;
	vec3 v;
	vec3 w;
	vec3 position;

	vec3 forward;
	vec3 right;
	vec3 up;

	float yaw;
	float pitch;
	float roll;

	int width;
	int height;

	float fov = 60.0f;
	float fovy = 60.0f;

};