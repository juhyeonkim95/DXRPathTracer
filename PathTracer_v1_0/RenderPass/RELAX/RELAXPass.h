#pragma once
#include "PostProcessPass.h"
#include "HeapData.h"

struct RELAXParameters
{
	ivec2 screenSize;
	int diffuseMaxAccumulatedFrame;
	int specularMaxAccumulatedFrame;
	float sigmaP;
	float sigmaN;
	float sigmaL;
	int stepSize;
};

class RELAXPass : public PostProcessPass
{
public:
	RELAXPass(ID3D12Device5Ptr mpDevice, uvec2 size);
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

	Shader* motionVectorShader;
	Shader* temporalAccumulationShader;
	Shader* varianceFilterShader;
	Shader* waveletShader;
	Shader* reconstructionShader;

	Shader* depthDerivativeShader;
	RenderTexture* depthDerivativeTexture;

	RenderTexture* motionVectorRenderTexture;
	RenderTexture* historyLengthRenderTexture;
	RenderTexture* historyLengthRenderTexturePrev;

	RenderTexture* temporalAccumulationTextureDiffuse;
	RenderTexture* temporalAccumulationTextureDiffusePrev;

	RenderTexture* temporalAccumulationTextureDiffuseMoment;
	RenderTexture* temporalAccumulationTextureDiffuseMomentPrev;

	RenderTexture* temporalAccumulationTextureSpecular;
	RenderTexture* temporalAccumulationTextureSpecularPrev;

	RenderTexture* temporalAccumulationTextureSpecularMoment;
	RenderTexture* temporalAccumulationTextureSpecularMomentPrev;

	RenderTexture* temporalAccumulationTextureDiffuseVarianceFilter;
	RenderTexture* temporalAccumulationTextureSpecularVarianceFilter;

	vector<RenderTexture*> waveletDiffuse;
	vector<RenderTexture*> waveletSpecular;

	RenderTexture* waveletDiffusePingPong1;
	RenderTexture* waveletDiffusePingPong2;
	RenderTexture* waveletSpecularPingPong1;
	RenderTexture* waveletSpecularPingPong2;

	// RELAX parameters
	RELAXParameters param;
	RELAXParameters defaultParam;

	bool mEnabled = true;
	bool mEnableVarianceFilter = true;

	const int maxWaveletCount = 8;
	int waveletCount = 3;
	int mFeedbackTap = 0;


	RenderTexture* reconstructionRenderTexture;

	vector<ID3D12ResourcePtr> mRELAXParameterBuffers;

	void uploadParams(uint32_t index);
};
