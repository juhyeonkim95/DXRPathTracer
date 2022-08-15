#include "RELAXPass.h"
#include <map>
#include "DX12Utils.h"
#include "DX12BufferUtils.h"
#include "BSDF/BSDFLobes.h"

RELAXPass::RELAXPass(ID3D12Device5Ptr mpDevice, uvec2 size, RELAX_TYPE relaxType, uint targetPathType, std::string name)
    : PostProcessPass(mpDevice, size)
{
    this->relaxType = relaxType;
    this->name = name;

    const wchar_t* temporalAccumulationShaderFile;
    const wchar_t* waveletShaderFile;
    const wchar_t* disocclusionFixShaderFile;

    if (relaxType == RELAX_TYPE::RELAX_SPECULAR)
    {
        temporalAccumulationShaderFile = L"RenderPass/RELAX/RELAXSpecularTemporalAccumulation.hlsl";
        waveletShaderFile = L"RenderPass/RELAX/RELAXSpecularAtrousWavelet.hlsl";
        disocclusionFixShaderFile = L"RenderPass/RELAX/RELAXSpecularDisocclusionFix.hlsl";
    }
    else
    {
        temporalAccumulationShaderFile = L"RenderPass/RELAX/RELAXDiffuseTemporalAccumulation.hlsl";
        waveletShaderFile = L"RenderPass/RELAX/RELAXDiffuseAtrousWavelet.hlsl";
        disocclusionFixShaderFile = L"RenderPass/RELAX/RELAXDiffuseDisocclusionFix.hlsl";
    }

    std::vector<DXGI_FORMAT> rtvFormats = { DXGI_FORMAT_R32G32B32A32_FLOAT, DXGI_FORMAT_R32G32_FLOAT,  DXGI_FORMAT_R32_FLOAT };
    this->temporalAccumulationShader = new Shader(kQuadVertexShader, temporalAccumulationShaderFile, mpDevice, 11, rtvFormats, 2);

    rtvFormats = { DXGI_FORMAT_R32G32B32A32_FLOAT };
    this->waveletShader = new Shader(kQuadVertexShader, waveletShaderFile, mpDevice, 8, rtvFormats, 2);

    rtvFormats = { DXGI_FORMAT_R32G32B32A32_FLOAT };
    this->varianceFilterShader = new Shader(kQuadVertexShader, disocclusionFixShaderFile, mpDevice, 7, rtvFormats, 2);

    for (int i = 0; i < kMaxWaveletCount; i++)
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

    mWaveletCount = 3;
    mFeedbackTap = 0;
    mEnableVarianceFilter = true;

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
        mDirty |= ImGui::SliderInt("waveletCount", &mWaveletCount, 0, kMaxWaveletCount);
        mDirty |= ImGui::SliderInt("Feedback Tap", &mFeedbackTap, 0, std::max(0, mWaveletCount));
        mDirty |= ImGui::SliderFloat("sigmaP", &mParam.sigmaP, 0.01f, 16.0f);
        mDirty |= ImGui::SliderFloat("sigmaN", &mParam.sigmaN, 0.01f, 256.0f);
        mDirty |= ImGui::SliderFloat("sigmaL", &mParam.sigmaL, 0.01f, 16.0f);

        if (ImGui::Button("Reset"))
        {
            mParam = mDefaultParam;
            mWaveletCount = 3;
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

    mpCmdList->SetGraphicsRootDescriptorTable(2, temporalAccumulationTexturePrev->getGPUSrvHandler());
    mpCmdList->SetGraphicsRootDescriptorTable(3, gpuHandles.at("gRadiance"));
    mpCmdList->SetGraphicsRootDescriptorTable(4, temporalAccumulationTextureMomentPrev->getGPUSrvHandler());
    mpCmdList->SetGraphicsRootDescriptorTable(5, historyLengthRenderTexturePrev->getGPUSrvHandler());
    mpCmdList->SetGraphicsRootDescriptorTable(6, gpuHandles.at("gMotionVector"));

    mpCmdList->SetGraphicsRootDescriptorTable(7, gpuHandles.at("gPositionMeshID"));
    mpCmdList->SetGraphicsRootDescriptorTable(8, gpuHandles.at("gPositionMeshIDPrev"));
    mpCmdList->SetGraphicsRootDescriptorTable(9, gpuHandles.at("gNormal"));
    mpCmdList->SetGraphicsRootDescriptorTable(10, gpuHandles.at("gNormalPrev"));
    mpCmdList->SetGraphicsRootDescriptorTable(11, gpuHandles.at("gPathType"));

    if (relaxType == RELAX_TYPE::RELAX_SPECULAR)
    {
        mpCmdList->SetGraphicsRootDescriptorTable(12, gpuHandles.at("gRoughness"));
    }

    this->uploadParams(0);
    mpCmdList->SetGraphicsRootConstantBufferView(0, mParamBuffers[0]->GetGPUVirtualAddress());
    mpCmdList->SetGraphicsRootConstantBufferView(1, pRenderContext->pSceneResourceManager->getCameraConstantBuffer()->GetGPUVirtualAddress());

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

        mpCmdList->SetGraphicsRootDescriptorTable(2, temporalAccumulationTexture->getGPUSrvHandler());
        mpCmdList->SetGraphicsRootDescriptorTable(3, gpuHandles.at("gNormal"));
        mpCmdList->SetGraphicsRootDescriptorTable(4, gpuHandles.at("gPositionMeshID"));
        mpCmdList->SetGraphicsRootDescriptorTable(5, temporalAccumulationTextureMoment->getGPUSrvHandler());
        mpCmdList->SetGraphicsRootDescriptorTable(6, historyLengthRenderTexture->getGPUSrvHandler());
        mpCmdList->SetGraphicsRootDescriptorTable(7, gpuHandles.at("gDepthDerivative"));
        mpCmdList->SetGraphicsRootDescriptorTable(8, gpuHandles.at("gPathType"));

        this->uploadParams(0);
        mpCmdList->SetGraphicsRootConstantBufferView(0, mParamBuffers.at(0)->GetGPUVirtualAddress());
        mpCmdList->SetGraphicsRootConstantBufferView(1, pRenderContext->pSceneResourceManager->getCameraConstantBuffer()->GetGPUVirtualAddress());

        mpCmdList->DrawInstanced(6, 1, 0, 0);
        mpCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(temporalAccumulationTextureVarianceFilter->mResource, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));
        std::swap(temporalAccumulationTextureVarianceFilter, temporalAccumulationTexture);
    }

    // (3) Wavelet
    mpCmdList->SetPipelineState(waveletShader->getPipelineStateObject());
    mpCmdList->SetGraphicsRootSignature(waveletShader->getRootSignature()); // set the root signature

    mpCmdList->SetGraphicsRootConstantBufferView(1, pRenderContext->pSceneResourceManager->getCameraConstantBuffer()->GetGPUVirtualAddress());
    mpCmdList->SetGraphicsRootDescriptorTable(3, gpuHandles.at("gNormal"));
    mpCmdList->SetGraphicsRootDescriptorTable(4, gpuHandles.at("gPositionMeshID"));
    mpCmdList->SetGraphicsRootDescriptorTable(5, gpuHandles.at("gDepthDerivative"));
    mpCmdList->SetGraphicsRootDescriptorTable(6, gpuHandles.at("gPathType"));

    if (relaxType == RELAX_TYPE::RELAX_SPECULAR)
    {
        mpCmdList->SetGraphicsRootDescriptorTable(7, gpuHandles.at("gDeltaReflectionNormal"));
        mpCmdList->SetGraphicsRootDescriptorTable(8, gpuHandles.at("gDeltaReflectionPositionMeshID"));
        mpCmdList->SetGraphicsRootDescriptorTable(9, gpuHandles.at("gRoughness"));
    }
    

    // (3) Wavelet
    for (int i = 0; i < mWaveletCount; i++) {
        mpCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(waveletPingPong1->mResource, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));
        mpCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(waveletPingPong2->mResource, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));

        mpCmdList->OMSetRenderTargets(1, &waveletPingPong1->mRtvDescriptorHandle, FALSE, nullptr);
        if (i == 0) {
            // mpCmdList->SetGraphicsRootDescriptorTable(1, gpuHandles.at("gOutputHDR"));
            mpCmdList->SetGraphicsRootDescriptorTable(2, temporalAccumulationTexture->getGPUSrvHandler());
        }
        else {
            mpCmdList->SetGraphicsRootDescriptorTable(2, waveletPingPong2->getGPUSrvHandler());
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

    RenderTexture* filteredRadiance = mWaveletCount > 0? waveletPingPong2 : temporalAccumulationTexture;
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