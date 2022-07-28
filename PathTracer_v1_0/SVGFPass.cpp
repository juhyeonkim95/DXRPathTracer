#include "SVGFPass.h"
#include <map>
#include "DX12Initializer.h"
#include "DX12Helper.h"

SVGFPass::SVGFPass(ID3D12Device5Ptr mpDevice, uvec2 size)
    : PostProcessPass(mpDevice, size)
{
    // Create Shaders
    this->motionVectorShader = new Shader(L"QuadVertexShader.hlsl", L"SVGFMotionVector.hlsl", mpDevice, 5);
    this->temporalAccumulationShader = new Shader(L"QuadVertexShader.hlsl", L"SVGFTemporalAccumulation.hlsl", mpDevice, 5);
    this->waveletShader = new Shader(L"QuadVertexShader.hlsl", L"SVGFATrousWavelet.hlsl", mpDevice, 3);
    this->reconstructionShader = new Shader(L"QuadVertexShader.hlsl", L"SVGFReconstruction.hlsl", mpDevice, 4);
    this->varianceFilterShader = new Shader(L"QuadVertexShader.hlsl", L"SVGFFilterVariance.hlsl", mpDevice, 5);

    mpWaveletParameterBuffer = createBuffer(mpDevice, sizeof(WaveletShaderParameters), D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_GENERIC_READ, kUploadHeapProps);
}

