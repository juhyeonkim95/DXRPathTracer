#include "MotionVectorDeltaReflection.h"
#include <map>
#include "DX12Utils.h"
#include "DX12BufferUtils.h"

MotionVectorDeltaReflection::MotionVectorDeltaReflection(ID3D12Device5Ptr mpDevice, uvec2 size)
    : PostProcessPass(mpDevice, size)
{
    // Create Shaders
    std::vector<DXGI_FORMAT> rtvFormats = { DXGI_FORMAT_R32G32_FLOAT };
    this->mpMotionVectorShader = new Shader(kQuadVertexShader, L"RenderPass/MotionVectorDeltaReflection/MotionVectorDeltaReflection.hlsl", mpDevice, 7, rtvFormats);
}

void MotionVectorDeltaReflection::createRenderTextures(
    HeapData* rtvHeap,
    HeapData* srvHeap)
{
    mpMotionVectorRenderTexture = createRenderTexture(mpDevice, rtvHeap, srvHeap, size, DXGI_FORMAT_R32G32_FLOAT);
}

void MotionVectorDeltaReflection::processGUI()
{

    mDirty = false;
    if (ImGui::CollapsingHeader("MotionVectorDeltaReflection"))
    {
        mDirty |= ImGui::Checkbox("enable MotionVectorDeltaReflection", &mEnabled);
    }
}


void MotionVectorDeltaReflection::forward(RenderContext* pRenderContext, RenderData& renderData)
{
    ID3D12GraphicsCommandList4Ptr mpCmdList = pRenderContext->pCmdList;
    map<string, D3D12_GPU_DESCRIPTOR_HANDLE>& gpuHandles = renderData.gpuHandleDictionary;
    this->setViewPort(mpCmdList);

    // (1) Render to motionVectorRenderTexture !!
    mpCmdList->SetPipelineState(mpMotionVectorShader->getPipelineStateObject());
    mpCmdList->SetGraphicsRootSignature(mpMotionVectorShader->getRootSignature()); // set the root signature

    mpCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mpMotionVectorRenderTexture->mResource, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

    D3D12_CPU_DESCRIPTOR_HANDLE motionVectorRTV[1] = { mpMotionVectorRenderTexture->mRtvDescriptorHandle };
    mpCmdList->OMSetRenderTargets(1, motionVectorRTV, FALSE, nullptr);

    mpCmdList->SetGraphicsRootDescriptorTable(1, gpuHandles.at("gPositionMeshIDPrev"));
    mpCmdList->SetGraphicsRootDescriptorTable(2, gpuHandles.at("gNormalDepthPrev"));
    mpCmdList->SetGraphicsRootDescriptorTable(3, gpuHandles.at("gPositionMeshID"));
    mpCmdList->SetGraphicsRootDescriptorTable(4, gpuHandles.at("gNormalDepth"));
    mpCmdList->SetGraphicsRootDescriptorTable(5, gpuHandles.at("gDeltaReflectionPositionMeshID"));
    mpCmdList->SetGraphicsRootDescriptorTable(6, gpuHandles.at("gDeltaReflectionPositionMeshIDPrev"));
    mpCmdList->SetGraphicsRootDescriptorTable(7, gpuHandles.at("gPathType"));

    mpCmdList->SetGraphicsRootConstantBufferView(0, pRenderContext->pSceneResourceManager->getCameraConstantBuffer()->GetGPUVirtualAddress());

    mpCmdList->DrawInstanced(6, 1, 0, 0);
    mpCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mpMotionVectorRenderTexture->mResource, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

    renderData.outputGPUHandleDictionary["gMotionVector"] = mpMotionVectorRenderTexture->getGPUSrvHandler();

    return;
}