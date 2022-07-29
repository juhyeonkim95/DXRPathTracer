#pragma once
#include "Scene.h"
#include "DX12Helper.h"
#include "Framework.h"
#include "HeapData.h"

class SceneResourceManager
{
public:
    SceneResourceManager(Scene* scene, ID3D12Device5Ptr pDevice, HeapData *srvHeap);
    void createSceneSRVs();
    void createSceneAccelerationStructure(
        ID3D12CommandQueuePtr mpCmdQueue,
        ID3D12CommandAllocatorPtr pCmdAllocator,
        ID3D12GraphicsCommandList4Ptr mpCmdList,
        ID3D12FencePtr mpFence,
        HANDLE mFenceEvent,
        uint64_t& mFenceValue
    );
    void createSceneTextureResources(
        ID3D12CommandQueuePtr mpCmdQueue,
        ID3D12CommandAllocatorPtr pCmdAllocator,
        ID3D12GraphicsCommandList4Ptr mpCmdList,
        ID3D12FencePtr mpFence,
        HANDLE mFenceEvent,
        uint64_t& mFenceValue
    );
    ID3D12ResourcePtr getToplevelAS() {return mpTopLevelAS;};
    D3D12_GPU_DESCRIPTOR_HANDLE getSRVStartHandle() { return mSrvUavHeap->getGPUHandleByName("MaterialData"); };

private:
    AccelerationStructureBuffers createTopLevelAccelerationStructure(ID3D12GraphicsCommandList4Ptr mpCmdList);

    Scene* scene;
    ID3D12Device5Ptr mpDevice;
    HeapData* mSrvUavHeap;

    ID3D12ResourcePtr mpMaterialBuffer = nullptr;
    ID3D12ResourcePtr mpGeometryInfoBuffer = nullptr;
    ID3D12ResourcePtr mpIndicesBuffer = nullptr;
    ID3D12ResourcePtr mpVerticesBuffer = nullptr;

    std::vector<ID3D12ResourcePtr> mpTextureBuffers;

    ID3D12ResourcePtr mpTopLevelAS;
    std::vector<ID3D12ResourcePtr> mpBottomLevelAS;
    uint64_t mTlasSize = 0;
};