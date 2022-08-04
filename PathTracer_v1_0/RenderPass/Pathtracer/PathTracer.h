#pragma once
#include "imgui.h"
#include "Framework.h"
#include "d3d12shader.h"
#include "DxilLibrary.h"
#include "Scene.h"
#include "RenderContext.h"
#include "SceneResourceManager.h"
#include "ReSTIR/ReSTIR.h"
#include "DX12StateSubobjects.h"


struct PathTracerParameters
{
	int maxDepth;
	int maxDepthDiffuse;
	int maxDepthSpecular;
	int maxDepthTransmittance;

	bool accumulate;
	int unused1;
	int unused2;
	int unused3;
};


class PathTracer
{
public:
	PathTracer(ID3D12Device5Ptr mpDevice, Scene* scene, uvec2 size);
	void processGUI();
	void forward(ID3D12GraphicsCommandList4Ptr pCmdList, SceneResourceManager* pSceneResourceManager, HeapData* pSrvUavHeap, ReSTIRParameters& restirParam);
	void copyback(ID3D12GraphicsCommandList4Ptr pCmdList);
	void createShaderResources(HeapData* pSrvUavHeap, SceneResourceManager* pSceneResourceManager);
	void createShaderTable(HeapData* pSrvUavHeap);
	ID3D12ResourcePtr getOutput(std::string name) { return this->outputUAVBuffers.at(name); };
	map<string, ID3D12ResourcePtr>& getOutputs() { return outputUAVBuffers; };
	bool mDirty = false;
private:
	void createRtPipelineState();
	RootSignatureDesc createRayGenRootDesc();
	RootSignatureDesc createHitRootDesc();
	RootSignatureDesc createGlobalRootDesc();

	ID3D12Device5Ptr mpDevice;
	uvec2 size;

	Scene* scene;
	D3D12_GPU_DESCRIPTOR_HANDLE mpTextureStartHandle;;

	PathTracerParameters param;
	PathTracerParameters defaultParam;

	DxilLibrary mShader;
	ID3D12StateObjectPtr mpPipelineState;
	ID3D12RootSignaturePtr mpGlobalRootSig;

	ID3D12ResourcePtr mpShaderTable;
	uint32_t mShaderTableEntrySize = 0;

	map<string, ID3D12ResourcePtr> outputUAVBuffers;
	ID3D12ResourcePtr mpParamBuffer = nullptr;

	const WCHAR* kShaderFile = L"RenderPass/Pathtracer/ReSTIRPathTracer.hlsl";
	const WCHAR* kShaderModel = L"lib_6_3";

	const WCHAR* kRayGenShader = L"rayGen";
	const WCHAR* kMissShader = L"miss";
	const WCHAR* kMissEnvShader = L"missEnv";
	const WCHAR* kClosestHitShader = L"chs";
	const WCHAR* kHitGroup = L"HitGroup";
	const WCHAR* kShadowChs = L"shadowChs";
	const WCHAR* kShadowMiss = L"shadowMiss";
	const WCHAR* kShadowHitGroup = L"ShadowHitGroup";
};