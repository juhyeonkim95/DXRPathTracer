#pragma once
#include "PostProcessPass.h"
#include "HeapData.h"

class MotionVectorDeltaReflection : public PostProcessPass
{
public:
	MotionVectorDeltaReflection(ID3D12Device5Ptr mpDevice, uvec2 size);

	void createRenderTextures(
		HeapData* rtvHeap,
		HeapData* srvHeap);

	void processGUI() override;
	void forward(RenderContext* pRenderContext, RenderData& renderData) override;

private:
	Shader* mpMotionVectorShader;
	RenderTexture* mpMotionVectorRenderTexture;
};
