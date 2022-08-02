#pragma once
#include "Framework.h"
#include <map>
using namespace std;

static const char* aaaa = "rayGen";

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
	// D3D12_CPU_DESCRIPTOR_HANDLE createRTV(ID3D12ResourcePtr pResource, DXGI_FORMAT format);
	ID3D12DescriptorHeapPtr getDescriptorHeap() { return pHeap; }
	uint32_t getLastIndex() { return usedEntries; }
	map<string, D3D12_GPU_DESCRIPTOR_HANDLE> &getGPUHandleMap() { return gpuHandlesMap; };
	D3D12_CPU_DESCRIPTOR_HANDLE getLastCPUHandle();
	D3D12_GPU_DESCRIPTOR_HANDLE getLastGPUHandle();
private:

	ID3D12DescriptorHeapPtr pHeap;
	uint32_t usedEntries = 0;

	ID3D12Device5Ptr mpDevice;
	D3D12_DESCRIPTOR_HEAP_TYPE type;
	map<string, D3D12_GPU_DESCRIPTOR_HANDLE> gpuHandlesMap;
	map<string, D3D12_CPU_DESCRIPTOR_HANDLE> cpuHandlesMap;
	map<string, uint32_t> handlesOffsetMap;
};