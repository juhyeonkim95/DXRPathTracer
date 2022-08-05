#include "SceneAccelerationStructure.h"
#include "DX12AccelerationStructureUtils.h"
#include "DX12Utils.h"


void SceneAccelerationStructure::createSceneAccelerationStructure(
    ID3D12Device5Ptr mpDevice,
    ID3D12CommandQueuePtr mpCmdQueue,
    ID3D12CommandAllocatorPtr pCmdAllocator,
    ID3D12GraphicsCommandList4Ptr mpCmdList,
    ID3D12FencePtr mpFence,
    HANDLE mFenceEvent,
    uint64_t& mFenceValue
)
{
    mBottomLevelASwithTransform.resize(scene->getMeshes().size());

    std::vector<AccelerationStructureBuffers> bottomLevelBuffers;
    for (int i = 0; i < scene->getMeshes().size(); i++) {
        Mesh& mesh = scene->getMeshes().at(i);
        mesh.createMeshBuffer(mpDevice);
        AccelerationStructureBuffers bottomLevelBuffer = createBottomLevelASTriangleMesh(mpDevice, mpCmdList, mesh.getVerticesBuffer(), mesh.getVerticesNumber(), mesh.getIndicesBuffer(), mesh.getIndicesNumber());
        bottomLevelBuffers.push_back(bottomLevelBuffer);
        
        mBottomLevelASwithTransform[i] = (make_pair(bottomLevelBuffer.pResult, mesh.transform));
    }

    AccelerationStructureBuffers topLevelBuffers = createTopLevelAS(mpDevice, mpCmdList, mBottomLevelASwithTransform, mTlasSize);

    // TODO : Flush(?)
    // The tutorial doesn't have any resource lifetime management, so we flush and sync here. This is not required by the DXR spec - you can submit the list whenever you like as long as you take care of the resources lifetime.
    mFenceValue = submitCommandList(mpCmdList, mpCmdQueue, mpFence, mFenceValue);
    mpFence->SetEventOnCompletion(mFenceValue, mFenceEvent);
    WaitForSingleObject(mFenceEvent, INFINITE);
    mpCmdList->Reset(pCmdAllocator, nullptr);

    // Store the AS buffers. The rest of the buffers will be released once we exit the function
    mpTopLevelAS = topLevelBuffers.pResult;
    //mpBottomLevelAS = bottomLevelBuffers.pResult;
}