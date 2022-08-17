#pragma once
#include "imgui.h"
#include "Framework.h"
#include "d3d12shader.h"
#include "DX12DxilLibrary.h"
#include "Scene.h"
#include "SceneResourceManager.h"
#include "ReSTIR/ReSTIR.h"
#include "DX12StateSubobjects.h"
#include "RenderPass.h"

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


class PathTracer : public RenderPass
{
public:
	PathTracer(ID3D12Device5Ptr mpDevice, Scene* scene, uvec2 size);
	void processGUI() override;
	void forward(RenderContext* pRenderContext, RenderData& renderData) override;
	void createShaderResources(HeapData* pSrvUavHeap);
	void createShaderTable(HeapData* pSrvUavHeap);
private:
	void createRtPipelineState();
	RootSignatureDesc createRayGenRootDesc();
	RootSignatureDesc createHitRootDesc();
	RootSignatureDesc createGlobalRootDesc();
	void swapHandle(HeapData* pSrvUavHeap, const char* name1, const char* name2);

	Scene* mpScene;
	D3D12_GPU_DESCRIPTOR_HANDLE mpTextureStartHandle;;

	PathTracerParameters mParam;
	PathTracerParameters mDefaultParam;

	DxilLibrary mShader;
	ID3D12StateObjectPtr mpPipelineState;
	ID3D12RootSignaturePtr mpGlobalRootSig;

	ID3D12ResourcePtr mpShaderTable;
	uint32_t mShaderTableEntrySize = 0;

	ID3D12ResourcePtr mpParamBuffer = nullptr;

	map<string, ID3D12ResourcePtr> mUAVResourceDictionary;

	const WCHAR* kShaderFile = L"RenderPass/Pathtracer/PathTracer.hlsl";

	const WCHAR* kShaderModel = L"lib_6_3";

	const WCHAR* kRayGenShader = L"rayGen";
	const WCHAR* kMissShader = L"miss";
	const WCHAR* kMissEnvShader = L"missEnv";
	const WCHAR* kClosestHitShader = L"chs";
	const WCHAR* kHitGroup = L"HitGroup";
	const WCHAR* kShadowChs = L"shadowChs";
	const WCHAR* kShadowMiss = L"shadowMiss";
	const WCHAR* kShadowHitGroup = L"ShadowHitGroup";

	bool swap = false;
};