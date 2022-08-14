#pragma once
#include "PostProcessPass.h"


class DepthDerivativePass : public PostProcessPass
{
public:
	DepthDerivativePass(ID3D12Device5Ptr mpDevice, uvec2 size);
	void createRenderTextures(
		HeapData* rtvHeap,
		HeapData* srvHeap);
	void processGUI() override;
	void forward(RenderContext* pRenderContext, RenderData& renderData) override;
	
	bool mEnabled = false;
	RenderTexture* depthDerivativeRenderTexture;
private:
	Shader* mpShader;
	void uploadParams();
};