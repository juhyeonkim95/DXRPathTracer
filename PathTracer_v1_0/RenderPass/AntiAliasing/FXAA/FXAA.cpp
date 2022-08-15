#include "FXAA.h"

FXAA::FXAA(ID3D12Device5Ptr mpDevice, uvec2 size)
    : PostProcessPass(mpDevice, size)
{
    // Create Shader
    std::vector<DXGI_FORMAT> rtvFormats = { DXGI_FORMAT_R32G32B32A32_FLOAT };
    this->mpShader = new Shader(kQuadVertexShader, L"RenderPass/AntiAliasing/FXAA/FXAA.hlsl", mpDevice, 1, rtvFormats);

    mParam.qualityEdgeThreshold = 0.166f;
    mParam.qualitySubPix = 0.75f;
    mParam.qualityEdgeThresholdMin = 0.0833f;
    mParam.earlyOut = 1;
    mParam.rcpTexDim = vec2(1.0f/size.x, 1.0f/size.y);

    mDefaultParam = mParam;
    mEnabled = false;

    mpParameterBuffer = createBuffer(mpDevice, sizeof(FXAAParameters), D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_GENERIC_READ, kUploadHeapProps);
}

void FXAA::createRenderTextures(
    HeapData* rtvHeap,
    HeapData* srvHeap)
{
    mpRenderTexture = createRenderTexture(mpDevice, rtvHeap, srvHeap, size, DXGI_FORMAT_R32G32B32A32_FLOAT);
}

void FXAA::processGUI()
{
    mDirty = false;
    if (ImGui::CollapsingHeader("FXAA"))
    {
        mDirty |= ImGui::Checkbox("Enable", &mEnabled);

        mDirty |= ImGui::SliderFloat("qualitySubPix", &mParam.qualitySubPix, 0.f, 1.0f);
        mDirty |= ImGui::SliderFloat("qualityEdgeThreshold", &mParam.qualityEdgeThreshold, 0.f, 1.0f);
        mDirty |= ImGui::SliderFloat("qualityEdgeThresholdMin", &mParam.qualityEdgeThresholdMin, 0.f, 1.0f);

        if (ImGui::Button("Reset"))
        {
            mDirty = true;
            mParam = mDefaultParam;
        }
    }
}

void FXAA::forward(RenderContext* pRenderContext, RenderData& renderData)
{
    ID3D12GraphicsCommandList4Ptr mpCmdList = pRenderContext->pCmdList;
    this->setViewPort(mpCmdList);

    mpCmdList->SetPipelineState(mpShader->getPipelineStateObject());
    mpCmdList->SetGraphicsRootSignature(mpShader->getRootSignature()); // set the root signature

    mpCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mpRenderTexture->mResource, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));
    mpCmdList->OMSetRenderTargets(1, &mpRenderTexture->mRtvDescriptorHandle, FALSE, nullptr);
   
    mpCmdList->SetGraphicsRootDescriptorTable(1, renderData.gpuHandleDictionary.at("gSrc"));

    this->uploadParams();
    mpCmdList->SetGraphicsRootConstantBufferView(0, mpParameterBuffer->GetGPUVirtualAddress());

    mpCmdList->DrawInstanced(6, 1, 0, 0);
    mpCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mpRenderTexture->mResource, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

    renderData.outputGPUHandleDictionary["gOutput"] = mpRenderTexture->getGPUSrvHandler();

}

void FXAA::uploadParams()
{
    uint8_t* pData;
    d3d_call(mpParameterBuffer->Map(0, nullptr, (void**)&pData));
    memcpy(pData, &mParam, sizeof(FXAAParameters));
    mpParameterBuffer->Unmap(0, nullptr);
}