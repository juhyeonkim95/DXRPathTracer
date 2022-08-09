#include "FXAA.h"

FXAA::FXAA(ID3D12Device5Ptr mpDevice, uvec2 size)
    : PostProcessPass(mpDevice, size)
{
    // Create Shader
    std::vector<DXGI_FORMAT> rtvFormats = { DXGI_FORMAT_R32G32B32A32_FLOAT };
    this->mpShader = new Shader(kQuadVertexShader, L"RenderPass/FXAA/FXAA.hlsl", mpDevice, 2, rtvFormats);

    mParam.qualityEdgeThreshold = 0.166f;
    mParam.qualitySubPix = 0.75f;
    mParam.qualityEdgeThresholdMin = 0.0833f;
    mParam.earlyOut = 1;
    mParam.rcpTexDim = vec2(1.0f/size.x, 1.0f/size.y);

    mDefaultParam = mParam;

    mpParameterBuffer = createBuffer(mpDevice, sizeof(FXAAParameters), D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_GENERIC_READ, kUploadHeapProps);
}

void FXAA::createRenderTextures(
    HeapData* rtvHeap,
    HeapData* srvHeap)
{
    renderTexture = createRenderTexture(mpDevice, rtvHeap, srvHeap, size, DXGI_FORMAT_R32G32B32A32_FLOAT);
}

void FXAA::processGUI()
{
    if (ImGui::CollapsingHeader("FXAA"))
    {
        ImGui::Checkbox("Enable", &mEnabled);

        ImGui::SliderFloat("qualitySubPix", &mParam.qualitySubPix, 0.f, 1.0f);
        ImGui::SliderFloat("qualityEdgeThreshold", &mParam.qualityEdgeThreshold, 0.f, 1.0f);
        ImGui::SliderFloat("qualityEdgeThresholdMin", &mParam.qualityEdgeThresholdMin, 0.f, 1.0f);

        if (ImGui::Button("Reset"))
        {
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

    mpCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(renderTexture->mResource, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));
    mpCmdList->OMSetRenderTargets(1, &renderTexture->mRtvDescriptorHandle, FALSE, nullptr);
   
    mpCmdList->SetGraphicsRootDescriptorTable(1, renderData.gpuHandleDictionary.at("src"));

    this->uploadParams();
    mpCmdList->SetGraphicsRootConstantBufferView(0, mpParameterBuffer->GetGPUVirtualAddress());

    mpCmdList->DrawInstanced(6, 1, 0, 0);
}

void FXAA::uploadParams()
{
    uint8_t* pData;
    d3d_call(mpParameterBuffer->Map(0, nullptr, (void**)&pData));
    memcpy(pData, &mParam, sizeof(FXAAParameters));
    mpParameterBuffer->Unmap(0, nullptr);
}