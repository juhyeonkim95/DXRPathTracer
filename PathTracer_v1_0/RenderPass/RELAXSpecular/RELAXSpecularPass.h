#pragma once
#include "PostProcessPass.h"
#include "HeapData.h"

struct RELAXSpecularParameters
{
	ivec2 screenSize;
	int maxAccumulatedFrame;
	float sigmaP;
	float sigmaN;
	float sigmaL;
	int stepSize;
	uint targetPathType;

	float3 cameraPosition;
	float roughnessMultiplier;
};


class RELAXSpecularPass : public PostProcessPass
{
public:
	RELAXSpecularPass(ID3D12Device5Ptr mpDevice, uvec2 size, uint targetPathType, std::string name);

	void createRenderTextures(
		HeapData* rtvHeap,
		HeapData* srvHeap);

	void processGUI() override;
	void forward(RenderContext* pRenderContext, RenderData& renderData) override;

	Shader* temporalAccumulationShader;
	Shader* varianceFilterShader;
	Shader* waveletShader;
	RenderTexture* temporalAccumulationTexture;
	RenderTexture* temporalAccumulationTexturePrev;

	RenderTexture* temporalAccumulationTextureMoment;
	RenderTexture* temporalAccumulationTextureMomentPrev;

	RenderTexture* temporalAccumulationTextureVarianceFilter;

	RenderTexture* waveletPingPong1;
	RenderTexture* waveletPingPong2;

	// RELAXSpecular parameters
	RELAXSpecularParameters param;
	RELAXSpecularParameters defaultParam;
	
	bool mEnabled = true;
	bool mEnableVarianceFilter = true;

	const int maxWaveletCount = 8;
	int waveletCount = 3;
	int mFeedbackTap = 0;
	vector<ID3D12ResourcePtr> mRELAXSpecularParameterBuffers;

	void uploadParams(uint32_t index);
};
