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

	void addOutputs(map<string, ID3D12ResourcePtr> output) {
		resourceDictionary.insert(output.begin(), output.end());
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
	}
	bool mDirty = false;
	virtual void processGUI() = 0;
	virtual void forward(RenderContext* pRenderContext, RenderData& renderData) = 0;

protected:
	ID3D12Device5Ptr mpDevice;
	uvec2 size;
};