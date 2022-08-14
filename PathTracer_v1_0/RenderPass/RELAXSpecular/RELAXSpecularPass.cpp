#include "RELAXSpecularPass.h"
#include <map>
#include "DX12Utils.h"
#include "DX12BufferUtils.h"

RELAXSpecularPass::RELAXSpecularPass(ID3D12Device5Ptr mpDevice, uvec2 size, uint targetPathType, std::string name)
    : PostProcessPass(mpDevice, size)
{
    this->name = name;
    std::vector<DXGI_FORMAT> rtvFormats = { DXGI_FORMAT_R32G32B32A32_FLOAT, DXGI_FORMAT_R32G32_FLOAT };
    this->temporalAccumulationShader = new Shader(kQuadVertexShader, L"RenderPass/RELAXSpecular/RELAXSpecularTemporalAccumulation.hlsl", mpDevice, 7, rtvFormats);

    rtvFormats = { DXGI_FORMAT_R32G32B32A32_FLOAT };
    this->waveletShader = new Shader(kQuadVertexShader, L"RenderPass/RELAXSpecular/RELAXSpecularATrousWavelet.hlsl", mpDevice, 8, rtvFormats);

    rtvFormats = { DXGI_FORMAT_R32G32B32A32_FLOAT };
    this->varianceFilterShader = new Shader(kQuadVertexShader, L"RenderPass/RELAXSpecular/RELAXSpecularFilterVariance.hlsl", mpDevice, 7, rtvFormats);

    for (int i = 0; i < maxWaveletCount; i++)
    {
        mRELAXSpecularParameterBuffers.push_back(createBuffer(mpDevice, sizeof(RELAXSpecularParameters), D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_GENERIC_READ, kUploadHeapProps));
    }
    
    param.maxAccumulatedFrame = 32;
    param.sigmaP = 1.0f;
    param.sigmaN = 128.0f;
    param.sigmaL = 4.0f;
    param.roughnessMultiplier = 5.0f;
    param.screenSize = ivec2(size.x, size.y);
    param.targetPathType = targetPathType;
    defaultParam = param;
}

void RELAXSpecularPass::createRenderTextures(
    HeapData* rtvHeap,
    HeapData* srvHeap)
{
    temporalAccumulationTexture = createRenderTexture(mpDevice, rtvHeap, srvHeap, size, DXGI_FORMAT_R32G32B32A32_FLOAT);
    temporalAccumulationTexturePrev = createRenderTexture(mpDevice, rtvHeap, srvHeap, size, DXGI_FORMAT_R32G32B32A32_FLOAT);
    temporalAccumulationTextureVarianceFilter = createRenderTexture(mpDevice, rtvHeap, srvHeap, size, DXGI_FORMAT_R32G32B32A32_FLOAT);

    temporalAccumulationTextureMoment = createRenderTexture(mpDevice, rtvHeap, srvHeap, size, DXGI_FORMAT_R32G32_FLOAT);
    temporalAccumulationTextureMomentPrev = createRenderTexture(mpDevice, rtvHeap, srvHeap, size, DXGI_FORMAT_R32G32_FLOAT);


    waveletPingPong1 = createRenderTexture(mpDevice, rtvHeap, srvHeap, size, DXGI_FORMAT_R32G32B32A32_FLOAT);
    waveletPingPong2 = createRenderTexture(mpDevice, rtvHeap, srvHeap, size, DXGI_FORMAT_R32G32B32A32_FLOAT);
}

