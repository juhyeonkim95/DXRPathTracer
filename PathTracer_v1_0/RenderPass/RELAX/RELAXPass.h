#pragma once
#include "PostProcessPass.h"
#include "HeapData.h"

struct RELAXParameters
{
	ivec2 screenSize;
	int maxAccumulatedFrame;
	float sigmaP;
	float sigmaN;
	float sigmaL;
	int stepSize;
	uint targetPathType;
};


class RELAXPass : public PostProcessPass
{
public:
	RELAXPass(ID3D12Device5Ptr mpDevice, uvec2 size, uint targetPathType, std::string name);
	/*void forward(
		ID3D12GraphicsCommandList4Ptr mpCmdList,
		map<string, D3D12_GPU_DESCRIPTOR_HANDLE> gpuHandles,
		map<string, ID3D12ResourcePtr> resourceBuffers,
		ID3D12ResourcePtr mpCameraConstantBuffer
	);*/
	void createRenderTextures(
		HeapData* rtvHeap,
		HeapData* srvHeap);

	void processGUI() override;
	void forward(RenderContext* pRenderContext, RenderData& renderData) override;

	//Shader* motionVectorShader;
	Shader* temporalAccumulationShader;
	Shader* varianceFilterShader;
	Shader* waveletShader;
	// Shader* reconstructionShader;

	Shader* depthDerivativeShader;
	RenderTexture* depthDerivativeTexture;

	//RenderTexture* motionVectorRenderTexture;
	//RenderTexture* historyLengthRenderTexture;
	//RenderTexture* historyLengthRenderTexturePrev;

	RenderTexture* temporalAccumulationTexture;
	RenderTexture* temporalAccumulationTexturePrev;

	RenderTexture* temporalAccumulationTextureMoment;
	RenderTexture* temporalAccumulationTextureMomentPrev;

	RenderTexture* temporalAccumulationTextureVarianceFilter;

	RenderTexture* waveletPingPong1;
	RenderTexture* waveletPingPong2;

	// RELAX parameters
	RELAXParameters param;
	RELAXParameters defaultParam;
	
	bool mEnabled = true;
	bool mEnableVarianceFilter = true;

	const int maxWaveletCount = 8;
	int waveletCount = 3;
	int mFeedbackTap = 0;

	// RenderTexture* reconstructionRenderTexture;

	vector<ID3D12ResourcePtr> mRELAXParameterBuffers;
	//ID3D12ResourcePtr mMvParameterBuffer;

	void uploadParams(uint32_t index);
};
