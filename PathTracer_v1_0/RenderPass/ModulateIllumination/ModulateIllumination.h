#pragma once
#include "PostProcessPass.h"

struct ModulateIlluminationParameters
{
	float alpha;
	int divide;
	uint unused1;
	uint unused2;
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

	void uploadParams();
};