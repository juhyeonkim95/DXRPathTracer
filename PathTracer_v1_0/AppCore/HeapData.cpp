#include "HeapData.h"

#pragma once
HeapData::HeapData(ID3D12Device5Ptr pDevice, uint32_t count, D3D12_DESCRIPTOR_HEAP_TYPE type, bool shaderVisible)
{
	D3D12_DESCRIPTOR_HEAP_DESC desc = {};
	desc.NumDescriptors = count;
	desc.Type = type;
	desc.Flags = shaderVisible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

	d3d_call(pDevice->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&this->pHeap)));
	this->usedEntries = 0;
	this->mpDevice = pDevice;
	this->type = type;
}

D3D12_GPU_DESCRIPTOR_HANDLE HeapData::getGPUHandle(uint32_t index) {
	D3D12_GPU_DESCRIPTOR_HANDLE handle = pHeap->GetGPUDescriptorHandleForHeapStart();
	handle.ptr += index * mpDevice->GetDescriptorHandleIncrementSize(this->type);
	return handle;
}

D3D12_CPU_DESCRIPTOR_HANDLE HeapData::getCPUHandle(uint32_t index) {
	D3D12_CPU_DESCRIPTOR_HANDLE handle = pHeap->GetCPUDescriptorHandleForHeapStart();
	handle.ptr += index * mpDevice->GetDescriptorHandleIncrementSize(this->type);
	return handle;
}

D3D12_GPU_DESCRIPTOR_HANDLE HeapData::getGPUHandleByName(const char* name) {
	return this->gpuHandlesMap.at(name);
}

D3D12_CPU_DESCRIPTOR_HANDLE HeapData::getCPUHandleByName(const char* name) {
	return this->cpuHandlesMap.at(name);
}


D3D12_CPU_DESCRIPTOR_HANDLE HeapData::getLastCPUHandle() {
	D3D12_CPU_DESCRIPTOR_HANDLE handle = this->pHeap->GetCPUDescriptorHandleForHeapStart();
	handle.ptr += this->usedEntries * mpDevice->GetDescriptorHandleIncrementSize(this->type);
	return handle;
}

D3D12_GPU_DESCRIPTOR_HANDLE HeapData::getLastGPUHandle() {
	D3D12_GPU_DESCRIPTOR_HANDLE handle = this->pHeap->GetGPUDescriptorHandleForHeapStart();
	handle.ptr += this->usedEntries * mpDevice->GetDescriptorHandleIncrementSize(this->type);
	return handle;
}

D3D12_CPU_DESCRIPTOR_HANDLE HeapData::addDescriptorHandle(const char* name) {
	gpuHandlesMap[name] = getLastGPUHandle();
	cpuHandlesMap[name] = getLastCPUHandle();
	handlesOffsetMap[name] = usedEntries;
	usedEntries++;
	return cpuHandlesMap[name];
}

D3D12_CPU_DESCRIPTOR_HANDLE HeapData::addDescriptorHandle() {
	string name = "anonymous_" + std::to_string(usedEntries);
	return this->addDescriptorHandle(name.c_str());
}