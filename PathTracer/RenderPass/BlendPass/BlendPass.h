#pragma once
#include "PostProcessPass.h"

struct BlendParameters
{
	float alpha;
	int divide;
	uint unused1;
	uint unused2;
};

class BlendPass : public PostProcessPass
{
public:
	BlendPass(ID3D12Device5Ptr mpDevice, uvec2 size);
	void createRenderTextures(
		HeapData* rtvHeap,
		HeapData* srvHeap);
	void processGUI() override;
	void forward(RenderContext* pRenderContext, RenderData& renderData) override;
	void setAlpha(int frameNumber);

	RenderTexture* blendRenderTexture;
	BlendParameters mParam;
private:
	Shader* mpShader;
	ID3D12ResourcePtr mpParameterBuffer = nullptr;

	const int kMaxFrameNumber = 512;
	int maxFrameNumber = 128;

	void uploadParams();
};