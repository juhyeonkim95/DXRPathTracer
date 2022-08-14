#include "HeapData.h"

#pragma once
HeapData::HeapData(ID3D12Device5Ptr pDevice, uint32_t count, D3D12_DESCRIPTOR_HEAP_TYPE type, bool shaderVisible)
{
	D3D12_DESCRIPTOR_HEAP_DESC desc = {};
	desc.NumDescriptors = count;
	desc.Type = type;
	desc.Flags = shaderVisible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

	d3d_call(pDevice->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&this->mpHeap)));
	this->mUsedEntries = 0;
	this->mpDevice = pDevice;
	this->mDescriptorType = type;
}

D3D12_GPU_DESCRIPTOR_HANDLE HeapData::getGPUHandle(uint32_t index) {
	D3D12_GPU_DESCRIPTOR_HANDLE handle = mpHeap->GetGPUDescriptorHandleForHeapStart();
	handle.ptr += index * mpDevice->GetDescriptorHandleIncrementSize(this->mDescriptorType);
	return handle;
}

D3D12_CPU_DESCRIPTOR_HANDLE HeapData::getCPUHandle(uint32_t index) {
	D3D12_CPU_DESCRIPTOR_HANDLE handle = mpHeap->GetCPUDescriptorHandleForHeapStart();
	handle.ptr += index * mpDevice->GetDescriptorHandleIncrementSize(this->mDescriptorType);
	return handle;
}

D3D12_GPU_DESCRIPTOR_HANDLE HeapData::getGPUHandleByName(const char* name) {
	return this->mGPUHandlesMap.at(name);
}

D3D12_CPU_DESCRIPTOR_HANDLE HeapData::getCPUHandleByName(const char* name) {
	return this->mCPUHandlesMap.at(name);
}


D3D12_CPU_DESCRIPTOR_HANDLE HeapData::getLastCPUHandle() {
	D3D12_CPU_DESCRIPTOR_HANDLE handle = this->mpHeap->GetCPUDescriptorHandleForHeapStart();
	handle.ptr += this->mUsedEntries * mpDevice->GetDescriptorHandleIncrementSize(this->mDescriptorType);
	return handle;
}

D3D12_GPU_DESCRIPTOR_HANDLE HeapData::getLastGPUHandle() {
	D3D12_GPU_DESCRIPTOR_HANDLE handle = this->mpHeap->GetGPUDescriptorHandleForHeapStart();
	handle.ptr += this->mUsedEntries * mpDevice->GetDescriptorHandleIncrementSize(this->mDescriptorType);
	return handle;
}

D3D12_CPU_DESCRIPTOR_HANDLE HeapData::addDescriptorHandle(const char* name) {
	mGPUHandlesMap[name] = getLastGPUHandle();
	mCPUHandlesMap[name] = getLastCPUHandle();
	mHandlesOffsetMap[name] = mUsedEntries;
	mUsedEntries++;
	return mCPUHandlesMap[name];
}

D3D12_CPU_DESCRIPTOR_HANDLE HeapData::addDescriptorHandle() {
	string name = "anonymous_" + std::to_string(mUsedEntries);
	return this->addDescriptorHandle(name.c_str());
}