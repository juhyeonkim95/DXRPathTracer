#include "RELAXPass.h"
#include <map>
#include "DX12Utils.h"
#include "DX12BufferUtils.h"

RELAXPass::RELAXPass(ID3D12Device5Ptr mpDevice, uvec2 size, uint targetPathType, std::string name)
    : PostProcessPass(mpDevice, size)
{
    this->name = name;
    // Create Shaders
    //std::vector<DXGI_FORMAT> rtvFormats = { DXGI_FORMAT_R32G32_FLOAT, DXGI_FORMAT_R32_FLOAT };
    //this->motionVectorShader = new Shader(kQuadVertexShader, L"RenderPass/RELAX/RELAXMotionVector.hlsl", mpDevice, 6, rtvFormats, 2);

    std::vector<DXGI_FORMAT> rtvFormats = { DXGI_FORMAT_R32G32B32A32_FLOAT, DXGI_FORMAT_R32G32_FLOAT,  DXGI_FORMAT_R32_FLOAT };
    this->temporalAccumulationShader = new Shader(kQuadVertexShader, L"RenderPass/RELAX/RELAXTemporalAccumulation.hlsl", mpDevice, 9, rtvFormats);

    rtvFormats = { DXGI_FORMAT_R32G32B32A32_FLOAT };
    this->waveletShader = new Shader(kQuadVertexShader, L"RenderPass/RELAX/RELAXATrousWavelet.hlsl", mpDevice, 5, rtvFormats);

    rtvFormats = { DXGI_FORMAT_R32G32B32A32_FLOAT };
    this->varianceFilterShader = new Shader(kQuadVertexShader, L"RenderPass/RELAX/RELAXFilterVariance.hlsl", mpDevice, 7, rtvFormats);

    for (int i = 0; i < maxWaveletCount; i++)
    {
        mParamBuffers.push_back(createBuffer(mpDevice, sizeof(RELAXParameters), D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_GENERIC_READ, kUploadHeapProps));
    }
    
    mParam.sigmaP = 1.0f;
    mParam.sigmaN = 128.0f;
    mParam.sigmaL = 4.0f;
    mParam.screenSize = ivec2(size.x, size.y);
    mParam.targetPathType = targetPathType;

    mParam.normalThreshold = 0.8f;
    mParam.positionThreshold = 0.1f;
    mParam.depthThreshold = 0.02f;
    mParam.maxAccumulateFrame = 32;

    mDefaultParam = mParam;
}

void RELAXPass::createRenderTextures(
    HeapData* rtvHeap,
    HeapData* srvHeap)
{
    historyLengthRenderTexture = createRenderTexture(mpDevice, rtvHeap, srvHeap, size, DXGI_FORMAT_R32_FLOAT);
    historyLengthRenderTexturePrev = createRenderTexture(mpDevice, rtvHeap, srvHeap, size, DXGI_FORMAT_R32_FLOAT);

    temporalAccumulationTexture = createRenderTexture(mpDevice, rtvHeap, srvHeap, size, DXGI_FORMAT_R32G32B32A32_FLOAT);
    temporalAccumulationTexturePrev = createRenderTexture(mpDevice, rtvHeap, srvHeap, size, DXGI_FORMAT_R32G32B32A32_FLOAT);
    temporalAccumulationTextureVarianceFilter = createRenderTexture(mpDevice, rtvHeap, srvHeap, size, DXGI_FORMAT_R32G32B32A32_FLOAT);

    temporalAccumulationTextureMoment = createRenderTexture(mpDevice, rtvHeap, srvHeap, size, DXGI_FORMAT_R32G32_FLOAT);
    temporalAccumulationTextureMomentPrev = createRenderTexture(mpDevice, rtvHeap, srvHeap, size, DXGI_FORMAT_R32G32_FLOAT);


    waveletPingPong1 = createRenderTexture(mpDevice, rtvHeap, srvHeap, size, DXGI_FORMAT_R32G32B32A32_FLOAT);
    waveletPingPong2 = createRenderTexture(mpDevice, rtvHeap, srvHeap, size, DXGI_FORMAT_R32G32B32A32_FLOAT);
}

