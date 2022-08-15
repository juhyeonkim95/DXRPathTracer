#pragma once
#include "PostProcessPass.h"

struct FXAAParameters
{
	vec2 rcpTexDim;
	float qualitySubPix;
	float qualityEdgeThreshold;
	float qualityEdgeThresholdMin;
	int earlyOut;
	int unused1;
	int unused2;
};

class FXAA : public PostProcessPass
{
public:
	FXAA(ID3D12Device5Ptr mpDevice, uvec2 size);
	void createRenderTextures(
		HeapData* rtvHeap,
		HeapData* srvHeap);
	void processGUI() override;
	void forward(RenderContext* pRenderContext, RenderData& renderData) override;

	bool mEnabled;
	RenderTexture* renderTexture;
	FXAAParameters mParam;
	FXAAParameters mDefaultParam;

private:
	Shader* mpShader;
	ID3D12ResourcePtr mpParameterBuffer = nullptr;

	void uploadParams();
};