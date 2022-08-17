#pragma once
#include "Framework.h"


namespace fresnel
{
	float ConductorReflectance(float eta, float k, float cosThetaI);
	vec3 ConductorReflectance(vec3 eta, vec3 k, float cosThetaI);
	float DielectricReflectance(float eta, float cosThetaI, float& cosThetaT);
	float DielectricReflectance(float eta, float cosThetaI);
	float DiffuseFresnel(float eta);
};
