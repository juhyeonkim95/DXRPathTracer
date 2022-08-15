#include "MotionVectorSpecular.h"
#include <map>
#include "DX12Utils.h"
#include "DX12BufferUtils.h"

MotionVectorSpecular::MotionVectorSpecular(ID3D12Device5Ptr mpDevice, uvec2 size)
    : PostProcessPass(mpDevice, size)
{
    // Create Shaders
    std::vector<DXGI_FORMAT> rtvFormats = { DXGI_FORMAT_R16G16_UNORM, DXGI_FORMAT_R32_FLOAT };
    this->MotionVectorSpecularShader = new Shader(kQuadVertexShader, L"RenderPass/MotionVectorSpecular/MotionVectorSpecular.hlsl", mpDevice, 9, rtvFormats, 2);
    mParameterBuffer = createBuffer(mpDevice, sizeof(MotionVectorSpecularParameters), D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_GENERIC_READ, kUploadHeapProps);

    mParam.normalThreshold = 0.75;
    mParam.positionThreshold = 0.1f;
    mParam.depthThreshold = 0.02f;
}

void MotionVectorSpecular::createRenderTextures(
    HeapData* rtvHeap,
    HeapData* srvHeap)
{
    MotionVectorSpecularRenderTexture = createRenderTexture(mpDevice, rtvHeap, srvHeap, size, DXGI_FORMAT_R16G16_UNORM);

    historyLengthRenderTexture = createRenderTexture(mpDevice, rtvHeap, srvHeap, size, DXGI_FORMAT_R32_FLOAT);
    historyLengthRenderTexturePrev = createRenderTexture(mpDevice, rtvHeap, srvHeap, size, DXGI_FORMAT_R32_FLOAT);
}

void MotionVectorSpecular::processGUI()
{

    mDirty = false;
    if (ImGui::CollapsingHeader("RELAX"))
    {
        mDirty |= ImGui::Checkbox("enable RELAX", &mEnabled);

        mDirty |= ImGui::SliderFloat("normalThreshold", &mParam.normalThreshold, 0.01f, 1.0f);
        mDirty |= ImGui::SliderFloat("positionThreshold", &mParam.positionThreshold, 0.01f, 1.0f);
        mDirty |= ImGui::SliderFloat("depthThreshold", &mParam.depthThreshold, 0.001f, 0.05f);
    }
}


void MotionVectorSpecular::forward(RenderContext* pRenderContext, RenderData& renderData)
{
    ID3D12GraphicsCommandList4Ptr mpCmdList = pRenderContext->pCmdList;
    map<string, D3D12_GPU_DESCRIPTOR_HANDLE>& gpuHandles = renderData.gpuHandleDictionary;
    this->setViewPort(mpCmdList);

    // (1) Render to MotionVectorSpecularRenderTexture !!
    mpCmdList->SetPipelineState(MotionVectorSpecularShader->getPipelineStateObject());
    mpCmdList->SetGraphicsRootSignature(MotionVectorSpecularShader->getRootSignature()); // set the root signature

    mpCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(MotionVectorSpecularRenderTexture->mResource, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));
    mpCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(historyLengthRenderTexture->mResource, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

    D3D12_CPU_DESCRIPTOR_HANDLE MotionVectorSpecularRTV[2] = { MotionVectorSpecularRenderTexture->mRtvDescriptorHandle, historyLengthRenderTexture->mRtvDescriptorHandle };
    mpCmdList->OMSetRenderTargets(2, MotionVectorSpecularRTV, FALSE, nullptr);

    mpCmdList->SetGraphicsRootDescriptorTable(2, gpuHandles.at("gPositionMeshIDPrev"));
    mpCmdList->SetGraphicsRootDescriptorTable(3, gpuHandles.at("gNormalPrev"));
    mpCmdList->SetGraphicsRootDescriptorTable(4, gpuHandles.at("gPositionMeshID"));
    mpCmdList->SetGraphicsRootDescriptorTable(5, gpuHandles.at("gNormal"));
    mpCmdList->SetGraphicsRootDescriptorTable(6, historyLengthRenderTexturePrev->getGPUSrvHandler());
    mpCmdList->SetGraphicsRootDescriptorTable(7, gpuHandles.at("gRoughness"));
    mpCmdList->SetGraphicsRootDescriptorTable(8, gpuHandles.at("gDeltaReflectionPositionMeshID"));
    mpCmdList->SetGraphicsRootDescriptorTable(9, gpuHandles.at("gDeltaReflectionPositionMeshIDPrev"));
    mpCmdList->SetGraphicsRootDescriptorTable(9, gpuHandles.at("gDeltaReflectionMotionVector"));


    mpCmdList->SetGraphicsRootConstantBufferView(0, pRenderContext->pSceneResourceManager->getCameraConstantBuffer()->GetGPUVirtualAddress());

    uint8_t* pData;
    d3d_call(mParameterBuffer->Map(0, nullptr, (void**)&pData));
    memcpy(pData, &mParam, sizeof(MotionVectorSpecularParameters));
    mParameterBuffer->Unmap(0, nullptr);
    mpCmdList->SetGraphicsRootConstantBufferView(1, mParameterBuffer->GetGPUVirtualAddress());

    mpCmdList->DrawInstanced(6, 1, 0, 0);
    mpCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(MotionVectorSpecularRenderTexture->mResource, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));
    mpCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(historyLengthRenderTexture->mResource, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));


    renderData.outputGPUHandleDictionary["gHistoryLength"] = historyLengthRenderTexture->getGPUSrvHandler();
    renderData.outputGPUHandleDictionary["gMotionVector"] = MotionVectorSpecularRenderTexture->getGPUSrvHandler();


    // update buffer
    std::swap(historyLengthRenderTexture, historyLengthRenderTexturePrev);
    return;
}