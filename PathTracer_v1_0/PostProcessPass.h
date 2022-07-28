#pragma once
#include "Framework.h"
#include "Shader.h"
#include "RenderTexture.h"
#include <map>
using namespace std;

class PostProcessQuad {
public:
	PostProcessQuad(
		ID3D12Device5Ptr device,
		ID3D12GraphicsCommandList4Ptr mpCmdList,
		ID3D12CommandQueuePtr mpCmdQueue,
		ID3D12FencePtr mpFence,
		uint64_t& mFenceValue
	);
	void bind(ID3D12GraphicsCommandList4Ptr mpCmdList);
	ID3D12Resource* vertexBuffer;
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView;
};

class PostProcessPass {
public:
	PostProcessPass(ID3D12Device5Ptr mpDevice, uvec2 size) 
	{ 
		this->mpDevice = mpDevice; 
		this->size = size; 

		// Fill out the Viewport
		viewport.TopLeftX = 0;
		viewport.TopLeftY = 0;
		viewport.Width = size.x;
		viewport.Height = size.y;
		viewport.MinDepth = 0.0f;
		viewport.MaxDepth = 1.0f;

		// Fill out a scissor rect
		scissorRect.left = 0;
		scissorRect.top = 0;
		scissorRect.right = size.x;
		scissorRect.bottom = size.y;
	};

	void setViewPort(ID3D12GraphicsCommandList4Ptr mpCmdList) {
		mpCmdList->RSSetViewports(1, &viewport); // set the viewports
		mpCmdList->RSSetScissorRects(1, &scissorRect); // set the scissor rects
	}

	D3D12_VIEWPORT viewport;
	D3D12_RECT scissorRect;

	ID3D12Device5Ptr mpDevice;
	uvec2 size;
};