void SVGFPass::createRenderTextures(
    ID3D12DescriptorHeapPtr pRTVHeap,
    uint32_t& usedRTVHeapEntries,
    ID3D12DescriptorHeapPtr pSRVHeap,
    uint32_t& usedSRVHeapEntries)
{

    motionVectorRenderTexture = createRenderTexture(mpDevice, pRTVHeap, usedRTVHeapEntries, pSRVHeap, usedSRVHeapEntries, size, DXGI_FORMAT_R32G32B32A32_FLOAT);
    historyLengthRenderTexture = createRenderTexture(mpDevice, pRTVHeap, usedRTVHeapEntries, pSRVHeap, usedSRVHeapEntries, size, DXGI_FORMAT_R16_FLOAT);
    historyLengthRenderTexturePrev = createRenderTexture(mpDevice, pRTVHeap, usedRTVHeapEntries, pSRVHeap, usedSRVHeapEntries, size, DXGI_FORMAT_R16_FLOAT);

    temporalAccumulationTextureDirect = createRenderTexture(mpDevice, pRTVHeap, usedRTVHeapEntries, pSRVHeap, usedSRVHeapEntries, size, DXGI_FORMAT_R32G32B32A32_FLOAT);
    temporalAccumulationTextureIndirect = createRenderTexture(mpDevice, pRTVHeap, usedRTVHeapEntries, pSRVHeap, usedSRVHeapEntries, size, DXGI_FORMAT_R32G32B32A32_FLOAT);
    temporalAccumulationTextureDirectPrev = createRenderTexture(mpDevice, pRTVHeap, usedRTVHeapEntries, pSRVHeap, usedSRVHeapEntries, size, DXGI_FORMAT_R32G32B32A32_FLOAT);
    temporalAccumulationTextureIndirectPrev = createRenderTexture(mpDevice, pRTVHeap, usedRTVHeapEntries, pSRVHeap, usedSRVHeapEntries, size, DXGI_FORMAT_R32G32B32A32_FLOAT);
    temporalAccumulationTextureDirectVarianceFilter = createRenderTexture(mpDevice, pRTVHeap, usedRTVHeapEntries, pSRVHeap, usedSRVHeapEntries, size, DXGI_FORMAT_R32G32B32A32_FLOAT);
    temporalAccumulationTextureIndirectVarianceFilter = createRenderTexture(mpDevice, pRTVHeap, usedRTVHeapEntries, pSRVHeap, usedSRVHeapEntries, size, DXGI_FORMAT_R32G32B32A32_FLOAT);

    temporalAccumulationTextureDirectMoment = createRenderTexture(mpDevice, pRTVHeap, usedRTVHeapEntries, pSRVHeap, usedSRVHeapEntries, size, DXGI_FORMAT_R32G32B32A32_FLOAT);
    temporalAccumulationTextureIndirectMoment = createRenderTexture(mpDevice, pRTVHeap, usedRTVHeapEntries, pSRVHeap, usedSRVHeapEntries, size, DXGI_FORMAT_R32G32B32A32_FLOAT);
    temporalAccumulationTextureDirectMomentPrev = createRenderTexture(mpDevice, pRTVHeap, usedRTVHeapEntries, pSRVHeap, usedSRVHeapEntries, size, DXGI_FORMAT_R32G32B32A32_FLOAT);
    temporalAccumulationTextureIndirectMomentPrev = createRenderTexture(mpDevice, pRTVHeap, usedRTVHeapEntries, pSRVHeap, usedSRVHeapEntries, size, DXGI_FORMAT_R32G32B32A32_FLOAT);


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
    mpCmdList->SetGraphicsRootDescriptorTable(5, historyLengthRenderTexturePrev->getGPUSrvHandler());

    mpCmdList->SetGraphicsRootConstantBufferView(0, mpCameraConstantBuffer->GetGPUVirtualAddress());

    mpCmdList->DrawInstanced(6, 1, 0, 0);
    mpCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(motionVectorRenderTexture->mResource, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));
    mpCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(historyLengthRenderTexture->mResource, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

    // (2) Temporal Accumulation
    // Direct
    mpCmdList->SetPipelineState(temporalAccumulationShader->getPipelineStateObject());
    mpCmdList->SetGraphicsRootSignature(temporalAccumulationShader->getRootSignature()); // set the root signature

    mpCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(temporalAccumulationTextureDirect->mResource, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));
    mpCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(temporalAccumulationTextureDirectMoment->mResource, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));
    
    D3D12_CPU_DESCRIPTOR_HANDLE handles[2] = { temporalAccumulationTextureDirect->mRtvDescriptorHandle, temporalAccumulationTextureDirectMoment->mRtvDescriptorHandle };
    mpCmdList->OMSetRenderTargets(2, handles, FALSE, nullptr);

    mpCmdList->SetGraphicsRootDescriptorTable(1, temporalAccumulationTextureDirectPrev->getGPUSrvHandler());
    mpCmdList->SetGraphicsRootDescriptorTable(2, motionVectorRenderTexture->getGPUSrvHandler());
    mpCmdList->SetGraphicsRootDescriptorTable(3, gpuHandles.at("gDirectIllumination"));
    mpCmdList->SetGraphicsRootDescriptorTable(4, temporalAccumulationTextureDirectMomentPrev->getGPUSrvHandler());
    mpCmdList->SetGraphicsRootDescriptorTable(5, historyLengthRenderTexture->getGPUSrvHandler());

    mpCmdList->DrawInstanced(6, 1, 0, 0);

    mpCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(temporalAccumulationTextureDirect->mResource, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));
    mpCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(temporalAccumulationTextureDirectMoment->mResource, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));


    // Indirect
    mpCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(temporalAccumulationTextureIndirect->mResource, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));
    mpCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(temporalAccumulationTextureIndirectMoment->mResource, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));
    
    D3D12_CPU_DESCRIPTOR_HANDLE handlesIndirect[2] = { temporalAccumulationTextureIndirect->mRtvDescriptorHandle, temporalAccumulationTextureIndirectMoment->mRtvDescriptorHandle };
    mpCmdList->OMSetRenderTargets(2, handlesIndirect, FALSE, nullptr);

    mpCmdList->SetGraphicsRootDescriptorTable(1, temporalAccumulationTextureIndirectPrev->getGPUSrvHandler());
    mpCmdList->SetGraphicsRootDescriptorTable(2, motionVectorRenderTexture->getGPUSrvHandler());
    mpCmdList->SetGraphicsRootDescriptorTable(3, gpuHandles.at("gIndirectIllumination"));
    mpCmdList->SetGraphicsRootDescriptorTable(4, temporalAccumulationTextureIndirectMomentPrev->getGPUSrvHandler());

    mpCmdList->DrawInstanced(6, 1, 0, 0);

    mpCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(temporalAccumulationTextureIndirect->mResource, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));
    mpCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(temporalAccumulationTextureIndirectMoment->mResource, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

    bool doVarianceFiltering = true;

    if (doVarianceFiltering) {
        // (2.5) Filter variance if length is <4
        // direct
        mpCmdList->SetPipelineState(varianceFilterShader->getPipelineStateObject());
        mpCmdList->SetGraphicsRootSignature(varianceFilterShader->getRootSignature()); // set the root signature

        mpCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(temporalAccumulationTextureDirectVarianceFilter->mResource, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

        mpCmdList->OMSetRenderTargets(1, &temporalAccumulationTextureDirectVarianceFilter->mRtvDescriptorHandle, FALSE, nullptr);

        mpCmdList->SetGraphicsRootDescriptorTable(1, temporalAccumulationTextureDirect->getGPUSrvHandler());
        mpCmdList->SetGraphicsRootDescriptorTable(2, gpuHandles.at("gNormal"));
        mpCmdList->SetGraphicsRootDescriptorTable(3, gpuHandles.at("gPositionMeshID"));
        mpCmdList->SetGraphicsRootDescriptorTable(4, temporalAccumulationTextureDirectMoment->getGPUSrvHandler());
        mpCmdList->SetGraphicsRootDescriptorTable(5, historyLengthRenderTexture->getGPUSrvHandler());

        WaveletShaderParameters waveletParam;
        waveletParam.texelSize = vec2(1.0f / size.x, 1.0f / size.y);

        uint8_t* pData;
        d3d_call(mpWaveletParameterBuffer->Map(0, nullptr, (void**)&pData));
        memcpy(pData, &waveletParam, sizeof(WaveletShaderParameters));
        mpWaveletParameterBuffer->Unmap(0, nullptr);

        mpCmdList->SetGraphicsRootConstantBufferView(0, mpWaveletParameterBuffer->GetGPUVirtualAddress());

        mpCmdList->DrawInstanced(6, 1, 0, 0);
        mpCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(temporalAccumulationTextureDirectVarianceFilter->mResource, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));
        std::swap(temporalAccumulationTextureDirectVarianceFilter, temporalAccumulationTextureDirect);

        // indirect
        mpCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(temporalAccumulationTextureIndirectVarianceFilter->mResource, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

        mpCmdList->OMSetRenderTargets(1, &temporalAccumulationTextureIndirectVarianceFilter->mRtvDescriptorHandle, FALSE, nullptr);

        mpCmdList->SetGraphicsRootDescriptorTable(1, temporalAccumulationTextureIndirect->getGPUSrvHandler());
        mpCmdList->SetGraphicsRootDescriptorTable(4, temporalAccumulationTextureIndirectMoment->getGPUSrvHandler());

        mpCmdList->DrawInstanced(6, 1, 0, 0);
        mpCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(temporalAccumulationTextureIndirectVarianceFilter->mResource, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));
        std::swap(temporalAccumulationTextureIndirectVarianceFilter, temporalAccumulationTextureIndirect);
    }

    // (3) Wavelet
    mpCmdList->SetPipelineState(waveletShader->getPipelineStateObject());
    mpCmdList->SetGraphicsRootSignature(waveletShader->getRootSignature()); // set the root signature

    mpCmdList->SetGraphicsRootDescriptorTable(2, gpuHandles.at("gNormal"));
    mpCmdList->SetGraphicsRootDescriptorTable(3, gpuHandles.at("gPositionMeshID"));
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


    // update buffer
    std::swap(historyLengthRenderTexture, historyLengthRenderTexturePrev);
    std::swap(temporalAccumulationTextureDirectMoment, temporalAccumulationTextureDirectMomentPrev);
    std::swap(temporalAccumulationTextureIndirectMoment, temporalAccumulationTextureIndirectMomentPrev);
    std::swap(waveletDirect[0], temporalAccumulationTextureDirectPrev);
    std::swap(waveletIndirect[0], temporalAccumulationTextureIndirectPrev);

    return;
}