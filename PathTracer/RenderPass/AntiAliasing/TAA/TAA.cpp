#include "TAA.h"

TAA::TAA(ID3D12Device5Ptr mpDevice, uvec2 size)
    : PostProcessPass(mpDevice, size)
{
    // Create Shader
    std::vector<DXGI_FORMAT> rtvFormats = { DXGI_FORMAT_R32G32B32A32_FLOAT };
    this->mpShader = new Shader(kQuadVertexShader, L"RenderPass/AntiAliasing/TAA/TAA.hlsl", mpDevice, 3, rtvFormats);
    mEnabled = false;

    mParam.gAlpha = 0.1f;
    mParam.gColorBoxSigma = 1.0f;

    mDefaultParam = mParam;

    mpParameterBuffer = createBuffer(mpDevice, sizeof(TAAParameters), D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_GENERIC_READ, kUploadHeapProps);
}

void TAA::createRenderTextures(
    HeapData* rtvHeap,
    HeapData* srvHeap)
{
    mpRenderTexture = createRenderTexture(mpDevice, rtvHeap, srvHeap, size, DXGI_FORMAT_R32G32B32A32_FLOAT);
    mpPrevTexture = createRenderTexture(mpDevice, rtvHeap, srvHeap, size, DXGI_FORMAT_R32G32B32A32_FLOAT);

}

void TAA::processGUI()
{
    mDirty = false;

    if (ImGui::CollapsingHeader("TAA"))
    {
        mDirty |= ImGui::Checkbox("Enable", &mEnabled);

        mDirty |= ImGui::SliderFloat("qualitySubPix", &mParam.gAlpha, 0.001f, 1.0f);
        mDirty |= ImGui::SliderFloat("qualityEdgeThreshold", &mParam.gColorBoxSigma, 0.001f, 15.0f);

        if (ImGui::Button("Reset"))
        {
            mDirty = false;
            mParam = mDefaultParam;
        }
    }
}

void TAA::forward(RenderContext* pRenderContext, RenderData& renderData)
{
    ID3D12GraphicsCommandList4Ptr mpCmdList = pRenderContext->pCmdList;
    this->setViewPort(mpCmdList);

    mpCmdList->SetPipelineState(mpShader->getPipelineStateObject());
    mpCmdList->SetGraphicsRootSignature(mpShader->getRootSignature()); // set the root signature

    mpCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mpRenderTexture->mResource, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));
    mpCmdList->OMSetRenderTargets(1, &mpRenderTexture->mRtvDescriptorHandle, FALSE, nullptr);
   
    mpCmdList->SetGraphicsRootDescriptorTable(1, renderData.gpuHandleDictionary.at("gTexColor"));
    mpCmdList->SetGraphicsRootDescriptorTable(2, renderData.gpuHandleDictionary.at("gTexMotionVec"));
    mpCmdList->SetGraphicsRootDescriptorTable(3, mpPrevTexture->getGPUSrvHandler());


    this->uploadParams();
    mpCmdList->SetGraphicsRootConstantBufferView(0, mpParameterBuffer->GetGPUVirtualAddress());

    mpCmdList->DrawInstanced(6, 1, 0, 0);
    mpCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mpRenderTexture->mResource, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

    renderData.outputGPUHandleDictionary["gOutput"] = mpRenderTexture->getGPUSrvHandler();

    std::swap(mpRenderTexture, mpPrevTexture);
}

void TAA::uploadParams()
{
    uint8_t* pData;
    d3d_call(mpParameterBuffer->Map(0, nullptr, (void**)&pData));
    memcpy(pData, &mParam, sizeof(TAAParameters));
    mpParameterBuffer->Unmap(0, nullptr);
}