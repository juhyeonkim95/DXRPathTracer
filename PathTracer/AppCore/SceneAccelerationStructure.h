#pragma once
#include "Framework.h"
#include "Scene.h"

class SceneAccelerationStructure
{
public:
    SceneAccelerationStructure(Scene* pScene) { this->mpScene = pScene; };
    void createSceneAccelerationStructure(
        ID3D12Device5Ptr pDevice,
        ID3D12CommandQueuePtr pCmdQueue,
        ID3D12CommandAllocatorPtr pCmdAllocator,
        ID3D12GraphicsCommandList4Ptr pCmdList,
        ID3D12FencePtr pFence,
        HANDLE fenceEvent,
        uint64_t& fenceValue
    );
    ID3D12ResourcePtr getTopLevelAS() { return this->mpTopLevelAS; }
private:
    ID3D12ResourcePtr mpTopLevelAS;
    vector<pair<ID3D12ResourcePtr, mat4>> mBottomLevelASwithTransform;
    uint64_t mTlasSize = 0;
    Scene* mpScene;
};