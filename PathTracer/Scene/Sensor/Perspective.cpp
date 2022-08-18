#include "Perspective.h"
#include "Utils/loader_utils.h"
#include <math.h>

Perspective::Perspective(XMLElement* e)
{
	this->fov = getFloatByName(e, "fov", 60.0f);
	this->transform = loadMatrix4(e->FirstChildElement("transform")->FirstChildElement("matrix"));
	mat3 orientation = mat3(this->transform);

	this->width = getIntByName(e->FirstChildElement("film"), "width");
	this->height = getIntByName(e->FirstChildElement("film"), "height");

	float focalLength = ((float)this->width) * 0.5f / tan(radians(fov) * 0.5);
	this->fovy = 2 * degrees(atan(height * 0.5f / focalLength));

	this->forward = orientation * vec3(0, 0, 1);
	this->right = orientation * vec3(-1, 0, 0);
	this->up = orientation * vec3(0, 1, 0);
	this->position = vec3(this->transform * vec4(0, 0, 0, 1));

	float aspectRatio = (float)width / (float) height;
	float ulen = (float) tan(0.5 * this->fov * M_PI / 180);
	float vlen = ulen / aspectRatio;

	u = vec3(this->right) * ulen;
	v = vec3(this->up) * vlen;
	w = vec3(this->forward);
}

void Perspective::update() {
	float aspectRatio = (float)width / (float)height;
	float ulen = (float)tan(0.5 * this->fov * M_PI / 180);
	float vlen = ulen / aspectRatio;

	u = vec3(this->right) * ulen;
	v = vec3(this->up) * vlen;
	w = vec3(this->forward);
}