void RELAXPass::processGUI()
{

    mDirty = false;
    if (ImGui::CollapsingHeader(this->name.c_str()))
    {
        mDirty |= ImGui::Checkbox("enable RELAX", &mEnabled);
        mDirty |= ImGui::Checkbox("enableVarianceFilter", &mEnableVarianceFilter);

        // Temporal Accumulation
        mDirty |= ImGui::SliderFloat("normalThreshold", &mParam.normalThreshold, 0.01f, 1.0f);
        mDirty |= ImGui::SliderFloat("positionThreshold", &mParam.positionThreshold, 0.01f, 1.0f);
        mDirty |= ImGui::SliderFloat("depthThreshold", &mParam.depthThreshold, 0.001f, 0.05f);
        mDirty |= ImGui::SliderInt("MaxAccumulatedFrame", &mParam.maxAccumulateFrame, 1, 64);

        // Wavelet
        mDirty |= ImGui::SliderInt("waveletCount", &waveletCount, 0, maxWaveletCount);
        mDirty |= ImGui::SliderInt("Feedback Tap", &mFeedbackTap, 0, std::max(0, waveletCount));
        mDirty |= ImGui::SliderFloat("sigmaP", &mParam.sigmaP, 0.01f, 16.0f);
        mDirty |= ImGui::SliderFloat("sigmaN", &mParam.sigmaN, 0.01f, 256.0f);
        mDirty |= ImGui::SliderFloat("sigmaL", &mParam.sigmaL, 0.01f, 16.0f);

        if (ImGui::Button("Reset"))
        {
            mParam = mDefaultParam;
            waveletCount = 3;
            mFeedbackTap = 0;
            mDirty = true;
        }
    }
}

void RELAXPass::uploadParams(uint32_t index)
{
    uint8_t* pData;
    d3d_call(mParamBuffers.at(index)->Map(0, nullptr, (void**)&pData));
    memcpy(pData, &mParam, sizeof(RELAXParameters));
    mParamBuffers.at(index)->Unmap(0, nullptr);
}

