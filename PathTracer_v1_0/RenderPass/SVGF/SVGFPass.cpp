#include "SVGFPass.h"
#include <map>
#include "DX12Utils.h"
#include "DX12BufferUtils.h"

SVGFPass::SVGFPass(ID3D12Device5Ptr mpDevice, uvec2 size)
    : PostProcessPass(mpDevice, size)
{
    // Create Shaders
    std::vector<DXGI_FORMAT> rtvFormats = { DXGI_FORMAT_R32G32_FLOAT, DXGI_FORMAT_R32_FLOAT };
    this->motionVectorShader = new Shader(kQuadVertexShader, L"RenderPass/SVGF/SVGFMotionVector.hlsl", mpDevice, 6, rtvFormats);

    rtvFormats = { DXGI_FORMAT_R32G32B32A32_FLOAT, DXGI_FORMAT_R32G32_FLOAT };
    this->temporalAccumulationShader = new Shader(kQuadVertexShader, L"RenderPass/SVGF/SVGFTemporalAccumulation.hlsl", mpDevice, 5, rtvFormats);

    rtvFormats = { DXGI_FORMAT_R32G32B32A32_FLOAT };
    this->waveletShader = new Shader(kQuadVertexShader, L"RenderPass/SVGF/SVGFATrousWavelet.hlsl", mpDevice, 4, rtvFormats);

    rtvFormats = { DXGI_FORMAT_R32G32B32A32_FLOAT, };
    this->reconstructionShader = new Shader(kQuadVertexShader, L"RenderPass/SVGF/SVGFReconstruction.hlsl", mpDevice, 4, rtvFormats);

    rtvFormats = { DXGI_FORMAT_R32G32B32A32_FLOAT };
    this->varianceFilterShader = new Shader(kQuadVertexShader, L"RenderPass/SVGF/SVGFFilterVariance.hlsl", mpDevice, 6, rtvFormats);

    rtvFormats = { DXGI_FORMAT_R32G32_FLOAT };
    this->depthDerivativeShader = new Shader(kQuadVertexShader, L"RenderPass/SVGF/SVGFDepthDerivative.hlsl", mpDevice, 1, rtvFormats);

    mSVGFParameterBuffer = createBuffer(mpDevice, sizeof(SVGFParameters), D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_GENERIC_READ, kUploadHeapProps);

    param.alpha = 0.05f;
    param.momentsAlpha = 0.2f;
    param.sigmaP = 1.0f;
    param.sigmaN = 128.0f;
    param.sigmaL = 4.0f;
    param.texelSize = vec2(1.0f / ((float)size.x), (1.0f / (float)size.y));
    defaultParam = param;
}

