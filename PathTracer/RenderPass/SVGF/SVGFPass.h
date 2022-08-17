#pragma once
#include "PostProcessPass.h"
#include "HeapData.h"

struct SVGFParameters
{
	ivec2 screenSize;
	float alpha;
	float momentsAlpha;
	float sigmaP;
	float sigmaN;
	float sigmaL;
	int stepSize;
};

class SVGFPass : public PostProcessPass
{
public:
	SVGFPass(ID3D12Device5Ptr mpDevice, uvec2 size);
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

	RenderTexture* waveletDirectPingPong1;
	RenderTexture* waveletDirectPingPong2;
	RenderTexture* waveletIndirectPingPong1;
	RenderTexture* waveletIndirectPingPong2;

	// SVGF parameters
	SVGFParameters param;
	SVGFParameters defaultParam;

	bool mEnabled = true;
	bool mEnableVarianceFilter = true;

	const int maxWaveletCount = 8;
	int waveletCount = 3;
	int mFeedbackTap = 0;


	RenderTexture* reconstructionRenderTexture;

	vector<ID3D12ResourcePtr> mSVGFParameterBuffers;

	void uploadParams(uint32_t index);
};
