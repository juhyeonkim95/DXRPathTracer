#include "NRDDeltaTransmissionMotionVector.h"
#include <map>
#include "DX12Utils.h"
#include "DX12BufferUtils.h"

NRDDeltaTransmissionMotionVector::NRDDeltaTransmissionMotionVector(ID3D12Device5Ptr mpDevice, uvec2 size)
    : PostProcessPass(mpDevice, size)
{
    // Create Shaders
    std::vector<DXGI_FORMAT> rtvFormats = { DXGI_FORMAT_R16G16_UNORM, DXGI_FORMAT_R32_FLOAT };
    this->motionVectorShader = new Shader(kQuadVertexShader, L"RenderPass/NRDDeltaTransmissionMotionVector/NRDDeltaTransmissionMotionVector.hlsl", mpDevice, 8, rtvFormats);
}

void NRDDeltaTransmissionMotionVector::createRenderTextures(
    HeapData* rtvHeap,
    HeapData* srvHeap)
{
    motionVectorRenderTexture = createRenderTexture(mpDevice, rtvHeap, srvHeap, size, DXGI_FORMAT_R16G16_UNORM);

    historyLengthRenderTexture = createRenderTexture(mpDevice, rtvHeap, srvHeap, size, DXGI_FORMAT_R32_FLOAT);
    historyLengthRenderTexturePrev = createRenderTexture(mpDevice, rtvHeap, srvHeap, size, DXGI_FORMAT_R32_FLOAT);
}

void NRDDeltaTransmissionMotionVector::processGUI()
{

    mDirty = false;
    if (ImGui::CollapsingHeader("NRDDeltaTransmissionMotionVector"))
    {
        mDirty |= ImGui::Checkbox("enable NRDDeltaTransmissionMotionVector", &mEnabled);
    }
}


void NRDDeltaTransmissionMotionVector::forward(RenderContext* pRenderContext, RenderData& renderData)
{
    ID3D12GraphicsCommandList4Ptr mpCmdList = pRenderContext->pCmdList;
    map<string, D3D12_GPU_DESCRIPTOR_HANDLE>& gpuHandles = renderData.gpuHandleDictionary;
    this->setViewPort(mpCmdList);

    // (1) Render to motionVectorRenderTexture !!
    mpCmdList->SetPipelineState(motionVectorShader->getPipelineStateObject());
    mpCmdList->SetGraphicsRootSignature(motionVectorShader->getRootSignature()); // set the root signature

    mpCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(motionVectorRenderTexture->mResource, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));
    mpCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(historyLengthRenderTexture->mResource, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

    D3D12_CPU_DESCRIPTOR_HANDLE motionVectorRTV[2] = { motionVectorRenderTexture->mRtvDescriptorHandle, historyLengthRenderTexture->mRtvDescriptorHandle };
    mpCmdList->OMSetRenderTargets(2, motionVectorRTV, FALSE, nullptr);

    mpCmdList->SetGraphicsRootDescriptorTable(1, gpuHandles.at("gPositionMeshIDPrev"));
    mpCmdList->SetGraphicsRootDescriptorTable(2, gpuHandles.at("gNormalPrev"));
    mpCmdList->SetGraphicsRootDescriptorTable(3, gpuHandles.at("gPositionMeshID"));
    mpCmdList->SetGraphicsRootDescriptorTable(4, gpuHandles.at("gNormal"));
    mpCmdList->SetGraphicsRootDescriptorTable(5, gpuHandles.at("gDeltaTransmissionPositionMeshID"));
    mpCmdList->SetGraphicsRootDescriptorTable(6, gpuHandles.at("gDeltaTransmissionPositionMeshIDPrev"));
    mpCmdList->SetGraphicsRootDescriptorTable(7, historyLengthRenderTexturePrev->getGPUSrvHandler());
    mpCmdList->SetGraphicsRootDescriptorTable(8, gpuHandles.at("gPathType"));

    mpCmdList->SetGraphicsRootConstantBufferView(0, pRenderContext->pSceneResourceManager->getCameraConstantBuffer()->GetGPUVirtualAddress());

    mpCmdList->DrawInstanced(6, 1, 0, 0);
    mpCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(motionVectorRenderTexture->mResource, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));
    mpCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(historyLengthRenderTexture->mResource, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

    renderData.outputGPUHandleDictionary["gHistoryLength"] = historyLengthRenderTexture->getGPUSrvHandler();
    renderData.outputGPUHandleDictionary["gMotionVector"] = motionVectorRenderTexture->getGPUSrvHandler();


    // update buffer
    std::swap(historyLengthRenderTexture, historyLengthRenderTexturePrev);
    return;
}