void SVGFPass::createRenderTextures(
    HeapData* rtvHeap,
    HeapData* srvHeap)
{
    depthDerivativeTexture = createRenderTexture(mpDevice, rtvHeap, srvHeap, size, DXGI_FORMAT_R32G32_FLOAT);
    motionVectorRenderTexture = createRenderTexture(mpDevice, rtvHeap, srvHeap, size, DXGI_FORMAT_R16G16_UNORM);
    historyLengthRenderTexture = createRenderTexture(mpDevice, rtvHeap, srvHeap, size, DXGI_FORMAT_R32_FLOAT);
    historyLengthRenderTexturePrev = createRenderTexture(mpDevice, rtvHeap, srvHeap, size, DXGI_FORMAT_R32_FLOAT);

    temporalAccumulationTextureDirect = createRenderTexture(mpDevice, rtvHeap, srvHeap, size, DXGI_FORMAT_R32G32B32A32_FLOAT);
    temporalAccumulationTextureIndirect = createRenderTexture(mpDevice, rtvHeap, srvHeap, size, DXGI_FORMAT_R32G32B32A32_FLOAT);
    temporalAccumulationTextureDirectPrev = createRenderTexture(mpDevice, rtvHeap, srvHeap, size, DXGI_FORMAT_R32G32B32A32_FLOAT);
    temporalAccumulationTextureIndirectPrev = createRenderTexture(mpDevice, rtvHeap, srvHeap, size, DXGI_FORMAT_R32G32B32A32_FLOAT);
    temporalAccumulationTextureDirectVarianceFilter = createRenderTexture(mpDevice, rtvHeap, srvHeap, size, DXGI_FORMAT_R32G32B32A32_FLOAT);
    temporalAccumulationTextureIndirectVarianceFilter = createRenderTexture(mpDevice, rtvHeap, srvHeap, size, DXGI_FORMAT_R32G32B32A32_FLOAT);

    temporalAccumulationTextureDirectMoment = createRenderTexture(mpDevice, rtvHeap, srvHeap, size, DXGI_FORMAT_R32G32_FLOAT);
    temporalAccumulationTextureIndirectMoment = createRenderTexture(mpDevice, rtvHeap, srvHeap, size, DXGI_FORMAT_R32G32_FLOAT);
    temporalAccumulationTextureDirectMomentPrev = createRenderTexture(mpDevice, rtvHeap, srvHeap, size, DXGI_FORMAT_R32G32_FLOAT);
    temporalAccumulationTextureIndirectMomentPrev = createRenderTexture(mpDevice, rtvHeap, srvHeap, size, DXGI_FORMAT_R32G32_FLOAT);


    waveletDirectPingPong1 = createRenderTexture(mpDevice, rtvHeap, srvHeap, size, DXGI_FORMAT_R32G32B32A32_FLOAT);
    waveletDirectPingPong2 = createRenderTexture(mpDevice, rtvHeap, srvHeap, size, DXGI_FORMAT_R32G32B32A32_FLOAT);
    waveletIndirectPingPong1 = createRenderTexture(mpDevice, rtvHeap, srvHeap, size, DXGI_FORMAT_R32G32B32A32_FLOAT);
    waveletIndirectPingPong2 = createRenderTexture(mpDevice, rtvHeap, srvHeap, size, DXGI_FORMAT_R32G32B32A32_FLOAT);


    /*for (int i = 0; i < maxWaveletCount; i++) {
        RenderTexture* waveletDirecti = createRenderTexture(mpDevice, rtvHeap, srvHeap, size, DXGI_FORMAT_R32G32B32A32_FLOAT);
        RenderTexture* waveletIndirecti = createRenderTexture(mpDevice, rtvHeap, srvHeap, size, DXGI_FORMAT_R32G32B32A32_FLOAT);
        this->waveletDirect.push_back(waveletDirecti);
        this->waveletIndirect.push_back(waveletIndirecti);
    }*/

    this->reconstructionRenderTexture = createRenderTexture(mpDevice, rtvHeap, srvHeap, size, DXGI_FORMAT_R32G32B32A32_FLOAT);
}

void SVGFPass::processGUI()
{

    mDirty = false;
    if (ImGui::CollapsingHeader("SVGF"))
    {
        mDirty |= ImGui::Checkbox("enable SVGF", &mEnabled);
        mDirty |= ImGui::Checkbox("enableVarianceFilter", &mEnableVarianceFilter);

        mDirty |= ImGui::SliderFloat("sigmaP", &param.sigmaP, 0.01f, 16.0f);
        mDirty |= ImGui::SliderFloat("sigmaN", &param.sigmaN, 0.01f, 256.0f);
        mDirty |= ImGui::SliderFloat("sigmaL", &param.sigmaL, 0.01f, 16.0f);
        mDirty |= ImGui::SliderFloat("alpha", &param.alpha, 0.01f, 1.0f);
        mDirty |= ImGui::SliderFloat("alphaMoment", &param.momentsAlpha, 0.01f, 1.0f);
        mDirty |= ImGui::SliderInt("waveletCount", &waveletCount, 0, maxWaveletCount);
        mDirty |= ImGui::SliderInt("Feedback Tap", &mFeedbackTap, 0, std::max(0, waveletCount));


        if (ImGui::Button("Reset"))
        {
            param = defaultParam;
            waveletCount = 3;
            mFeedbackTap = 0;
            mDirty = true;
        }
    }
}

