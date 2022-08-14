#ifndef EVAL_UTILS
#define EVAL_UTILS

#include "../Common/CommonStructs.hlsli"
#include "../Common/DescriptorTable.hlsli"

// TODO : Remove this file!!

float3 EvalDiffuseReflectance(in Material material, in RayPayload payload) {
	return payload.diffuseReflectance;
	//if (material.diffuseReflectanceTextureID) {
	//	float3 color = g_textures.SampleLevel(g_s0, payload.uv, 0.0f).xyz;
	//	return color;
	//}
	//return material.diffuseReflectance;
}

float EvalRoughness(in Material mat, in RayPayload payload)
{
	return mat.roughness;
}

float3 EvalSpecularReflectance(in Material mat, in RayPayload payload)
{
	return mat.specularReflectance;
}

float3 EvalSpecularTransmittance(in Material mat, in RayPayload payload)
{
	return mat.specularTransmittance;
}

#endif