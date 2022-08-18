#include "RELAXSinglePass.h"
#include <map>
#include "DX12Utils.h"
#include "DX12BufferUtils.h"
#include "BSDF/BSDFLobes.h"

RELAXSinglePass::RELAXSinglePass(ID3D12Device5Ptr mpDevice, uvec2 size, std::string name)
    : PostProcessPass(mpDevice, size)
{
    this->name = name;

    const wchar_t* temporalAccumulationShaderFile;
    const wchar_t* waveletShaderFile;
    const wchar_t* disocclusionFixShaderFile;

    temporalAccumulationShaderFile = L"RenderPass/RELAXSingle/RELAXSingleTemporalAccumulation.hlsl";
    waveletShaderFile = L"RenderPass/RELAXSingle/RELAXSingleAtrousWavelet.hlsl";
    disocclusionFixShaderFile = L"RenderPass/RELAXSingle/RELAXSingleDisocclusionFix.hlsl";

    std::vector<DXGI_FORMAT> rtvFormats = { 
        DXGI_FORMAT_R32G32B32A32_FLOAT, DXGI_FORMAT_R32G32B32A32_FLOAT,
        DXGI_FORMAT_R32G32B32A32_FLOAT, DXGI_FORMAT_R32G32B32A32_FLOAT,
        DXGI_FORMAT_R32G32B32A32_FLOAT, DXGI_FORMAT_R32G32B32A32_FLOAT,
        DXGI_FORMAT_R32G32B32A32_FLOAT, DXGI_FORMAT_R32G32B32A32_FLOAT
    };
    this->temporalAccumulationShader = new Shader(kQuadVertexShader, temporalAccumulationShaderFile, mpDevice, 29, rtvFormats, 2);

    rtvFormats = { DXGI_FORMAT_R32G32B32A32_FLOAT, DXGI_FORMAT_R32G32B32A32_FLOAT, DXGI_FORMAT_R32G32B32A32_FLOAT, DXGI_FORMAT_R32G32B32A32_FLOAT };
    this->waveletShader = new Shader(kQuadVertexShader, waveletShaderFile, mpDevice, 13, rtvFormats, 2);


    this->disocclusionFixShader = new Shader(kQuadVertexShader, disocclusionFixShaderFile, mpDevice, 16, rtvFormats, 2);

    for (int i = 0; i < kMaxWaveletCount; i++)
    {
        mParamBuffers.push_back(createBuffer(mpDevice, sizeof(RELAXSingleParameters), D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_GENERIC_READ, kUploadHeapProps));
    }
    
    mParam.sigmaP = 1.0f;
    mParam.sigmaN = 128.0f;
    mParam.sigmaL = 4.0f;
    mParam.screenSize = ivec2(size.x, size.y);
    mParam.targetPathType = BSDF_LOBE_ALL;

    mParam.normalThreshold = 0.8f;
    mParam.positionThreshold = 0.1f;
    mParam.depthThreshold = 0.02f;
    mParam.maxAccumulateFrame = 32;

    mWaveletCount = 3;
    mFeedbackTap = 0;
    mEnableVarianceFilter = true;

    mDefaultParam = mParam;
}

