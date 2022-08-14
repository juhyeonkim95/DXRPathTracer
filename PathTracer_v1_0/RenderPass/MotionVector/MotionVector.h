#pragma once
#include "PostProcessPass.h"
#include "HeapData.h"

struct MotionVectorParameters
{
	float normalThreshold;
	float positionThreshold;
	float depthThreshold;
	float unused;
};

class MotionVector : public PostProcessPass
{
public:
	MotionVector(ID3D12Device5Ptr mpDevice, uvec2 size);

	void createRenderTextures(
		HeapData* rtvHeap,
		HeapData* srvHeap);

	void processGUI() override;
	void forward(RenderContext* pRenderContext, RenderData& renderData) override;

	Shader* motionVectorShader;

	RenderTexture* motionVectorRenderTexture;
	RenderTexture* historyLengthRenderTexture;
	RenderTexture* historyLengthRenderTexturePrev;

	MotionVectorParameters mParam;
	MotionVectorParameters mDefaultParam;

	ID3D12ResourcePtr mParameterBuffer;

	bool mDirty;
	bool mEnabled;
};
