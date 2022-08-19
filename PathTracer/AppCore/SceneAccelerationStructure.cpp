#include "SceneAccelerationStructure.h"
#include "DX12AccelerationStructureUtils.h"
#include "DX12Utils.h"


void SceneAccelerationStructure::createSceneAccelerationStructure(
    ID3D12Device5Ptr pDevice,
    ID3D12CommandQueuePtr pCmdQueue,
    ID3D12CommandAllocatorPtr pCmdAllocator,
    ID3D12GraphicsCommandList4Ptr pCmdList,
    ID3D12FencePtr pFence,
    HANDLE fenceEvent,
    uint64_t& fenceValue
)
{
    mBottomLevelASwithTransform.resize(mpScene->getMeshes().size());

    std::vector<AccelerationStructureBuffers> bottomLevelBuffers;
    for (int i = 0; i < mpScene->getMeshes().size(); i++) {
        Mesh& mesh = mpScene->getMeshes().at(i);
        mesh.createMeshBuffer(pDevice);
        AccelerationStructureBuffers bottomLevelBuffer = createBottomLevelASTriangleMesh(pDevice, pCmdList, mesh.getVerticesBuffer(), mesh.getVerticesNumber(), mesh.getIndicesBuffer(), mesh.getIndicesNumber());
        bottomLevelBuffers.push_back(bottomLevelBuffer);
        
        mBottomLevelASwithTransform[i] = (make_pair(bottomLevelBuffer.pResult, mesh.transform));
    }

    AccelerationStructureBuffers topLevelBuffers = createTopLevelAS(pDevice, pCmdList, mBottomLevelASwithTransform, mTlasSize);

    // TODO : Flush(?)
    // The tutorial doesn't have any resource lifetime management, so we flush and sync here. This is not required by the DXR spec - you can submit the list whenever you like as long as you take care of the resources lifetime.
    fenceValue = submitCommandList(pCmdList, pCmdQueue, pFence, fenceValue);
    pFence->SetEventOnCompletion(fenceValue, fenceEvent);
    WaitForSingleObject(fenceEvent, INFINITE);
    pCmdList->Reset(pCmdAllocator, nullptr);

    // Store the AS buffers. The rest of the buffers will be released once we exit the function
    mpTopLevelAS = topLevelBuffers.pResult;
}