void RELAXSinglePass::createRenderTextures(
    HeapData* rtvHeap,
    HeapData* srvHeap)
{
    for (int i = 0; i < 4; i++)
    {
        RenderTexture* temporalAccumulationTexture = createRenderTexture(mpDevice, rtvHeap, srvHeap, size, DXGI_FORMAT_R32G32B32A32_FLOAT);
        RenderTexture* temporalAccumulationTexturePrev = createRenderTexture(mpDevice, rtvHeap, srvHeap, size, DXGI_FORMAT_R32G32B32A32_FLOAT);
        RenderTexture* temporalAccumulationTextureMomentAndLength = createRenderTexture(mpDevice, rtvHeap, srvHeap, size, DXGI_FORMAT_R32G32B32A32_FLOAT);
        RenderTexture* temporalAccumulationTextureMomentAndLengthPrev = createRenderTexture(mpDevice, rtvHeap, srvHeap, size, DXGI_FORMAT_R32G32B32A32_FLOAT);

        RenderTexture* temporalAccumulationTextureVarianceFilter = createRenderTexture(mpDevice, rtvHeap, srvHeap, size, DXGI_FORMAT_R32G32B32A32_FLOAT);


        RenderTexture* waveletPingPong1 = createRenderTexture(mpDevice, rtvHeap, srvHeap, size, DXGI_FORMAT_R32G32B32A32_FLOAT);
        RenderTexture* waveletPingPong2 = createRenderTexture(mpDevice, rtvHeap, srvHeap, size, DXGI_FORMAT_R32G32B32A32_FLOAT);

        temporalAccumulationTextures.push_back(temporalAccumulationTexture);
        temporalAccumulationTexturesPrev.push_back(temporalAccumulationTexturePrev);
        temporalAccumulationTexturesMomentAndLength.push_back(temporalAccumulationTextureMomentAndLength);
        temporalAccumulationTexturesMomentAndLengthPrev.push_back(temporalAccumulationTextureMomentAndLengthPrev);

        temporalAccumulationTexturesVarianceFilter.push_back(temporalAccumulationTextureVarianceFilter);


        waveletPingPongs1.push_back(waveletPingPong1);
        waveletPingPongs2.push_back(waveletPingPong2);
    }

    
}

