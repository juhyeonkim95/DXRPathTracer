#pragma once
#include "PostProcessPass.h"

class ToneMapper : public PostProcessPass
{
public:
	ToneMapper(ID3D12Device5Ptr mpDevice, uvec2 size);
	void forward(
		ID3D12GraphicsCommandList4Ptr mpCmdList,
		D3D12_GPU_DESCRIPTOR_HANDLE input,
		D3D12_CPU_DESCRIPTOR_HANDLE output,
		ID3D12ResourcePtr outputResource
	);
	void createRenderTextures(
		ID3D12DescriptorHeapPtr pRTVHeap,
		uint32_t& usedRTVHeapEntries,
		ID3D12DescriptorHeapPtr pSRVHeap,
		uint32_t& usedSRVHeapEntries);
	void processGUI() override;

	Shader* tonemapShader;
	RenderTexture* tonemapRenderTexture = nullptr;
};