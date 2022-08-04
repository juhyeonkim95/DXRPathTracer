#pragma once
#include "HeapData.h"

class RenderContext
{
public:
	void createUAVBuffer(DXGI_FORMAT format, uvec2 size, std::string name, uint depth, uint struct_size = 0);
	HeapData* getSevUavHeap() { return mpSrvUavHeap; };
	RenderContext(ID3D12Device5Ptr pDevice, HeapData* pSrvUavHeap);
private:
	ID3D12Device5Ptr mpDevice;
	HeapData* mpSrvUavHeap;
	std::map<string, ID3D12ResourcePtr> outputUavBuffers;
};