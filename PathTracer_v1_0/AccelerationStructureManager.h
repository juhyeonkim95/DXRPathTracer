#pragma once
class AccelerationStructureManager
{
public:
    void createAccelerationStructureFromScene(
        Scene* scene,
        ID3D12Device5Ptr mpDevice,
        ID3D12CommandQueuePtr mpCmdQueue,
        ID3D12CommandAllocatorPtr pCmdAllocator,
        ID3D12GraphicsCommandList4Ptr mpCmdList,
        ID3D12FencePtr mpFence,
        HANDLE mFenceEvent,
        uint64_t& mFenceValue
    );
    AccelerationStructureBuffers createTopLevelAccelerationStructure(Scene* scene, ID3D12GraphicsCommandList4Ptr mpCmdList);

};