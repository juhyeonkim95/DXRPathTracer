#pragma once
#include "Framework.h"

class RenderTexture
{
public:
	RenderTexture() {};
	void createWithSize(size_t width, size_t height, DXGI_FORMAT format);
	D3D12_CPU_DESCRIPTOR_HANDLE mRtvDescriptorHandle;
	D3D12_CPU_DESCRIPTOR_HANDLE mSrvDescriptorHandle;
	int mSrvDescriptorHandleOffset;

	ID3D12Device5Ptr mpDevice;
	ID3D12ResourcePtr mResource;

	D3D12_RESOURCE_STATES mState;

	size_t mWidth;
	size_t mHeight;
};