#pragma once
#include "Framework.h"
#include <map>
using namespace std;


class HeapData
{
public:
	HeapData(ID3D12Device5Ptr pDevice, uint32_t count, D3D12_DESCRIPTOR_HEAP_TYPE type, bool shaderVisible);
	D3D12_GPU_DESCRIPTOR_HANDLE getGPUHandle(uint32_t index);
	D3D12_CPU_DESCRIPTOR_HANDLE getCPUHandle(uint32_t index);
	D3D12_GPU_DESCRIPTOR_HANDLE getGPUHandleByName(const char* name);
	D3D12_CPU_DESCRIPTOR_HANDLE getCPUHandleByName(const char* name);
	D3D12_CPU_DESCRIPTOR_HANDLE addDescriptorHandle(const char* name);
	D3D12_CPU_DESCRIPTOR_HANDLE addDescriptorHandle();
	ID3D12DescriptorHeapPtr getDescriptorHeap() { return mpHeap; }
	uint32_t getLastIndex() { return mUsedEntries; }
	map<string, D3D12_GPU_DESCRIPTOR_HANDLE> &getGPUHandleMap() { return mGPUHandlesMap; };
	D3D12_CPU_DESCRIPTOR_HANDLE getLastCPUHandle();
	D3D12_GPU_DESCRIPTOR_HANDLE getLastGPUHandle();
private:
	ID3D12Device5Ptr mpDevice;
	ID3D12DescriptorHeapPtr mpHeap;
	uint32_t mUsedEntries = 0;

	D3D12_DESCRIPTOR_HEAP_TYPE mDescriptorType;
	map<string, D3D12_GPU_DESCRIPTOR_HANDLE> mGPUHandlesMap;
	map<string, D3D12_CPU_DESCRIPTOR_HANDLE> mCPUHandlesMap;
	map<string, uint32_t> mHandlesOffsetMap;
};