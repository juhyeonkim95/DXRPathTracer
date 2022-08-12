#pragma once
#include "PostProcessPass.h"
#include "HeapData.h"

struct NRDDeltaReflectionParameters
{
	ivec2 screenSize;
	int diffuseMaxAccumulatedFrame;
	int specularMaxAccumulatedFrame;
	float sigmaP;
	float sigmaN;
	float sigmaL;
	int stepSize;
};

class NRDDeltaReflection : public PostProcessPass
{
public:
	NRDDeltaReflection(ID3D12Device5Ptr mpDevice, uvec2 size);
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

	Shader* depthDerivativeShader;
	RenderTexture* depthDerivativeTexture;

	RenderTexture* motionVectorRenderTexture;
	RenderTexture* historyLengthRenderTexture;
	RenderTexture* historyLengthRenderTexturePrev;

	RenderTexture* temporalAccumulationTextureDeltaReflection;
	RenderTexture* temporalAccumulationTextureDeltaReflectionPrev;

	RenderTexture* temporalAccumulationTextureDeltaReflectionMoment;
	RenderTexture* temporalAccumulationTextureDeltaReflectionMomentPrev;

	RenderTexture* temporalAccumulationTextureDeltaTransmission;
	RenderTexture* temporalAccumulationTextureDeltaTransmissionPrev;

	RenderTexture* temporalAccumulationTextureDeltaTransmissionMoment;
	RenderTexture* temporalAccumulationTextureDeltaTransmissionMomentPrev;

	RenderTexture* temporalAccumulationTextureDeltaReflectionVarianceFilter;
	RenderTexture* temporalAccumulationTextureDeltaTransmissionVarianceFilter;

	RenderTexture* waveletDeltaReflectionPingPong1;
	RenderTexture* waveletDeltaReflectionPingPong2;
	RenderTexture* waveletDeltaTransmissionPingPong1;
	RenderTexture* waveletDeltaTransmissionPingPong2;


	// NRDDeltaReflectionParameters parameters
	NRDDeltaReflectionParameters param;
	NRDDeltaReflectionParameters defaultParam;

	bool mEnabled = true;
	bool mEnableVarianceFilter = true;

	const int maxWaveletCount = 8;
	int waveletCount = 3;
	int mFeedbackTap = 0;


	// RenderTexture* reconstructionRenderTexture;

	vector<ID3D12ResourcePtr> mNRDDeltaReflectionParameterBuffers;

	void uploadParams(uint32_t index);
};
