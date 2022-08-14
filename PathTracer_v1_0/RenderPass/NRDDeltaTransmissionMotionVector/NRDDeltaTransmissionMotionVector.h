#pragma once
#include "PostProcessPass.h"
#include "HeapData.h"

struct NRDDeltaTransmissionMotionVectorParameters
{
	ivec2 screenSize;
	int diffuseMaxAccumulatedFrame;
	int specularMaxAccumulatedFrame;
	float sigmaP;
	float sigmaN;
	float sigmaL;
	int stepSize;
};

class NRDDeltaTransmissionMotionVector : public PostProcessPass
{
public:
	NRDDeltaTransmissionMotionVector(ID3D12Device5Ptr mpDevice, uvec2 size);

	void createRenderTextures(
		HeapData* rtvHeap,
		HeapData* srvHeap);

	void processGUI() override;
	void forward(RenderContext* pRenderContext, RenderData& renderData) override;

	Shader* motionVectorShader;

	RenderTexture* motionVectorRenderTexture;
	RenderTexture* historyLengthRenderTexture;
	RenderTexture* historyLengthRenderTexturePrev;

	bool mDirty;
	bool mEnabled;
};
