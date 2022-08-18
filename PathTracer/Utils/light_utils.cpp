#include "light_utils.h"

LightParameter getLightParameter(mat4 transform) {
	vec3 p0 = vec3(-1, -1, 0);
	vec3 p1 = vec3(1, -1, 0);
	vec3 p2 = vec3(-1, 1, 0);

	vec3 q0 = vec3(transform * vec4(p0, 1.0f));
	vec3 q1 = vec3(transform * vec4(p1, 1.0f));
	vec3 q2 = vec3(transform * vec4(p2, 1.0f));

	LightParameter param;
	param.position = vec4(q0, 1.0f);
	param.u = vec4(q1 - q0, 1.0f);
	param.v = vec4(q2 - q0, 1.0f);
	float area = length(cross(q1 - q0, q2 - q0));
	float pdf = 1.0f / area;
	param.normalAndPdf = vec4(normalize(cross(q1 - q0, q2 - q0)), pdf);
	return param;
}