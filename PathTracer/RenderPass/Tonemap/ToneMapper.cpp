#include "ToneMapper.h"

ToneMapper::ToneMapper(ID3D12Device5Ptr mpDevice, uvec2 size)
    : PostProcessPass(mpDevice, size)
{
    // Create Shader
    std::vector<DXGI_FORMAT> rtvFormats = { DXGI_FORMAT_R8G8B8A8_UNORM };
    this->mpShader = new Shader(kQuadVertexShader, L"RenderPass/Tonemap/Tonemap.hlsl", mpDevice, 1, rtvFormats);

    mParam.mode = (int)ToneMapperOperator::Reinhard;
    mParam.whiteMaxLuminance = 1.0f;
    mParam.whiteScale = 11.2f;
    mParam.srgbConversion = true;

    mDefaultParam = mParam;

    mpParameterBuffer = createBuffer(mpDevice, sizeof(TonemapParameters), D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_GENERIC_READ, kUploadHeapProps);

}

void ToneMapper::createRenderTextures(
    ID3D12DescriptorHeapPtr pRTVHeap,
    uint32_t& usedRTVHeapEntries,
    ID3D12DescriptorHeapPtr pSRVHeap,
    uint32_t& usedSRVHeapEntries)
{
    // tonemapRenderTexture = createRenderTexture(mpDevice, pRTVHeap, usedRTVHeapEntries, pSRVHeap, usedSRVHeapEntries, size, DXGI_FORMAT_R32G32B32A32_FLOAT);
}

void ToneMapper::processGUI()
{
    if (ImGui::CollapsingHeader("Tonemap"))
    {
        const char* items[] = { 
            "Linear", 
            "Reinhard", 
            "ReinhardModified", 
            "HejiHableAlu",
            "HableUc2",
            "Aces"
        };
        ImGui::Combo("Tonemap Mode", &mParam.mode, items, IM_ARRAYSIZE(items), 4);

        bool srgbConversion = mParam.srgbConversion;
        ImGui::Checkbox("SRGB conversion", &srgbConversion);
        mParam.srgbConversion = srgbConversion;

        if (mParam.mode == (int)ToneMapperOperator::ReinhardModified) 
        {
            ImGui::SliderFloat("White Max Luminance", &mParam.whiteMaxLuminance, 0.1f, 10.0f);
        }

        if (mParam.mode == (int)ToneMapperOperator::HableUc2)
        {
            ImGui::SliderFloat("White Scale", &mParam.whiteScale, 0.1f, 20.0f);
        }

        if (ImGui::Button("Reset"))
        {
            mParam = mDefaultParam;
        }
    }
}

void ToneMapper::forward(RenderContext* pRenderContext, RenderData& renderData)
{
    ID3D12GraphicsCommandList4Ptr mpCmdList = pRenderContext->pCmdList;
    this->setViewPort(mpCmdList);

    mpCmdList->SetPipelineState(mpShader->getPipelineStateObject());
    mpCmdList->SetGraphicsRootSignature(mpShader->getRootSignature()); // set the root signature

    mpCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(renderData.resourceDictionary.at("dst"), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));
    mpCmdList->OMSetRenderTargets(1, &renderData.cpuHandleDictionary.at("dst"), FALSE, nullptr);
   
    mpCmdList->SetGraphicsRootDescriptorTable(1, renderData.gpuHandleDictionary.at("src"));

    this->uploadParams();
    mpCmdList->SetGraphicsRootConstantBufferView(0, mpParameterBuffer->GetGPUVirtualAddress());

    mpCmdList->DrawInstanced(6, 1, 0, 0);
}

void ToneMapper::uploadParams()
{
    uint8_t* pData;
    d3d_call(mpParameterBuffer->Map(0, nullptr, (void**)&pData));
    memcpy(pData, &mParam, sizeof(TonemapParameters));
    mpParameterBuffer->Unmap(0, nullptr);
}