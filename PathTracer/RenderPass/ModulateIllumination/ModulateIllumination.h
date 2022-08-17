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

private:
	RenderTexture* mpRenderTexture;
	ModulateIlluminationParameters mParam;

	Shader* mpShader;
	ID3D12ResourcePtr mpParameterBuffer = nullptr;

	bool mEnableDiffuseRadiance;
	bool mEnableDiffuseReflectance;
	bool mEnableSpecularRadiance;
	bool mEnableSpecularReflectance;
	bool mEnableEmission;

	bool mEnableDeltaReflectionRadiance;
	bool mEnableDeltaReflectionReflectance;
	bool mEnableDeltaReflectionEmission;

	bool mEnableDeltaTransmissionRadiance;
	bool mEnableDeltaTransmissionReflectance;
	bool mEnableDeltaTransmissionEmission;
	bool mEnableResidualRadiance;

	void uploadParams();
};