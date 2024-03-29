#pragma once
#include "PostProcessPass.h"
#include "HeapData.h"

enum RELAX_TYPE : unsigned int {
	RELAX_DIFFUSE = 1 << 0,
	RELAX_SPECULAR = 1 << 1,
	RELAX_DIFFUSE_SPECULAR = 1 << 2
};

struct RELAXParameters
{
	// common
	ivec2 screenSize;
	uint targetPathType;

	// Temporal Accumulation
	float normalThreshold;
	float positionThreshold;
	float depthThreshold;
	int maxAccumulateFrame;

	// Wavelet
	float sigmaP;
	float sigmaN;
	float sigmaL;
	int stepSize;
};


class RELAXPass : public PostProcessPass
{
public:
	RELAXPass(ID3D12Device5Ptr mpDevice, uvec2 size, RELAX_TYPE relaxType, uint targetPathType, std::string name);

	void createRenderTextures(
		HeapData* rtvHeap,
		HeapData* srvHeap);

	void processGUI() override;
	void forward(RenderContext* pRenderContext, RenderData& renderData) override;

	Shader* temporalAccumulationShader;
	Shader* varianceFilterShader;
	Shader* waveletShader;

	// Temporal Accumulation
	RenderTexture* historyLengthRenderTexture;
	RenderTexture* historyLengthRenderTexturePrev;
	RenderTexture* temporalAccumulationTexture;
	RenderTexture* temporalAccumulationTexturePrev;
	RenderTexture* temporalAccumulationTextureMoment;
	RenderTexture* temporalAccumulationTextureMomentPrev;

	// Variance Filter
	RenderTexture* temporalAccumulationTextureVarianceFilter;

	// AtrousWavelet
	RenderTexture* waveletPingPong1;
	RenderTexture* waveletPingPong2;

	// RELAX parameters
	RELAXParameters mParam;
	RELAXParameters mDefaultParam;
	vector<ID3D12ResourcePtr> mParamBuffers;
	
	bool mEnabled = true;
	bool mEnableVarianceFilter;

	static const int kMaxWaveletCount = 8;
	int mWaveletCount;
	int mFeedbackTap;
	RELAX_TYPE relaxType;

	void uploadParams(uint32_t index);
};
