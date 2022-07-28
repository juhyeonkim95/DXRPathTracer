#pragma once
#include "PostProcessPass.h"

struct WaveletShaderParameters
{
	int level;
	vec2 texelSize;
};

class SVGFPass : public PostProcessPass
{
public:
	SVGFPass(ID3D12Device5Ptr mpDevice, uvec2 size);
	void forward(
		ID3D12GraphicsCommandList4Ptr mpCmdList,
		map<string, D3D12_GPU_DESCRIPTOR_HANDLE> gpuHandles,
		map<string, ID3D12ResourcePtr> resourceBuffers,
		ID3D12ResourcePtr mpCameraConstantBuffer
	);
	void createRenderTextures(
		ID3D12DescriptorHeapPtr pRTVHeap,
		uint32_t& usedRTVHeapEntries,
		ID3D12DescriptorHeapPtr pSRVHeap,
		uint32_t& usedSRVHeapEntries);

	Shader* motionVectorShader;
	Shader* temporalAccumulationShader;
	Shader* varianceFilterShader;
	Shader* waveletShader;
	Shader* reconstructionShader;

	RenderTexture* motionVectorRenderTexture;
	RenderTexture* historyLengthRenderTexture;
	RenderTexture* historyLengthRenderTexturePrev;

	RenderTexture* temporalAccumulationTextureDirect;
	RenderTexture* temporalAccumulationTextureDirectPrev;

	RenderTexture* temporalAccumulationTextureDirectMoment;
	RenderTexture* temporalAccumulationTextureDirectMomentPrev;

	RenderTexture* temporalAccumulationTextureIndirect;
	RenderTexture* temporalAccumulationTextureIndirectPrev;

	RenderTexture* temporalAccumulationTextureIndirectMoment;
	RenderTexture* temporalAccumulationTextureIndirectMomentPrev;

	RenderTexture* temporalAccumulationTextureDirectVarianceFilter;
	RenderTexture* temporalAccumulationTextureIndirectVarianceFilter;

	vector<RenderTexture*> waveletDirect;
	vector<RenderTexture*> waveletIndirect;
	int waveletCount = 3;

	RenderTexture* reconstructionRenderTexture;

	ID3D12ResourcePtr mpWaveletParameterBuffer = nullptr;
};

