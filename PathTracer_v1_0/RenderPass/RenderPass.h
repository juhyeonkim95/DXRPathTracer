#pragma once
#include "Framework.h"
#include "SceneResourceManager.h"
#include "ReSTIR/ReSTIR.h"

struct RenderContext
{
public:
	ID3D12GraphicsCommandList4Ptr pCmdList;
	SceneResourceManager* pSceneResourceManager;
	HeapData* pSrvUavHeap;
	ReSTIRParameters *restirParam;
};

class RenderData
{
public:
	map<string, D3D12_GPU_DESCRIPTOR_HANDLE> gpuHandleDictionary;
	map<string, D3D12_CPU_DESCRIPTOR_HANDLE> cpuHandleDictionary;
	map<string, ID3D12ResourcePtr> resourceDictionary;

	map<string, D3D12_GPU_DESCRIPTOR_HANDLE> outputGPUHandleDictionary;

	void addOutputs(map<string, D3D12_GPU_DESCRIPTOR_HANDLE> output) {
		outputGPUHandleDictionary.insert(output.begin(), output.end());
	};

	void clear(){
		this->gpuHandleDictionary.clear();
		this->cpuHandleDictionary.clear();
		this->resourceDictionary.clear();
	}
};

class RenderPass
{
public:
	RenderPass(ID3D12Device5Ptr mpDevice, uvec2 size) {
		this->mpDevice = mpDevice;
		this->size = size;
		mDirty = false;
		mEnabled = true;
	}
	bool isEnabled() { return mEnabled; }
	bool setEnabled(bool enable) { mEnabled = enable; }
	bool isDirty() { return mDirty; }

	virtual void processGUI() = 0;
	virtual void forward(RenderContext* pRenderContext, RenderData& renderData) = 0;
	std::string name;

protected:
	ID3D12Device5Ptr mpDevice;
	uvec2 size;
	bool mDirty;
	bool mEnabled;
};