#pragma once
#include "Framework.h"

class PostProcessor {
public:
	PostProcessor() {};
	void Init() {};
	void Update() {};

	ID3D12Device5Ptr device;
	ID3D12RootSignature* rootSignature;
	ID3D12GraphicsCommandList4Ptr mpCmdList;
	ID3D12CommandQueuePtr mpCmdQueue;
	ID3D12FencePtr mpFence;
	uint64_t mFenceValue = 0;

	int Width;
	int Height;

	D3D12_VIEWPORT viewport; // area that output from rasterizer will be stretched to.

	D3D12_RECT scissorRect; // the area to draw in. pixels outside that area will not be drawn onto

	ID3D12PipelineState* pipelineStateObject; // pso containing a pipeline state

	ID3D12Resource* vertexBuffer; // a default buffer in GPU memory that we will load vertex data for our triangle into

	D3D12_VERTEX_BUFFER_VIEW vertexBufferView; // a structure containing a pointer to the vertex data in gpu memory
											   // the total size of the buffer, and the size of each element (vertex)

};