void SVGFPass::uploadParams()
{
    uint8_t* pData;
    d3d_call(mSVGFParameterBuffer->Map(0, nullptr, (void**)&pData));
    memcpy(pData, &param, sizeof(SVGFParameters));
    mSVGFParameterBuffer->Unmap(0, nullptr);
}

void SVGFPass::forward(RenderContext* pRenderContext, RenderData& renderData)
{
    ID3D12GraphicsCommandList4Ptr mpCmdList = pRenderContext->pCmdList;
    HeapData* pSrvUavHeap = pRenderContext->pSrvUavHeap;
    map<string, D3D12_GPU_DESCRIPTOR_HANDLE>& gpuHandles = pSrvUavHeap->getGPUHandleMap();
    this->setViewPort(mpCmdList);

    // (0) prepare depth derivative
    mpCmdList->SetPipelineState(depthDerivativeShader->getPipelineStateObject());
    mpCmdList->SetGraphicsRootSignature(depthDerivativeShader->getRootSignature()); // set the root signature

    mpCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(depthDerivativeTexture->mResource, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

    mpCmdList->OMSetRenderTargets(1, &depthDerivativeTexture->mRtvDescriptorHandle, FALSE, nullptr);
    ;
    mpCmdList->SetGraphicsRootDescriptorTable(1, gpuHandles.at("gNormal"));

    mpCmdList->DrawInstanced(6, 1, 0, 0);
    mpCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(depthDerivativeTexture->mResource, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));


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
    mpCmdList->SetGraphicsRootDescriptorTable(6, depthDerivativeTexture->getGPUSrvHandler());

    mpCmdList->SetGraphicsRootConstantBufferView(0, pRenderContext->pSceneResourceManager->getCameraConstantBuffer()->GetGPUVirtualAddress());

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
    this->uploadParams();
    mpCmdList->SetGraphicsRootConstantBufferView(0, mSVGFParameterBuffer->GetGPUVirtualAddress());

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

    if (mEnableVarianceFilter) {
        // (2.5) Filter variance if length is <4
        // direct
        mpCmdList->SetPipelineState(varianceFilterShader->getPipelineStateObject());
        mpCmdList->SetGraphicsRootSignature(varianceFilterShader->getRootSignature()); // set the root signature

        mpCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(temporalAccumulationTextureDirectVarianceFilter->mResource, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

        mpCmdList->OMSetRenderTargets(1, &temporalAccumulationTextureDirectVarianceFilter->mRtvDescriptorHandle, FALSE, nullptr);

        mpCmdList->SetGraphicsRootDescriptorTable(1, temporalAccumulationTextureDirect->getGPUSrvHandler());
        // mpCmdList->SetGraphicsRootDescriptorTable(1, gpuHandles.at("gOutputHDR"));

        mpCmdList->SetGraphicsRootDescriptorTable(2, gpuHandles.at("gNormal"));
        mpCmdList->SetGraphicsRootDescriptorTable(3, gpuHandles.at("gPositionMeshID"));
        mpCmdList->SetGraphicsRootDescriptorTable(4, temporalAccumulationTextureDirectMoment->getGPUSrvHandler());
        mpCmdList->SetGraphicsRootDescriptorTable(5, historyLengthRenderTexture->getGPUSrvHandler());
        mpCmdList->SetGraphicsRootDescriptorTable(6, depthDerivativeTexture->getGPUSrvHandler());
        
        this->uploadParams();

        mpCmdList->SetGraphicsRootConstantBufferView(0, mSVGFParameterBuffer->GetGPUVirtualAddress());

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
    mpCmdList->SetGraphicsRootDescriptorTable(4, depthDerivativeTexture->getGPUSrvHandler());


    // Direct
    for (int i = 0; i < waveletCount; i++) {
        mpCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(waveletDirectPingPong1->mResource, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));
        mpCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(waveletDirectPingPong2->mResource, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));

        mpCmdList->OMSetRenderTargets(1, &waveletDirectPingPong1->mRtvDescriptorHandle, FALSE, nullptr);
        if (i == 0) {
            mpCmdList->SetGraphicsRootDescriptorTable(1, gpuHandles.at("gOutputHDR"));
            // mpCmdList->SetGraphicsRootDescriptorTable(1, temporalAccumulationTextureDirect->getGPUSrvHandler());
        }
        else {
            mpCmdList->SetGraphicsRootDescriptorTable(1, waveletDirectPingPong2->getGPUSrvHandler());
        }
        param.stepSize = 1 << i;
        uploadParams();

        mpCmdList->SetGraphicsRootConstantBufferView(0, mSVGFParameterBuffer->GetGPUVirtualAddress());

        mpCmdList->DrawInstanced(6, 1, 0, 0);
        mpCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(waveletDirectPingPong1->mResource, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));
        mpCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(waveletDirectPingPong2->mResource, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_PRESENT));

        if (i + 1 == mFeedbackTap)
        {
            resourceBarrier(mpCmdList, waveletDirectPingPong1->mResource, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_COPY_SOURCE);
            resourceBarrier(mpCmdList, temporalAccumulationTextureDirectPrev->mResource, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_COPY_DEST);
            mpCmdList->CopyResource(temporalAccumulationTextureDirectPrev->mResource, waveletDirectPingPong1->mResource);
            resourceBarrier(mpCmdList, waveletDirectPingPong1->mResource, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_PRESENT);
        }

        std::swap(waveletDirectPingPong1, waveletDirectPingPong2);
    }

    // Indirect
    for (int i = 0; i < waveletCount; i++) {
        mpCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(waveletIndirectPingPong1->mResource, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

        mpCmdList->OMSetRenderTargets(1, &waveletIndirectPingPong1->mRtvDescriptorHandle, FALSE, nullptr);
        if (i == 0) {
            mpCmdList->SetGraphicsRootDescriptorTable(1, temporalAccumulationTextureIndirect->getGPUSrvHandler());
        }
        else {
            mpCmdList->SetGraphicsRootDescriptorTable(1, waveletIndirectPingPong2->getGPUSrvHandler());
        }
        param.stepSize = 1 << i;
        uploadParams();

        mpCmdList->SetGraphicsRootConstantBufferView(0, mSVGFParameterBuffer->GetGPUVirtualAddress());

        mpCmdList->DrawInstanced(6, 1, 0, 0);
        mpCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(waveletIndirectPingPong1->mResource, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

        if (i + 1 == mFeedbackTap)
        {
            resourceBarrier(mpCmdList, waveletIndirectPingPong1->mResource, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_COPY_SOURCE);
            resourceBarrier(mpCmdList, temporalAccumulationTextureIndirectPrev->mResource, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_COPY_DEST);
            mpCmdList->CopyResource(temporalAccumulationTextureIndirectPrev->mResource, waveletIndirectPingPong1->mResource);
            resourceBarrier(mpCmdList, waveletIndirectPingPong1->mResource, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_PRESENT);
        }

        std::swap(waveletIndirectPingPong1, waveletIndirectPingPong2);
    }

    // (4) Reconstruction!
    mpCmdList->SetPipelineState(reconstructionShader->getPipelineStateObject());
    mpCmdList->SetGraphicsRootSignature(reconstructionShader->getRootSignature()); // set the root signature

    RenderTexture* directRenderTexture;
    RenderTexture* indirectRenderTexture;

    if (waveletCount > 0)
    {
        directRenderTexture = waveletDirectPingPong2;
        indirectRenderTexture = waveletIndirectPingPong2;
    }
    else
    {
        directRenderTexture = temporalAccumulationTextureDirect;
        indirectRenderTexture = temporalAccumulationTextureIndirect;
    }



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

    if (mFeedbackTap == 0)
    {
        std::swap(temporalAccumulationTextureDirect, temporalAccumulationTextureDirectPrev);
        std::swap(temporalAccumulationTextureIndirect, temporalAccumulationTextureIndirectPrev);
    }
    return;
}