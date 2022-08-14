#pragma once
#include "PostProcessPass.h"
#include "HeapData.h"

struct MotionVectorSpecularParameters
{
	float normalThreshold;
	float positionThreshold;
	float depthThreshold;
	float unused;
};

class MotionVectorSpecular : public PostProcessPass
{
public:
	MotionVectorSpecular(ID3D12Device5Ptr mpDevice, uvec2 size);

	void createRenderTextures(
		HeapData* rtvHeap,
		HeapData* srvHeap);

	void processGUI() override;
	void forward(RenderContext* pRenderContext, RenderData& renderData) override;

	Shader* MotionVectorSpecularShader;

	RenderTexture* MotionVectorSpecularRenderTexture;
	RenderTexture* historyLengthRenderTexture;
	RenderTexture* historyLengthRenderTexturePrev;

	MotionVectorSpecularParameters mParam;
	MotionVectorSpecularParameters mDefaultParam;

	ID3D12ResourcePtr mParameterBuffer;

	bool mDirty;
	bool mEnabled;
};
