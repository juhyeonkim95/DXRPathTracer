#include "BlendPass.h"

BlendPass::BlendPass(ID3D12Device5Ptr mpDevice, uvec2 size)
    : PostProcessPass(mpDevice, size)
{
    // Create Shader
    std::vector<DXGI_FORMAT> rtvFormats = { DXGI_FORMAT_R32G32B32A32_FLOAT };
    this->mpShader = new Shader(kQuadVertexShader, L"RenderPass/BlendPass/BlendPass.hlsl", mpDevice, 2, rtvFormats);

    mpParameterBuffer = createBuffer(mpDevice, sizeof(BlendParameters), D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_GENERIC_READ, kUploadHeapProps);
    mEnabled = false;
}

void BlendPass::createRenderTextures(
    HeapData* rtvHeap,
    HeapData* srvHeap)
{
    blendRenderTexture = createRenderTexture(mpDevice, rtvHeap, srvHeap, size, DXGI_FORMAT_R32G32B32A32_FLOAT);
}

void BlendPass::processGUI()
{
    mDirty = false;
    if (ImGui::CollapsingHeader("Blend"))
    {
        bool divide;
        mDirty |= ImGui::Checkbox("Blend with GT Path Tracer", &mEnabled);
        const char* items[] = { "Temporal Blend", "Divide Mode" };
        mDirty |= ImGui::Combo("Blend Mode Mode", &mParam.divide, items, IM_ARRAYSIZE(items), 2);

        if (mParam.divide == 0) 
        {
            mDirty |= ImGui::SliderInt("Max FrameNumber", &maxFrameNumber, 1, 512);
        }
    }
}

void BlendPass::forward(RenderContext* pRenderContext, RenderData& renderData)
{
    ID3D12GraphicsCommandList4Ptr mpCmdList = pRenderContext->pCmdList;
    this->setViewPort(mpCmdList);

    mpCmdList->SetPipelineState(mpShader->getPipelineStateObject());
    mpCmdList->SetGraphicsRootSignature(mpShader->getRootSignature()); // set the root signature

    mpCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(blendRenderTexture->mResource, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));
    mpCmdList->OMSetRenderTargets(1, &blendRenderTexture->mRtvDescriptorHandle, FALSE, nullptr);
   
    mpCmdList->SetGraphicsRootDescriptorTable(1, renderData.gpuHandleDictionary.at("src1"));
    mpCmdList->SetGraphicsRootDescriptorTable(2, renderData.gpuHandleDictionary.at("src2"));

    this->uploadParams();
    mpCmdList->SetGraphicsRootConstantBufferView(0, mpParameterBuffer->GetGPUVirtualAddress());

    mpCmdList->DrawInstanced(6, 1, 0, 0);
    mpCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(blendRenderTexture->mResource, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));
    
    renderData.outputGPUHandleDictionary["blendedTexture"] = blendRenderTexture->getGPUSrvHandler();
}

void BlendPass::uploadParams()
{
    uint8_t* pData;
    d3d_call(mpParameterBuffer->Map(0, nullptr, (void**)&pData));
    memcpy(pData, &mParam, sizeof(BlendParameters));
    mpParameterBuffer->Unmap(0, nullptr);
}

void BlendPass::setAlpha(int frameNumber) 
{
    float alpha = 1.0f - (float)(frameNumber - 1) / (float)maxFrameNumber;

    alpha = alpha > 1 ? 1 : alpha;
    alpha = alpha < 0 ? 0 : alpha;

    this->mParam.alpha = alpha;
}