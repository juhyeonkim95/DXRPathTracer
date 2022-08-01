#pragma once
#include "PostProcessPass.h"
#include "HeapData.h"

struct ReSTIRParameters
{
	float alpha;
	float momentsAlpha;
	float sigmaP;
	float sigmaN;
	float sigmaL;

	int level;
	vec2 texelSize;
};

class ReSTIRPass : public PostProcessPass
{
public:
	ReSTIRPass(ID3D12Device5Ptr mpDevice, uvec2 size);
	void forward(
		ID3D12GraphicsCommandList4Ptr mpCmdList,
		map<string, D3D12_GPU_DESCRIPTOR_HANDLE> gpuHandles,
		map<string, ID3D12ResourcePtr> resourceBuffers,
		ID3D12ResourcePtr mpCameraConstantBuffer
	);
	void createRenderTextures(
		HeapData* rtvHeap,
		HeapData* srvHeap);

	void processGUI() override;

	Shader* initLightAndTemporalShader;

};