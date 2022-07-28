#pragma once
#include "Framework.h"
#include "HeapData.h"


class RenderTexture
{
public:
	RenderTexture() {};
	void createWithSize(size_t width, size_t height, DXGI_FORMAT format);
	D3D12_GPU_DESCRIPTOR_HANDLE getGPUSrvHandler();

	D3D12_CPU_DESCRIPTOR_HANDLE mRtvDescriptorHandle;
	D3D12_CPU_DESCRIPTOR_HANDLE mSrvDescriptorHandle;
	D3D12_GPU_DESCRIPTOR_HANDLE mSrvDescriptorHandleGPU;

	// ID3D12DescriptorHeapPtr mpSrvHeap;
	// int mSrvDescriptorHandleOffset;

	ID3D12Device5Ptr mpDevice;
	ID3D12ResourcePtr mResource;

	D3D12_RESOURCE_STATES mState;

	size_t mWidth;
	size_t mHeight;
};

RenderTexture* createRenderTexture(
	ID3D12Device5Ptr pDevice,
	HeapData* rtvHeap,
	HeapData* srvHeap,
	uvec2 size,
	DXGI_FORMAT format
);