void RELAXPass::forward(RenderContext* pRenderContext, RenderData& renderData)
{
    ID3D12GraphicsCommandList4Ptr mpCmdList = pRenderContext->pCmdList;
    map<string, D3D12_GPU_DESCRIPTOR_HANDLE>& gpuHandles = renderData.gpuHandleDictionary;
    this->setViewPort(mpCmdList);


    // (1) Temporal Accumulation
    mpCmdList->SetPipelineState(temporalAccumulationShader->getPipelineStateObject());
    mpCmdList->SetGraphicsRootSignature(temporalAccumulationShader->getRootSignature()); // set the root signature

    mpCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(temporalAccumulationTexture->mResource, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));
    mpCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(temporalAccumulationTextureMoment->mResource, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));
    mpCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(historyLengthRenderTexture->mResource, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

    D3D12_CPU_DESCRIPTOR_HANDLE handles[3] = { temporalAccumulationTexture->mRtvDescriptorHandle, temporalAccumulationTextureMoment->mRtvDescriptorHandle, historyLengthRenderTexture->mRtvDescriptorHandle };
    mpCmdList->OMSetRenderTargets(3, handles, FALSE, nullptr);

    mpCmdList->SetGraphicsRootDescriptorTable(1, temporalAccumulationTexturePrev->getGPUSrvHandler());
    mpCmdList->SetGraphicsRootDescriptorTable(2, gpuHandles.at("gRadiance"));
    mpCmdList->SetGraphicsRootDescriptorTable(3, temporalAccumulationTextureMomentPrev->getGPUSrvHandler());
    mpCmdList->SetGraphicsRootDescriptorTable(4, historyLengthRenderTexturePrev->getGPUSrvHandler());
    mpCmdList->SetGraphicsRootDescriptorTable(5, gpuHandles.at("gMotionVector"));

    mpCmdList->SetGraphicsRootDescriptorTable(6, gpuHandles.at("gPositionMeshID"));
    mpCmdList->SetGraphicsRootDescriptorTable(7, gpuHandles.at("gPositionMeshIDPrev"));
    mpCmdList->SetGraphicsRootDescriptorTable(8, gpuHandles.at("gNormal"));
    mpCmdList->SetGraphicsRootDescriptorTable(9, gpuHandles.at("gNormalPrev"));


    this->uploadParams(0);
    mpCmdList->SetGraphicsRootConstantBufferView(0, mParamBuffers[0]->GetGPUVirtualAddress());
    
    mpCmdList->DrawInstanced(6, 1, 0, 0);

    mpCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(temporalAccumulationTexture->mResource, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));
    mpCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(temporalAccumulationTextureMoment->mResource, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));
    mpCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(historyLengthRenderTexture->mResource, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

    if (mEnableVarianceFilter) {
        // (2.5) Filter variance if length is <4
        // 
        mpCmdList->SetPipelineState(varianceFilterShader->getPipelineStateObject());
        mpCmdList->SetGraphicsRootSignature(varianceFilterShader->getRootSignature()); // set the root signature

        mpCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(temporalAccumulationTextureVarianceFilter->mResource, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

        mpCmdList->OMSetRenderTargets(1, &temporalAccumulationTextureVarianceFilter->mRtvDescriptorHandle, FALSE, nullptr);

        mpCmdList->SetGraphicsRootDescriptorTable(1, temporalAccumulationTexture->getGPUSrvHandler());
        // mpCmdList->SetGraphicsRootDescriptorTable(1, gpuHandles.at("gOutputHDR"));

        mpCmdList->SetGraphicsRootDescriptorTable(2, gpuHandles.at("gNormal"));
        mpCmdList->SetGraphicsRootDescriptorTable(3, gpuHandles.at("gPositionMeshID"));
        mpCmdList->SetGraphicsRootDescriptorTable(4, temporalAccumulationTextureMoment->getGPUSrvHandler());
        mpCmdList->SetGraphicsRootDescriptorTable(5, historyLengthRenderTexture->getGPUSrvHandler());
        mpCmdList->SetGraphicsRootDescriptorTable(6, gpuHandles.at("gDepthDerivative"));
        mpCmdList->SetGraphicsRootDescriptorTable(7, gpuHandles.at("gPathType"));

        this->uploadParams(0);
        mpCmdList->SetGraphicsRootConstantBufferView(0, mParamBuffers.at(0)->GetGPUVirtualAddress());

        mpCmdList->DrawInstanced(6, 1, 0, 0);
        mpCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(temporalAccumulationTextureVarianceFilter->mResource, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));
        std::swap(temporalAccumulationTextureVarianceFilter, temporalAccumulationTexture);
    }

    // (3) Wavelet
    mpCmdList->SetPipelineState(waveletShader->getPipelineStateObject());
    mpCmdList->SetGraphicsRootSignature(waveletShader->getRootSignature()); // set the root signature

    mpCmdList->SetGraphicsRootDescriptorTable(2, gpuHandles.at("gNormal"));
    mpCmdList->SetGraphicsRootDescriptorTable(3, gpuHandles.at("gPositionMeshID"));
    mpCmdList->SetGraphicsRootDescriptorTable(4, gpuHandles.at("gDepthDerivative"));
    mpCmdList->SetGraphicsRootDescriptorTable(5, gpuHandles.at("gPathType"));


    // (3) Wavelet
    for (int i = 0; i < waveletCount; i++) {
        mpCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(waveletPingPong1->mResource, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));
        mpCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(waveletPingPong2->mResource, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));

        mpCmdList->OMSetRenderTargets(1, &waveletPingPong1->mRtvDescriptorHandle, FALSE, nullptr);
        if (i == 0) {
            // mpCmdList->SetGraphicsRootDescriptorTable(1, gpuHandles.at("gOutputHDR"));
            mpCmdList->SetGraphicsRootDescriptorTable(1, temporalAccumulationTexture->getGPUSrvHandler());
        }
        else {
            mpCmdList->SetGraphicsRootDescriptorTable(1, waveletPingPong2->getGPUSrvHandler());
        }
        mParam.stepSize = 1 << i;
        uploadParams(i);

        mpCmdList->SetGraphicsRootConstantBufferView(0, mParamBuffers.at(i)->GetGPUVirtualAddress());

        mpCmdList->DrawInstanced(6, 1, 0, 0);
        mpCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(waveletPingPong1->mResource, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));
        mpCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(waveletPingPong2->mResource, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_PRESENT));

        if (i + 1 == mFeedbackTap)
        {
            resourceBarrier(mpCmdList, waveletPingPong1->mResource, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_COPY_SOURCE);
            resourceBarrier(mpCmdList, temporalAccumulationTexturePrev->mResource, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_COPY_DEST);
            mpCmdList->CopyResource(temporalAccumulationTexturePrev->mResource, waveletPingPong1->mResource);
            resourceBarrier(mpCmdList, waveletPingPong1->mResource, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_PRESENT);
        }

        std::swap(waveletPingPong1, waveletPingPong2);
    }

    RenderTexture* filteredRadiance = waveletCount > 0? waveletPingPong2 : temporalAccumulationTexture;
    renderData.outputGPUHandleDictionary["filteredRadiance"] = filteredRadiance -> getGPUSrvHandler();


    //gpuHandles["gRadiance"] = RenderTexture->getGPUSrvHandler();
    //gpuHandles["gSpecularRadiance"] = specularRenderTexture->getGPUSrvHandler();

    // update buffer
    std::swap(temporalAccumulationTextureMoment, temporalAccumulationTextureMomentPrev);
    std::swap(historyLengthRenderTexture, historyLengthRenderTexturePrev);

    // std::swap(temporalAccumulationTextureSpecularMoment, temporalAccumulationTextureSpecularMomentPrev);

    if (mFeedbackTap == 0)
    {
        std::swap(temporalAccumulationTexture, temporalAccumulationTexturePrev);
        // std::swap(temporalAccumulationTextureSpecular, temporalAccumulationTextureSpecularPrev);
    }
    return;
}