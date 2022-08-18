#pragma once
#include "PostProcessPass.h"
#include "HeapData.h"

struct RELAXSingleParameters
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


class RELAXSinglePass : public PostProcessPass
{
public:
	RELAXSinglePass(ID3D12Device5Ptr mpDevice, uvec2 size, std::string name);

	void createRenderTextures(
		HeapData* rtvHeap,
		HeapData* srvHeap);

	void processGUI() override;
	void forward(RenderContext* pRenderContext, RenderData& renderData) override;

	Shader* temporalAccumulationShader;
	Shader* disocclusionFixShader;
	Shader* waveletShader;

	// Temporal Accumulation
	vector<RenderTexture*> temporalAccumulationTextures;
	vector<RenderTexture*> temporalAccumulationTexturesPrev;
	vector<RenderTexture*> temporalAccumulationTexturesMomentAndLength;
	vector<RenderTexture*> temporalAccumulationTexturesMomentAndLengthPrev;

	// Variance Filter
	vector<RenderTexture*> temporalAccumulationTexturesVarianceFilter;

	// AtrousWavelet
	vector<RenderTexture*> waveletPingPongs1;
	vector<RenderTexture*> waveletPingPongs2;

	// RELAXSingle parameters
	RELAXSingleParameters mParam;
	RELAXSingleParameters mDefaultParam;
	vector<ID3D12ResourcePtr> mParamBuffers;
	
	bool mEnabled = true;
	bool mEnableVarianceFilter;

	static const int kMaxWaveletCount = 8;
	int mWaveletCount;
	int mFeedbackTap;

	void uploadParams(uint32_t index);
};
