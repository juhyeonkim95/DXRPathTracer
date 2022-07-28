#include "SVGFPass.h"
#include <map>
#include "DX12Initializer.h"
#include "DX12Helper.h"

SVGFPass::SVGFPass(ID3D12Device5Ptr mpDevice, uvec2 size)
{
	this->mpDevice = mpDevice;

    // Create Shaders
    this->motionVectorShader = new Shader(L"QuadVertexShader.hlsl", L"SVGFMotionVector.hlsl", mpDevice, 5);
    this->temporalAccumulationShader = new Shader(L"QuadVertexShader.hlsl", L"SVGFTemporalAccumulation.hlsl", mpDevice, 5);
    this->waveletShader = new Shader(L"QuadVertexShader.hlsl", L"SVGFATrousWavelet.hlsl", mpDevice, 3);
    this->reconstructionShader = new Shader(L"QuadVertexShader.hlsl", L"SVGFReconstruction.hlsl", mpDevice, 4);

    this->size = size;
}

void SVGFPass::createRenderTextures(
    ID3D12DescriptorHeapPtr pRTVHeap,
    uint32_t& usedRTVHeapEntries,
    ID3D12DescriptorHeapPtr pSRVHeap,
    uint32_t& usedSRVHeapEntries)
{

    motionVectorRenderTexture = createRenderTexture(mpDevice, pRTVHeap, usedRTVHeapEntries, pSRVHeap, usedSRVHeapEntries, size, DXGI_FORMAT_R32G32B32A32_FLOAT);
    historyLengthRenderTexture = createRenderTexture(mpDevice, pRTVHeap, usedRTVHeapEntries, pSRVHeap, usedSRVHeapEntries, size, DXGI_FORMAT_R16_FLOAT);

    temporalAccumulationTextureDirect = createRenderTexture(mpDevice, pRTVHeap, usedRTVHeapEntries, pSRVHeap, usedSRVHeapEntries, size, DXGI_FORMAT_R32G32B32A32_FLOAT);
    temporalAccumulationTextureIndirect = createRenderTexture(mpDevice, pRTVHeap, usedRTVHeapEntries, pSRVHeap, usedSRVHeapEntries, size, DXGI_FORMAT_R32G32B32A32_FLOAT);
    temporalAccumulationTextureDirectMoment = createRenderTexture(mpDevice, pRTVHeap, usedRTVHeapEntries, pSRVHeap, usedSRVHeapEntries, size, DXGI_FORMAT_R32G32B32A32_FLOAT);
    temporalAccumulationTextureIndirectMoment = createRenderTexture(mpDevice, pRTVHeap, usedRTVHeapEntries, pSRVHeap, usedSRVHeapEntries, size, DXGI_FORMAT_R32G32B32A32_FLOAT);


    for (int i = 0; i < waveletCount; i++) {
        RenderTexture* waveletDirecti = createRenderTexture(mpDevice, pRTVHeap, usedRTVHeapEntries, pSRVHeap, usedSRVHeapEntries, size, DXGI_FORMAT_R32G32B32A32_FLOAT);
        RenderTexture* waveletIndirecti = createRenderTexture(mpDevice, pRTVHeap, usedRTVHeapEntries, pSRVHeap, usedSRVHeapEntries, size, DXGI_FORMAT_R32G32B32A32_FLOAT);
        this->waveletDirect.push_back(waveletDirecti);
        this->waveletIndirect.push_back(waveletIndirecti);
    }

    this->reconstructionRenderTexture = createRenderTexture(mpDevice, pRTVHeap, usedRTVHeapEntries, pSRVHeap, usedSRVHeapEntries, size, DXGI_FORMAT_R32G32B32A32_FLOAT);
}

