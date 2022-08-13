#pragma once
#include "PostProcessPass.h"

struct ModulateIlluminationParameters
{
	uint enableDiffuseRadiance;
	uint enableDiffuseReflectance;
	uint enableSpecularRadiance;
	uint enableSpecularReflectance;
	uint enableEmission;

	uint enableDeltaReflectionRadiance;
	uint enableDeltaReflectionReflectance;
	uint enableDeltaReflectionEmission;

	uint enableDeltaTransmissionRadiance;
	uint enableDeltaTransmissionReflectance;
	uint enableDeltaTransmissionEmission;

	uint enableResidualRadiance;
};

class ModulateIllumination : public PostProcessPass
{
public:
	ModulateIllumination(ID3D12Device5Ptr mpDevice, uvec2 size);
	void createRenderTextures(
		HeapData* rtvHeap,
		HeapData* srvHeap);
	void processGUI() override;
	void forward(RenderContext* pRenderContext, RenderData& renderData) override;
	void setAlpha(int frameNumber);

	bool mEnabled = false;
	RenderTexture* blendRenderTexture;
	ModulateIlluminationParameters mParam;
private:
	Shader* mpShader;
	ID3D12ResourcePtr mpParameterBuffer = nullptr;

	bool enableDiffuseRadiance;
	bool enableDiffuseReflectance;
	bool enableSpecularRadiance;
	bool enableSpecularReflectance;
	bool enableEmission;

	bool enableDeltaReflectionRadiance;
	bool enableDeltaReflectionReflectance;
	bool enableDeltaReflectionEmission;

	bool enableDeltaTransmissionRadiance;
	bool enableDeltaTransmissionReflectance;
	bool enableDeltaTransmissionEmission;

	bool enableResidualRadiance;

	void uploadParams();
};