#include "MotionVectorDeltaTransmission.h"
#include <map>
#include "DX12Utils.h"
#include "DX12BufferUtils.h"

MotionVectorDeltaTransmission::MotionVectorDeltaTransmission(ID3D12Device5Ptr mpDevice, uvec2 size)
    : PostProcessPass(mpDevice, size)
{
    // Create Shaders
    std::vector<DXGI_FORMAT> rtvFormats = { DXGI_FORMAT_R32G32_FLOAT };
    this->motionVectorShader = new Shader(kQuadVertexShader, L"RenderPass/MotionVectorDeltaTransmission/MotionVectorDeltaTransmission.hlsl", mpDevice, 7, rtvFormats);
}

void MotionVectorDeltaTransmission::createRenderTextures(
    HeapData* rtvHeap,
    HeapData* srvHeap)
{
    motionVectorRenderTexture = createRenderTexture(mpDevice, rtvHeap, srvHeap, size, DXGI_FORMAT_R32G32_FLOAT);
}

void MotionVectorDeltaTransmission::processGUI()
{

    mDirty = false;
    if (ImGui::CollapsingHeader("MotionVectorDeltaTransmission"))
    {
        mDirty |= ImGui::Checkbox("enable MotionVectorDeltaTransmission", &mEnabled);
    }
}


void MotionVectorDeltaTransmission::forward(RenderContext* pRenderContext, RenderData& renderData)
{
    ID3D12GraphicsCommandList4Ptr mpCmdList = pRenderContext->pCmdList;
    map<string, D3D12_GPU_DESCRIPTOR_HANDLE>& gpuHandles = renderData.gpuHandleDictionary;
    this->setViewPort(mpCmdList);

    // (1) Render to motionVectorRenderTexture !!
    mpCmdList->SetPipelineState(motionVectorShader->getPipelineStateObject());
    mpCmdList->SetGraphicsRootSignature(motionVectorShader->getRootSignature()); // set the root signature

    mpCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(motionVectorRenderTexture->mResource, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

    mpCmdList->OMSetRenderTargets(1, &motionVectorRenderTexture->mRtvDescriptorHandle, FALSE, nullptr);

    mpCmdList->SetGraphicsRootDescriptorTable(1, gpuHandles.at("gPositionMeshIDPrev"));
    mpCmdList->SetGraphicsRootDescriptorTable(2, gpuHandles.at("gNormalPrev"));
    mpCmdList->SetGraphicsRootDescriptorTable(3, gpuHandles.at("gPositionMeshID"));
    mpCmdList->SetGraphicsRootDescriptorTable(4, gpuHandles.at("gNormal"));
    mpCmdList->SetGraphicsRootDescriptorTable(5, gpuHandles.at("gDeltaTransmissionPositionMeshID"));
    mpCmdList->SetGraphicsRootDescriptorTable(6, gpuHandles.at("gDeltaTransmissionPositionMeshIDPrev"));
    mpCmdList->SetGraphicsRootDescriptorTable(7, gpuHandles.at("gPathType"));

    mpCmdList->SetGraphicsRootConstantBufferView(0, pRenderContext->pSceneResourceManager->getCameraConstantBuffer()->GetGPUVirtualAddress());

    mpCmdList->DrawInstanced(6, 1, 0, 0);
    mpCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(motionVectorRenderTexture->mResource, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

    renderData.outputGPUHandleDictionary["gMotionVector"] = motionVectorRenderTexture->getGPUSrvHandler();
    return;
}