void SVGFPass::forward(
    ID3D12GraphicsCommandList4Ptr mpCmdList,
    std::map<string, D3D12_GPU_DESCRIPTOR_HANDLE> gpuHandles,
    std::map<string, ID3D12ResourcePtr> resourceBuffers,
    ID3D12ResourcePtr mpCameraConstantBuffer
)
{
    // (1) Render to motionVectorRenderTexture !!
    mpCmdList->SetPipelineState(motionVectorShader->getPipelineStateObject());
    mpCmdList->SetGraphicsRootSignature(motionVectorShader->getRootSignature()); // set the root signature

    mpCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(motionVectorRenderTexture->mResource, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));
    mpCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(historyLengthRenderTexture->mResource, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

    D3D12_CPU_DESCRIPTOR_HANDLE motionVectorRTV[2] = { motionVectorRenderTexture->mRtvDescriptorHandle, historyLengthRenderTexture->mRtvDescriptorHandle };
    mpCmdList->OMSetRenderTargets(2, motionVectorRTV, FALSE, nullptr);

    mpCmdList->SetGraphicsRootDescriptorTable(1, gpuHandles.at("gPositionMeshIDPrev"));
    mpCmdList->SetGraphicsRootDescriptorTable(2, gpuHandles.at("gNormalPrev"));
    mpCmdList->SetGraphicsRootDescriptorTable(3, gpuHandles.at("gPositionMeshID"));
    mpCmdList->SetGraphicsRootDescriptorTable(4, gpuHandles.at("gNormal"));
    mpCmdList->SetGraphicsRootDescriptorTable(5, gpuHandles.at("gHistoryLengthPrev"));

    mpCmdList->SetGraphicsRootConstantBufferView(0, mpCameraConstantBuffer->GetGPUVirtualAddress());

    mpCmdList->DrawInstanced(6, 1, 0, 0);
    mpCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(motionVectorRenderTexture->mResource, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));
    mpCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(historyLengthRenderTexture->mResource, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

    // Update HistoryLength
    resourceBarrier(mpCmdList, historyLengthRenderTexture->mResource, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_COPY_SOURCE);
    resourceBarrier(mpCmdList, resourceBuffers["gHistoryLengthPrev"], D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_DEST);
    mpCmdList->CopyResource(resourceBuffers["gHistoryLengthPrev"], historyLengthRenderTexture->mResource);
    resourceBarrier(mpCmdList, historyLengthRenderTexture->mResource, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

    // (2) Temporal Accumulation
    // Direct
    mpCmdList->SetPipelineState(temporalAccumulationShader->getPipelineStateObject());
    mpCmdList->SetGraphicsRootSignature(temporalAccumulationShader->getRootSignature()); // set the root signature

    mpCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(temporalAccumulationTextureDirect->mResource, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));
    mpCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(temporalAccumulationTextureDirectMoment->mResource, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));
    resourceBarrier(mpCmdList, resourceBuffers["gDirectIlluminationColorHistory"], D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    resourceBarrier(mpCmdList, resourceBuffers["gDirectIlluminationMomentHistory"], D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

    D3D12_CPU_DESCRIPTOR_HANDLE handles[2] = { temporalAccumulationTextureDirect->mRtvDescriptorHandle, temporalAccumulationTextureDirectMoment->mRtvDescriptorHandle };
    mpCmdList->OMSetRenderTargets(2, handles, FALSE, nullptr);

    mpCmdList->SetGraphicsRootDescriptorTable(1, gpuHandles.at("gDirectIlluminationColorHistory"));
    mpCmdList->SetGraphicsRootDescriptorTable(2, motionVectorRenderTexture->getGPUSrvHandler());
    mpCmdList->SetGraphicsRootDescriptorTable(3, gpuHandles.at("gDirectIllumination"));
    mpCmdList->SetGraphicsRootDescriptorTable(4, gpuHandles.at("gDirectIlluminationMomentHistory"));
    mpCmdList->SetGraphicsRootDescriptorTable(5, historyLengthRenderTexture->getGPUSrvHandler());

    mpCmdList->DrawInstanced(6, 1, 0, 0);

    mpCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(temporalAccumulationTextureDirect->mResource, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));
    mpCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(temporalAccumulationTextureDirectMoment->mResource, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

    // Update Direct Moment History
    resourceBarrier(mpCmdList, temporalAccumulationTextureDirectMoment->mResource, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_COPY_SOURCE);
    resourceBarrier(mpCmdList, resourceBuffers["gDirectIlluminationMomentHistory"], D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_DEST);
    mpCmdList->CopyResource(resourceBuffers["gDirectIlluminationMomentHistory"], temporalAccumulationTextureDirectMoment->mResource);

    // Indirect
    mpCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(temporalAccumulationTextureIndirect->mResource, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));
    mpCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(temporalAccumulationTextureIndirectMoment->mResource, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));
    resourceBarrier(mpCmdList, resourceBuffers["gIndirectIlluminationColorHistory"], D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

    D3D12_CPU_DESCRIPTOR_HANDLE handlesIndirect[2] = { temporalAccumulationTextureIndirect->mRtvDescriptorHandle, temporalAccumulationTextureIndirectMoment->mRtvDescriptorHandle };
    mpCmdList->OMSetRenderTargets(2, handlesIndirect, FALSE, nullptr);

    mpCmdList->SetGraphicsRootDescriptorTable(1, gpuHandles.at("gIndirectIlluminationColorHistory"));
    mpCmdList->SetGraphicsRootDescriptorTable(2, motionVectorRenderTexture->getGPUSrvHandler());
    mpCmdList->SetGraphicsRootDescriptorTable(3, gpuHandles.at("gIndirectIllumination"));
    mpCmdList->SetGraphicsRootDescriptorTable(4, gpuHandles.at("gIndirectIlluminationMomentHistory"));

    mpCmdList->DrawInstanced(6, 1, 0, 0);

    mpCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(temporalAccumulationTextureIndirect->mResource, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));
    mpCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(temporalAccumulationTextureIndirectMoment->mResource, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

    // Update Indirect Moment History
    resourceBarrier(mpCmdList, temporalAccumulationTextureIndirectMoment->mResource, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_COPY_SOURCE);
    resourceBarrier(mpCmdList, resourceBuffers["gIndirectIlluminationMomentHistory"], D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_DEST);
    mpCmdList->CopyResource(resourceBuffers["gIndirectIlluminationMomentHistory"], temporalAccumulationTextureIndirectMoment->mResource);



    // (3) Wavelet
    mpCmdList->SetPipelineState(waveletShader->getPipelineStateObject());
    mpCmdList->SetGraphicsRootSignature(waveletShader->getRootSignature()); // set the root signature

    mpCmdList->SetGraphicsRootDescriptorTable(2, gpuHandles.at("gNormal"));
    mpCmdList->SetGraphicsRootDescriptorTable(3, gpuHandles.at("gPositionMeshID"));


    if (!this->mpWaveletParameterBuffer)
        mpWaveletParameterBuffer = createBuffer(mpDevice, sizeof(WaveletShaderParameters), D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_GENERIC_READ, kUploadHeapProps);


    // Direct
    for (int i = 0; i < waveletCount; i++) {
        mpCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(waveletDirect[i]->mResource, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

        mpCmdList->OMSetRenderTargets(1, &waveletDirect[i]->mRtvDescriptorHandle, FALSE, nullptr);
        if (i == 0) {
            mpCmdList->SetGraphicsRootDescriptorTable(1, temporalAccumulationTextureDirect->getGPUSrvHandler());
        }
        else {
            mpCmdList->SetGraphicsRootDescriptorTable(1, waveletDirect[i - 1]->getGPUSrvHandler());
        }
        WaveletShaderParameters waveletParam;
        waveletParam.level = i;
        waveletParam.texelSize = vec2(1.0f / size.x, 1.0f / size.y);

        uint8_t* pData;
        d3d_call(mpWaveletParameterBuffer->Map(0, nullptr, (void**)&pData));
        memcpy(pData, &waveletParam, sizeof(WaveletShaderParameters));
        mpWaveletParameterBuffer->Unmap(0, nullptr);

        mpCmdList->SetGraphicsRootConstantBufferView(0, mpWaveletParameterBuffer->GetGPUVirtualAddress());

        mpCmdList->DrawInstanced(6, 1, 0, 0);
        mpCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(waveletDirect[i]->mResource, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));
    }

    // Update Direct Color History
    resourceBarrier(mpCmdList, waveletDirect[0]->mResource, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_COPY_SOURCE);
    resourceBarrier(mpCmdList, resourceBuffers["gDirectIlluminationColorHistory"], D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_DEST);
    mpCmdList->CopyResource(resourceBuffers["gDirectIlluminationColorHistory"], waveletDirect[0]->mResource);

    // Indirect
    for (int i = 0; i < waveletCount; i++) {
        mpCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(waveletIndirect[i]->mResource, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

        mpCmdList->OMSetRenderTargets(1, &waveletIndirect[i]->mRtvDescriptorHandle, FALSE, nullptr);
        if (i == 0) {
            mpCmdList->SetGraphicsRootDescriptorTable(1, temporalAccumulationTextureIndirect->getGPUSrvHandler());
        }
        else {
            mpCmdList->SetGraphicsRootDescriptorTable(1, waveletIndirect[i - 1]->getGPUSrvHandler());
        }
        WaveletShaderParameters waveletParam;
        waveletParam.level = i;
        waveletParam.texelSize = vec2(1.0f / size.x, 1.0f / size.y);

        uint8_t* pData;
        d3d_call(mpWaveletParameterBuffer->Map(0, nullptr, (void**)&pData));
        memcpy(pData, &waveletParam, sizeof(WaveletShaderParameters));
        mpWaveletParameterBuffer->Unmap(0, nullptr);

        mpCmdList->SetGraphicsRootConstantBufferView(0, mpWaveletParameterBuffer->GetGPUVirtualAddress());

        mpCmdList->DrawInstanced(6, 1, 0, 0);
        mpCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(waveletIndirect[i]->mResource, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));
    }

    // Update Indirect Color History
    resourceBarrier(mpCmdList, waveletIndirect[0]->mResource, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_COPY_SOURCE);
    resourceBarrier(mpCmdList, resourceBuffers["gIndirectIlluminationColorHistory"], D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_DEST);
    mpCmdList->CopyResource(resourceBuffers["gIndirectIlluminationColorHistory"], waveletIndirect[0]->mResource);

    // (4) Reconstruction!
    mpCmdList->SetPipelineState(reconstructionShader->getPipelineStateObject());
    mpCmdList->SetGraphicsRootSignature(reconstructionShader->getRootSignature()); // set the root signature

    RenderTexture* directRenderTexture = waveletDirect[waveletDirect.size() - 1];
    RenderTexture* indirectRenderTexture = waveletIndirect[waveletIndirect.size() - 1];


    mpCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(reconstructionRenderTexture->mResource, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

    mpCmdList->OMSetRenderTargets(1, &reconstructionRenderTexture->mRtvDescriptorHandle, FALSE, nullptr);

    mpCmdList->SetGraphicsRootDescriptorTable(1, directRenderTexture->getGPUSrvHandler());
    mpCmdList->SetGraphicsRootDescriptorTable(2, indirectRenderTexture->getGPUSrvHandler());
    mpCmdList->SetGraphicsRootDescriptorTable(3, gpuHandles.at("gReflectance"));
    mpCmdList->SetGraphicsRootDescriptorTable(4, gpuHandles.at("gOutputHDR"));

    mpCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(reconstructionRenderTexture->mResource, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));
    mpCmdList->DrawInstanced(6, 1, 0, 0);

    return;
}