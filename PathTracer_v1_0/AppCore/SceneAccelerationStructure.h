#pragma once
#include "Framework.h"
#include "Scene.h"

class SceneAccelerationStructure
{
public:
    SceneAccelerationStructure(Scene* scene) { this->scene = scene; };
    void createSceneAccelerationStructure(
        ID3D12Device5Ptr mpDevice,
        ID3D12CommandQueuePtr mpCmdQueue,
        ID3D12CommandAllocatorPtr pCmdAllocator,
        ID3D12GraphicsCommandList4Ptr mpCmdList,
        ID3D12FencePtr mpFence,
        HANDLE mFenceEvent,
        uint64_t& mFenceValue
    );
    ID3D12ResourcePtr getTopLevelAS() { return this->mpTopLevelAS; }
private:
    ID3D12ResourcePtr mpTopLevelAS;
    vector<pair<ID3D12ResourcePtr, mat4>> mBottomLevelASwithTransform;

    // std::vector<ID3D12ResourcePtr> mpBottomLevelAS;
    uint64_t mTlasSize = 0;

    Scene* scene;
};