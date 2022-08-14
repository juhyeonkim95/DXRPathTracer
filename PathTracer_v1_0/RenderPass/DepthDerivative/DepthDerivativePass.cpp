#include "DepthDerivativePass.h"

DepthDerivativePass::DepthDerivativePass(ID3D12Device5Ptr mpDevice, uvec2 size)
    : PostProcessPass(mpDevice, size)
{
    // Create Shader
    std::vector<DXGI_FORMAT> rtvFormats = { DXGI_FORMAT_R32G32_FLOAT };
    this->mpShader = new Shader(kQuadVertexShader, L"RenderPass/DepthDerivative/DepthDerivative.hlsl", mpDevice, 1, rtvFormats);
}

void DepthDerivativePass::createRenderTextures(
    HeapData* rtvHeap,
    HeapData* srvHeap)
{
    depthDerivativeRenderTexture = createRenderTexture(mpDevice, rtvHeap, srvHeap, size, DXGI_FORMAT_R32G32_FLOAT);
}

void DepthDerivativePass::processGUI()
{
    
}

void DepthDerivativePass::forward(RenderContext* pRenderContext, RenderData& renderData)
{
    ID3D12GraphicsCommandList4Ptr mpCmdList = pRenderContext->pCmdList;
    this->setViewPort(mpCmdList);

    mpCmdList->SetPipelineState(mpShader->getPipelineStateObject());
    mpCmdList->SetGraphicsRootSignature(mpShader->getRootSignature()); // set the root signature

    mpCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(depthDerivativeRenderTexture->mResource, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));
    mpCmdList->OMSetRenderTargets(1, &depthDerivativeRenderTexture->mRtvDescriptorHandle, FALSE, nullptr);

    mpCmdList->SetGraphicsRootDescriptorTable(1, renderData.gpuHandleDictionary.at("gNormalDepth"));

    mpCmdList->DrawInstanced(6, 1, 0, 0);

    mpCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(depthDerivativeRenderTexture->mResource, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

    renderData.outputGPUHandleDictionary["gDepthDerivative"] = depthDerivativeRenderTexture->getGPUSrvHandler();
    return;
}