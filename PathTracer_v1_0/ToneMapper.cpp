#include "ToneMapper.h"

ToneMapper::ToneMapper(ID3D12Device5Ptr mpDevice, uvec2 size)
    : PostProcessPass(mpDevice, size)
{
    // Create Shader
    this->tonemapShader = new Shader(L"QuadVertexShader.hlsl", L"Tonemap.hlsl", mpDevice, 1);
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

}

void ToneMapper::forward(
    ID3D12GraphicsCommandList4Ptr mpCmdList,
    D3D12_GPU_DESCRIPTOR_HANDLE input,
    D3D12_CPU_DESCRIPTOR_HANDLE output,
    ID3D12ResourcePtr outputResource
)
{
    mpCmdList->SetPipelineState(tonemapShader->getPipelineStateObject());
    mpCmdList->SetGraphicsRootSignature(tonemapShader->getRootSignature()); // set the root signature

    mpCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(outputResource, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));
    mpCmdList->OMSetRenderTargets(1, &output, FALSE, nullptr);

    mpCmdList->SetGraphicsRootDescriptorTable(1, input);
    
    mpCmdList->DrawInstanced(6, 1, 0, 0);
}