void RELAXSinglePass::processGUI()
{

    mDirty = false;
    if (ImGui::CollapsingHeader(this->name.c_str()))
    {
        mDirty |= ImGui::Checkbox("enable RELAXSingle", &mEnabled);
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

void RELAXSinglePass::uploadParams(uint32_t index)
{
    uint8_t* pData;
    d3d_call(mParamBuffers.at(index)->Map(0, nullptr, (void**)&pData));
    memcpy(pData, &mParam, sizeof(RELAXSingleParameters));
    mParamBuffers.at(index)->Unmap(0, nullptr);
}

void RELAXSinglePass::forward(RenderContext* pRenderContext, RenderData& renderData)
{
    ID3D12GraphicsCommandList4Ptr mpCmdList = pRenderContext->pCmdList;
    map<string, D3D12_GPU_DESCRIPTOR_HANDLE>& gpuHandles = renderData.gpuHandleDictionary;
    this->setViewPort(mpCmdList);

    std::chrono::steady_clock::time_point startTime = std::chrono::steady_clock::now();
    std::chrono::steady_clock::time_point now = startTime;
    float elapsedTimeMicrosec;

    // (1) Temporal Accumulation
    mpCmdList->SetPipelineState(temporalAccumulationShader->getPipelineStateObject());
    mpCmdList->SetGraphicsRootSignature(temporalAccumulationShader->getRootSignature()); // set the root signature

    D3D12_CPU_DESCRIPTOR_HANDLE handles[8];
    for (int i = 0; i < 4; i++) {
        mpCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(temporalAccumulationTextures.at(i)->mResource, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));
        mpCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(temporalAccumulationTexturesMomentAndLength.at(i)->mResource, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));
        handles[2 * i + 0] = temporalAccumulationTextures.at(i)->mRtvDescriptorHandle;
        handles[2 * i + 1] = temporalAccumulationTexturesMomentAndLength.at(i)->mRtvDescriptorHandle;
    }
    mpCmdList->OMSetRenderTargets(8, handles, FALSE, nullptr);

    for (int i = 0; i < 4; i++) {
        mpCmdList->SetGraphicsRootDescriptorTable(2 + i, temporalAccumulationTexturesPrev.at(i)->getGPUSrvHandler());
        mpCmdList->SetGraphicsRootDescriptorTable(10 + i, temporalAccumulationTexturesMomentAndLengthPrev.at(i)->getGPUSrvHandler());
    }

    mpCmdList->SetGraphicsRootDescriptorTable(6, gpuHandles.at("gDiffuseRadiance"));
    mpCmdList->SetGraphicsRootDescriptorTable(7, gpuHandles.at("gSpecularRadiance"));
    mpCmdList->SetGraphicsRootDescriptorTable(8, gpuHandles.at("gDeltaReflectionRadiance"));
    mpCmdList->SetGraphicsRootDescriptorTable(9, gpuHandles.at("gDeltaTransmissionRadiance"));

    mpCmdList->SetGraphicsRootDescriptorTable(14, gpuHandles.at("gMotionVector"));
    mpCmdList->SetGraphicsRootDescriptorTable(15, gpuHandles.at("gDeltaReflectionMotionVector"));
    mpCmdList->SetGraphicsRootDescriptorTable(16, gpuHandles.at("gDeltaTransmissionMotionVector"));

    mpCmdList->SetGraphicsRootDescriptorTable(17, gpuHandles.at("gPositionMeshID"));
    mpCmdList->SetGraphicsRootDescriptorTable(18, gpuHandles.at("gDeltaReflectionPositionMeshID"));
    mpCmdList->SetGraphicsRootDescriptorTable(19, gpuHandles.at("gDeltaTransmissionPositionMeshID"));

    mpCmdList->SetGraphicsRootDescriptorTable(20, gpuHandles.at("gPositionMeshIDPrev"));
    mpCmdList->SetGraphicsRootDescriptorTable(21, gpuHandles.at("gDeltaReflectionPositionMeshIDPrev"));
    mpCmdList->SetGraphicsRootDescriptorTable(22, gpuHandles.at("gDeltaTransmissionPositionMeshIDPrev"));

    mpCmdList->SetGraphicsRootDescriptorTable(23, gpuHandles.at("gNormalDepth"));
    mpCmdList->SetGraphicsRootDescriptorTable(24, gpuHandles.at("gDeltaReflectionNormal"));
    mpCmdList->SetGraphicsRootDescriptorTable(25, gpuHandles.at("gDeltaTransmissionNormal"));

    mpCmdList->SetGraphicsRootDescriptorTable(26, gpuHandles.at("gNormalDepthPrev"));
    mpCmdList->SetGraphicsRootDescriptorTable(27, gpuHandles.at("gDeltaReflectionNormalPrev"));
    mpCmdList->SetGraphicsRootDescriptorTable(28, gpuHandles.at("gDeltaTransmissionNormalPrev"));

    mpCmdList->SetGraphicsRootDescriptorTable(29, gpuHandles.at("gPathType"));
    mpCmdList->SetGraphicsRootDescriptorTable(30, gpuHandles.at("gRoughness"));

    this->uploadParams(0);
    mpCmdList->SetGraphicsRootConstantBufferView(0, mParamBuffers[0]->GetGPUVirtualAddress());
    mpCmdList->SetGraphicsRootConstantBufferView(1, pRenderContext->pSceneResourceManager->getCameraConstantBuffer()->GetGPUVirtualAddress());

    mpCmdList->DrawInstanced(6, 1, 0, 0);

    for (int i = 0; i < 4; i++) {
        mpCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(temporalAccumulationTextures.at(i)->mResource, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));
        mpCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(temporalAccumulationTexturesMomentAndLength.at(i)->mResource, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));
    }

    now = std::chrono::steady_clock::now();
    renderData.elapsedTimeRecords.push_back(std::make_pair(this->name + "(Temporal Accumulation)", std::chrono::duration_cast<std::chrono::microseconds>(now - startTime).count()));
    startTime = now;

    if (mEnableVarianceFilter) {
        // (2) Filter variance if length is <4
        // 
        mpCmdList->SetPipelineState(disocclusionFixShader->getPipelineStateObject());
        mpCmdList->SetGraphicsRootSignature(disocclusionFixShader->getRootSignature()); // set the root signature
        D3D12_CPU_DESCRIPTOR_HANDLE handles[4];
        for (int j = 0; j < 4; j++)
        {
            mpCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(temporalAccumulationTexturesVarianceFilter.at(j)->mResource, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));
            handles[j] = temporalAccumulationTexturesVarianceFilter.at(j)->mRtvDescriptorHandle;
            mpCmdList->SetGraphicsRootDescriptorTable(2 + j, temporalAccumulationTextures.at(j)->getGPUSrvHandler());
            mpCmdList->SetGraphicsRootDescriptorTable(6 + j, temporalAccumulationTexturesMomentAndLength.at(j)->getGPUSrvHandler());
        }

        mpCmdList->OMSetRenderTargets(4, handles, FALSE, nullptr);

        mpCmdList->SetGraphicsRootDescriptorTable(10, gpuHandles.at("gPositionMeshID"));
        mpCmdList->SetGraphicsRootDescriptorTable(11, gpuHandles.at("gDeltaReflectionPositionMeshID"));
        mpCmdList->SetGraphicsRootDescriptorTable(12, gpuHandles.at("gDeltaTransmissionPositionMeshID"));

        mpCmdList->SetGraphicsRootDescriptorTable(13, gpuHandles.at("gNormalDepth"));
        mpCmdList->SetGraphicsRootDescriptorTable(14, gpuHandles.at("gDeltaReflectionNormal"));
        mpCmdList->SetGraphicsRootDescriptorTable(15, gpuHandles.at("gDeltaTransmissionNormal"));

        mpCmdList->SetGraphicsRootDescriptorTable(16, gpuHandles.at("gDepthDerivative"));
        mpCmdList->SetGraphicsRootDescriptorTable(17, gpuHandles.at("gPathType"));

        this->uploadParams(0);
        mpCmdList->SetGraphicsRootConstantBufferView(0, mParamBuffers.at(0)->GetGPUVirtualAddress());
        mpCmdList->SetGraphicsRootConstantBufferView(1, pRenderContext->pSceneResourceManager->getCameraConstantBuffer()->GetGPUVirtualAddress());

        mpCmdList->DrawInstanced(6, 1, 0, 0);

        for (int j = 0; j < 4; j++)
        {
            mpCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(temporalAccumulationTexturesVarianceFilter.at(j)->mResource, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));
            
            std::swap(temporalAccumulationTexturesVarianceFilter.at(j), temporalAccumulationTextures.at(j));

            handles[j] = temporalAccumulationTexturesVarianceFilter.at(j)->mRtvDescriptorHandle;
            mpCmdList->SetGraphicsRootDescriptorTable(2 + j, temporalAccumulationTextures.at(j)->getGPUSrvHandler());
            mpCmdList->SetGraphicsRootDescriptorTable(6 + j, temporalAccumulationTexturesMomentAndLength.at(j)->getGPUSrvHandler());
        }

        now = std::chrono::steady_clock::now();
        renderData.elapsedTimeRecords.push_back(std::make_pair(this->name + "(Disocclusion Fix)", std::chrono::duration_cast<std::chrono::microseconds>(now - startTime).count()));
        startTime = now;
    }


    // (3) Wavelet
    mpCmdList->SetPipelineState(waveletShader->getPipelineStateObject());
    mpCmdList->SetGraphicsRootSignature(waveletShader->getRootSignature()); // set the root signature

    mpCmdList->SetGraphicsRootConstantBufferView(1, pRenderContext->pSceneResourceManager->getCameraConstantBuffer()->GetGPUVirtualAddress());
    
    mpCmdList->SetGraphicsRootDescriptorTable(6, gpuHandles.at("gPositionMeshID"));
    mpCmdList->SetGraphicsRootDescriptorTable(7, gpuHandles.at("gDeltaReflectionPositionMeshID"));
    mpCmdList->SetGraphicsRootDescriptorTable(8, gpuHandles.at("gDeltaTransmissionPositionMeshID"));

    mpCmdList->SetGraphicsRootDescriptorTable(9, gpuHandles.at("gNormalDepth"));
    mpCmdList->SetGraphicsRootDescriptorTable(10, gpuHandles.at("gDeltaReflectionNormal"));
    mpCmdList->SetGraphicsRootDescriptorTable(11, gpuHandles.at("gDeltaTransmissionNormal"));


    mpCmdList->SetGraphicsRootDescriptorTable(12, gpuHandles.at("gDepthDerivative"));
    mpCmdList->SetGraphicsRootDescriptorTable(13, gpuHandles.at("gPathType"));
    mpCmdList->SetGraphicsRootDescriptorTable(14, gpuHandles.at("gRoughness"));


    // (3) Wavelet
    for (int i = 0; i < mWaveletCount; i++) {
        D3D12_CPU_DESCRIPTOR_HANDLE handles[4];
        for (int j = 0; j < 4; j++) {
            mpCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(waveletPingPongs1.at(j)->mResource, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));
            mpCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(waveletPingPongs2.at(j)->mResource, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));
            handles[j] = waveletPingPongs1.at(j)->mRtvDescriptorHandle;

            if (i == 0) {
                mpCmdList->SetGraphicsRootDescriptorTable(2 + j, temporalAccumulationTextures.at(j)->getGPUSrvHandler());
            }
            else {
                mpCmdList->SetGraphicsRootDescriptorTable(2 + j, waveletPingPongs2.at(j)->getGPUSrvHandler());
            }
        }
        
        mpCmdList->OMSetRenderTargets(4, handles, FALSE, nullptr);

        mParam.stepSize = 1 << i;
        uploadParams(i);

        mpCmdList->SetGraphicsRootConstantBufferView(0, mParamBuffers.at(i)->GetGPUVirtualAddress());

        mpCmdList->DrawInstanced(6, 1, 0, 0);
        
        for (int j = 0; j < 4; j++) {
            mpCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(waveletPingPongs1.at(j)->mResource, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));
            mpCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(waveletPingPongs2.at(j)->mResource, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_PRESENT));

            if (i + 1 == mFeedbackTap)
            {
                resourceBarrier(mpCmdList, waveletPingPongs1.at(j)->mResource, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_COPY_SOURCE);
                resourceBarrier(mpCmdList, temporalAccumulationTexturesPrev.at(j)->mResource, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_COPY_DEST);
                mpCmdList->CopyResource(temporalAccumulationTexturesPrev.at(j)->mResource, waveletPingPongs1.at(j)->mResource);
                resourceBarrier(mpCmdList, waveletPingPongs1.at(j)->mResource, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_PRESENT);
            }

            std::swap(waveletPingPongs1.at(j), waveletPingPongs2.at(j));
        }
    }
    

    RenderTexture* filteredRadianceDiffuse = mWaveletCount > 0? waveletPingPongs2.at(0) : temporalAccumulationTextures.at(0);
    renderData.outputGPUHandleDictionary["gDiffuseRadianceFiltered"] = filteredRadianceDiffuse -> getGPUSrvHandler();

    RenderTexture* filteredRadianceSpecular = mWaveletCount > 0 ? waveletPingPongs2.at(1) : temporalAccumulationTextures.at(1);
    renderData.outputGPUHandleDictionary["gSpecularRadianceFiltered"] = filteredRadianceSpecular->getGPUSrvHandler();

    RenderTexture* filteredRadianceDeltaReflection = mWaveletCount > 0 ? waveletPingPongs2.at(2) : temporalAccumulationTextures.at(2);
    renderData.outputGPUHandleDictionary["gDeltaReflectionRadianceFiltered"] = filteredRadianceDeltaReflection->getGPUSrvHandler();

    RenderTexture* filteredRadianceDeltaTransmission = mWaveletCount > 0 ? waveletPingPongs2.at(3) : temporalAccumulationTextures.at(3);
    renderData.outputGPUHandleDictionary["gDeltaTransmissionRadianceFiltered"] = filteredRadianceDeltaTransmission->getGPUSrvHandler();


    for (int j = 0; j < 4; j++)
    {
        // update buffer
        std::swap(temporalAccumulationTexturesMomentAndLength.at(j), temporalAccumulationTexturesMomentAndLengthPrev.at(j));
        // std::swap(historyLengthRenderTexture, historyLengthRenderTexturePrev);

        // std::swap(temporalAccumulationTextureSpecularMoment, temporalAccumulationTextureSpecularMomentPrev);

        if (mFeedbackTap == 0)
        {
            std::swap(temporalAccumulationTextures.at(j), temporalAccumulationTexturesPrev.at(j));
            // std::swap(temporalAccumulationTextureSpecular, temporalAccumulationTextureSpecularPrev);
        }
    }
    
    return;
}