void RELAXSpecularPass::processGUI()
{

    mDirty = false;
    if (ImGui::CollapsingHeader(this->name.c_str()))
    {
        mDirty |= ImGui::Checkbox("enable RELAXSpecular", &mEnabled);
        mDirty |= ImGui::Checkbox("enableVarianceFilter", &mEnableVarianceFilter);


        mDirty |= ImGui::SliderFloat("sigmaP", &param.sigmaP, 0.01f, 16.0f);
        mDirty |= ImGui::SliderFloat("sigmaN", &param.sigmaN, 0.01f, 256.0f);
        mDirty |= ImGui::SliderFloat("sigmaL", &param.sigmaL, 0.01f, 16.0f);
        mDirty |= ImGui::SliderFloat("roughnessMultiplier", &param.roughnessMultiplier, 0.01f, 10.0f);

        mDirty |= ImGui::SliderInt("MaxAccumulatedFrame", &param.maxAccumulatedFrame, 1, 64);
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

void RELAXSpecularPass::uploadParams(uint32_t index)
{
    uint8_t* pData;
    d3d_call(mRELAXSpecularParameterBuffers.at(index)->Map(0, nullptr, (void**)&pData));
    memcpy(pData, &param, sizeof(RELAXSpecularParameters));
    mRELAXSpecularParameterBuffers.at(index)->Unmap(0, nullptr);
}

void RELAXSpecularPass::forward(RenderContext* pRenderContext, RenderData& renderData)
{
    ID3D12GraphicsCommandList4Ptr mpCmdList = pRenderContext->pCmdList;
    map<string, D3D12_GPU_DESCRIPTOR_HANDLE>& gpuHandles = renderData.gpuHandleDictionary;
    this->setViewPort(mpCmdList);


    // (1) Temporal Accumulation
    mpCmdList->SetPipelineState(temporalAccumulationShader->getPipelineStateObject());
    mpCmdList->SetGraphicsRootSignature(temporalAccumulationShader->getRootSignature()); // set the root signature

    mpCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(temporalAccumulationTexture->mResource, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));
    mpCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(temporalAccumulationTextureMoment->mResource, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

    D3D12_CPU_DESCRIPTOR_HANDLE handles[2] = { temporalAccumulationTexture->mRtvDescriptorHandle, temporalAccumulationTextureMoment->mRtvDescriptorHandle };
    mpCmdList->OMSetRenderTargets(2, handles, FALSE, nullptr);

    mpCmdList->SetGraphicsRootDescriptorTable(1, temporalAccumulationTexturePrev->getGPUSrvHandler());
    mpCmdList->SetGraphicsRootDescriptorTable(2, gpuHandles.at("gMotionVector"));
    mpCmdList->SetGraphicsRootDescriptorTable(3, gpuHandles.at("gRadiance"));
    mpCmdList->SetGraphicsRootDescriptorTable(4, temporalAccumulationTextureMomentPrev->getGPUSrvHandler());
    mpCmdList->SetGraphicsRootDescriptorTable(5, gpuHandles.at("gHistoryLength"));
    mpCmdList->SetGraphicsRootDescriptorTable(6, gpuHandles.at("gDeltaReflectionMotionVector"));
    mpCmdList->SetGraphicsRootDescriptorTable(7, gpuHandles.at("gRoughness"));

    this->uploadParams(0);
    mpCmdList->SetGraphicsRootConstantBufferView(0, mRELAXSpecularParameterBuffers[0]->GetGPUVirtualAddress());

    mpCmdList->DrawInstanced(6, 1, 0, 0);

    mpCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(temporalAccumulationTexture->mResource, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));
    mpCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(temporalAccumulationTextureMoment->mResource, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

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
        mpCmdList->SetGraphicsRootDescriptorTable(5, gpuHandles.at("gHistoryLength"));
        mpCmdList->SetGraphicsRootDescriptorTable(6, gpuHandles.at("gDepthDerivative"));
        mpCmdList->SetGraphicsRootDescriptorTable(7, gpuHandles.at("gPathType"));

        this->uploadParams(0);
        mpCmdList->SetGraphicsRootConstantBufferView(0, mRELAXSpecularParameterBuffers.at(0)->GetGPUVirtualAddress());

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
    mpCmdList->SetGraphicsRootDescriptorTable(5, gpuHandles.at("gDeltaReflectionNormal"));
    mpCmdList->SetGraphicsRootDescriptorTable(5, gpuHandles.at("gDeltaReflectionPositionMeshID"));
    mpCmdList->SetGraphicsRootDescriptorTable(5, gpuHandles.at("gRoughness"));


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
        param.stepSize = 1 << i;
        uploadParams(i);

        mpCmdList->SetGraphicsRootConstantBufferView(0, mRELAXSpecularParameterBuffers.at(i)->GetGPUVirtualAddress());

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
    // std::swap(temporalAccumulationTextureSpecularMoment, temporalAccumulationTextureSpecularMomentPrev);

    if (mFeedbackTap == 0)
    {
        std::swap(temporalAccumulationTexture, temporalAccumulationTexturePrev);
        // std::swap(temporalAccumulationTextureSpecular, temporalAccumulationTextureSpecularPrev);
    }
    return;
}