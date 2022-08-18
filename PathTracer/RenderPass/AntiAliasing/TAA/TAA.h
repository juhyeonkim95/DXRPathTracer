#pragma once
#include "PostProcessPass.h"

struct TAAParameters
{
	float gAlpha;
	float gColorBoxSigma;
	float unused1;
	float unused2;
};

class TAA : public PostProcessPass
{
public:
	TAA(ID3D12Device5Ptr mpDevice, uvec2 size);
	void createRenderTextures(
		HeapData* rtvHeap,
		HeapData* srvHeap);
	void processGUI() override;
	void forward(RenderContext* pRenderContext, RenderData& renderData) override;

private:
	Shader* mpShader;
	ID3D12ResourcePtr mpParameterBuffer = nullptr;

	RenderTexture* mpRenderTexture;
	RenderTexture* mpPrevTexture;

	TAAParameters mParam;
	TAAParameters mDefaultParam;

	void uploadParams();
};