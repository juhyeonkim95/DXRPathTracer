#include "NRDDeltaReflection.h"
#include <map>
#include "DX12Utils.h"
#include "DX12BufferUtils.h"

NRDDeltaReflection::NRDDeltaReflection(ID3D12Device5Ptr mpDevice, uvec2 size)
    : PostProcessPass(mpDevice, size)
{
    // Create Shaders
    std::vector<DXGI_FORMAT> rtvFormats = { DXGI_FORMAT_R32G32_FLOAT, DXGI_FORMAT_R32_FLOAT };
    this->motionVectorShader = new Shader(kQuadVertexShader, L"RenderPass/NRDDeltaReflection/NRDDeltaReflectionMotionVector.hlsl", mpDevice, 6, rtvFormats);

    rtvFormats = { DXGI_FORMAT_R32G32B32A32_FLOAT, DXGI_FORMAT_R32G32_FLOAT };
    this->temporalAccumulationShader = new Shader(kQuadVertexShader, L"RenderPass/NRDDeltaReflection/NRDDeltaReflectionTemporalAccumulation.hlsl", mpDevice, 5, rtvFormats);

    rtvFormats = { DXGI_FORMAT_R32G32B32A32_FLOAT };
    this->waveletShader = new Shader(kQuadVertexShader, L"RenderPass/NRDDeltaReflection/NRDDeltaReflectionATrousWavelet.hlsl", mpDevice, 5, rtvFormats);

    //rtvFormats = { DXGI_FORMAT_R32G32B32A32_FLOAT, };
    //this->reconstructionShader = new Shader(kQuadVertexShader, L"RenderPass/NRDDeltaReflection/NRDDeltaReflectionReconstruction.hlsl", mpDevice, 7, rtvFormats);

    rtvFormats = { DXGI_FORMAT_R32G32B32A32_FLOAT };
    this->varianceFilterShader = new Shader(kQuadVertexShader, L"RenderPass/NRDDeltaReflection/NRDDeltaReflectionFilterVariance.hlsl", mpDevice, 6, rtvFormats);

    rtvFormats = { DXGI_FORMAT_R32G32_FLOAT };
    this->depthDerivativeShader = new Shader(kQuadVertexShader, L"RenderPass/NRDDeltaReflection/NRDDeltaReflectionDepthDerivative.hlsl", mpDevice, 1, rtvFormats);

    for (int i = 0; i < maxWaveletCount; i++)
    {
        mNRDDeltaReflectionParameterBuffers.push_back(createBuffer(mpDevice, sizeof(NRDDeltaReflectionParameters), D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_GENERIC_READ, kUploadHeapProps));
    }

    param.diffuseMaxAccumulatedFrame = 32;
    param.specularMaxAccumulatedFrame = 32;
    param.sigmaP = 1.0f;
    param.sigmaN = 128.0f;
    param.sigmaL = 4.0f;
    param.screenSize = ivec2(size.x, size.y);
    defaultParam = param;
}

void NRDDeltaReflection::createRenderTextures(
    HeapData* rtvHeap,
    HeapData* srvHeap)
{
    depthDerivativeTexture = createRenderTexture(mpDevice, rtvHeap, srvHeap, size, DXGI_FORMAT_R32G32_FLOAT);
    motionVectorRenderTexture = createRenderTexture(mpDevice, rtvHeap, srvHeap, size, DXGI_FORMAT_R16G16_UNORM);
    historyLengthRenderTexture = createRenderTexture(mpDevice, rtvHeap, srvHeap, size, DXGI_FORMAT_R32_FLOAT);
    historyLengthRenderTexturePrev = createRenderTexture(mpDevice, rtvHeap, srvHeap, size, DXGI_FORMAT_R32_FLOAT);

    temporalAccumulationTextureDeltaReflection = createRenderTexture(mpDevice, rtvHeap, srvHeap, size, DXGI_FORMAT_R32G32B32A32_FLOAT);
    temporalAccumulationTextureDeltaTransmission = createRenderTexture(mpDevice, rtvHeap, srvHeap, size, DXGI_FORMAT_R32G32B32A32_FLOAT);
    temporalAccumulationTextureDeltaReflectionPrev = createRenderTexture(mpDevice, rtvHeap, srvHeap, size, DXGI_FORMAT_R32G32B32A32_FLOAT);
    temporalAccumulationTextureDeltaTransmissionPrev = createRenderTexture(mpDevice, rtvHeap, srvHeap, size, DXGI_FORMAT_R32G32B32A32_FLOAT);
    temporalAccumulationTextureDeltaReflectionVarianceFilter = createRenderTexture(mpDevice, rtvHeap, srvHeap, size, DXGI_FORMAT_R32G32B32A32_FLOAT);
    temporalAccumulationTextureDeltaTransmissionVarianceFilter = createRenderTexture(mpDevice, rtvHeap, srvHeap, size, DXGI_FORMAT_R32G32B32A32_FLOAT);

    temporalAccumulationTextureDeltaReflectionMoment = createRenderTexture(mpDevice, rtvHeap, srvHeap, size, DXGI_FORMAT_R32G32_FLOAT);
    temporalAccumulationTextureDeltaTransmissionMoment = createRenderTexture(mpDevice, rtvHeap, srvHeap, size, DXGI_FORMAT_R32G32_FLOAT);
    temporalAccumulationTextureDeltaReflectionMomentPrev = createRenderTexture(mpDevice, rtvHeap, srvHeap, size, DXGI_FORMAT_R32G32_FLOAT);
    temporalAccumulationTextureDeltaTransmissionMomentPrev = createRenderTexture(mpDevice, rtvHeap, srvHeap, size, DXGI_FORMAT_R32G32_FLOAT);


    waveletDeltaReflectionPingPong1 = createRenderTexture(mpDevice, rtvHeap, srvHeap, size, DXGI_FORMAT_R32G32B32A32_FLOAT);
    waveletDeltaReflectionPingPong2 = createRenderTexture(mpDevice, rtvHeap, srvHeap, size, DXGI_FORMAT_R32G32B32A32_FLOAT);
    waveletDeltaTransmissionPingPong1 = createRenderTexture(mpDevice, rtvHeap, srvHeap, size, DXGI_FORMAT_R32G32B32A32_FLOAT);
    waveletDeltaTransmissionPingPong2 = createRenderTexture(mpDevice, rtvHeap, srvHeap, size, DXGI_FORMAT_R32G32B32A32_FLOAT);



    /*for (int i = 0; i < maxWaveletCount; i++) {
        RenderTexture* waveletDeltaReflectioni = createRenderTexture(mpDevice, rtvHeap, srvHeap, size, DXGI_FORMAT_R32G32B32A32_FLOAT);
        RenderTexture* waveletDeltaTransmissioni = createRenderTexture(mpDevice, rtvHeap, srvHeap, size, DXGI_FORMAT_R32G32B32A32_FLOAT);
        this->waveletDeltaReflection.push_back(waveletDeltaReflectioni);
        this->waveletDeltaTransmission.push_back(waveletDeltaTransmissioni);
    }*/

    // this->reconstructionRenderTexture = createRenderTexture(mpDevice, rtvHeap, srvHeap, size, DXGI_FORMAT_R32G32B32A32_FLOAT);
}

void NRDDeltaReflection::processGUI()
{

    mDirty = false;
    if (ImGui::CollapsingHeader("NRDDeltaReflection"))
    {
        mDirty |= ImGui::Checkbox("enable NRDDeltaReflection", &mEnabled);
        mDirty |= ImGui::Checkbox("enableVarianceFilter", &mEnableVarianceFilter);

        mDirty |= ImGui::SliderFloat("sigmaP", &param.sigmaP, 0.01f, 16.0f);
        mDirty |= ImGui::SliderFloat("sigmaN", &param.sigmaN, 0.01f, 256.0f);
        mDirty |= ImGui::SliderFloat("sigmaL", &param.sigmaL, 0.01f, 16.0f);
        mDirty |= ImGui::SliderInt("diffuseMaxAccumulatedFrame", &param.diffuseMaxAccumulatedFrame, 1, 64);
        mDirty |= ImGui::SliderInt("specularMaxAccumulatedFrame", &param.specularMaxAccumulatedFrame, 1, 64);
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

void NRDDeltaReflection::uploadParams(uint32_t index)
{
    uint8_t* pData;
    d3d_call(mNRDDeltaReflectionParameterBuffers.at(index)->Map(0, nullptr, (void**)&pData));
    memcpy(pData, &param, sizeof(NRDDeltaReflectionParameters));
    mNRDDeltaReflectionParameterBuffers.at(index)->Unmap(0, nullptr);
}

void NRDDeltaReflection::forward(RenderContext* pRenderContext, RenderData& renderData)
{
    ID3D12GraphicsCommandList4Ptr mpCmdList = pRenderContext->pCmdList;
    HeapData* pSrvUavHeap = pRenderContext->pSrvUavHeap;
    map<string, D3D12_GPU_DESCRIPTOR_HANDLE>& gpuHandles = renderData.gpuHandleDictionary; //pSrvUavHeap->getGPUHandleMap();
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
    // DeltaReflection
    mpCmdList->SetPipelineState(temporalAccumulationShader->getPipelineStateObject());
    mpCmdList->SetGraphicsRootSignature(temporalAccumulationShader->getRootSignature()); // set the root signature

    mpCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(temporalAccumulationTextureDeltaReflection->mResource, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));
    mpCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(temporalAccumulationTextureDeltaReflectionMoment->mResource, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

    D3D12_CPU_DESCRIPTOR_HANDLE handles[2] = { temporalAccumulationTextureDeltaReflection->mRtvDescriptorHandle, temporalAccumulationTextureDeltaReflectionMoment->mRtvDescriptorHandle };
    mpCmdList->OMSetRenderTargets(2, handles, FALSE, nullptr);

    mpCmdList->SetGraphicsRootDescriptorTable(1, temporalAccumulationTextureDeltaReflectionPrev->getGPUSrvHandler());
    mpCmdList->SetGraphicsRootDescriptorTable(2, motionVectorRenderTexture->getGPUSrvHandler());
    mpCmdList->SetGraphicsRootDescriptorTable(3, gpuHandles.at("gDeltaReflectionRadiance"));
    mpCmdList->SetGraphicsRootDescriptorTable(4, temporalAccumulationTextureDeltaReflectionMomentPrev->getGPUSrvHandler());
    mpCmdList->SetGraphicsRootDescriptorTable(5, historyLengthRenderTexture->getGPUSrvHandler());
    this->uploadParams(0);
    mpCmdList->SetGraphicsRootConstantBufferView(0, mNRDDeltaReflectionParameterBuffers[0]->GetGPUVirtualAddress());

    mpCmdList->DrawInstanced(6, 1, 0, 0);

    mpCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(temporalAccumulationTextureDeltaReflection->mResource, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));
    mpCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(temporalAccumulationTextureDeltaReflectionMoment->mResource, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));


    // DeltaTransmission
    mpCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(temporalAccumulationTextureDeltaTransmission->mResource, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));
    mpCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(temporalAccumulationTextureDeltaTransmissionMoment->mResource, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

    D3D12_CPU_DESCRIPTOR_HANDLE handlesDeltaTransmission[2] = { temporalAccumulationTextureDeltaTransmission->mRtvDescriptorHandle, temporalAccumulationTextureDeltaTransmissionMoment->mRtvDescriptorHandle };
    mpCmdList->OMSetRenderTargets(2, handlesDeltaTransmission, FALSE, nullptr);

    mpCmdList->SetGraphicsRootDescriptorTable(1, temporalAccumulationTextureDeltaTransmissionPrev->getGPUSrvHandler());
    mpCmdList->SetGraphicsRootDescriptorTable(2, motionVectorRenderTexture->getGPUSrvHandler());
    mpCmdList->SetGraphicsRootDescriptorTable(3, gpuHandles.at("gDeltaTransmissionRadiance"));
    mpCmdList->SetGraphicsRootDescriptorTable(4, temporalAccumulationTextureDeltaTransmissionMomentPrev->getGPUSrvHandler());

    mpCmdList->DrawInstanced(6, 1, 0, 0);

    mpCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(temporalAccumulationTextureDeltaTransmission->mResource, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));
    mpCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(temporalAccumulationTextureDeltaTransmissionMoment->mResource, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

    if (mEnableVarianceFilter) {
        // (2.5) Filter variance if length is <4
        // diffuse
        mpCmdList->SetPipelineState(varianceFilterShader->getPipelineStateObject());
        mpCmdList->SetGraphicsRootSignature(varianceFilterShader->getRootSignature()); // set the root signature

        mpCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(temporalAccumulationTextureDeltaReflectionVarianceFilter->mResource, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

        mpCmdList->OMSetRenderTargets(1, &temporalAccumulationTextureDeltaReflectionVarianceFilter->mRtvDescriptorHandle, FALSE, nullptr);

        mpCmdList->SetGraphicsRootDescriptorTable(1, temporalAccumulationTextureDeltaReflection->getGPUSrvHandler());
        // mpCmdList->SetGraphicsRootDescriptorTable(1, gpuHandles.at("gOutputHDR"));

        mpCmdList->SetGraphicsRootDescriptorTable(2, gpuHandles.at("gNormal"));
        mpCmdList->SetGraphicsRootDescriptorTable(3, gpuHandles.at("gPositionMeshID"));
        mpCmdList->SetGraphicsRootDescriptorTable(4, temporalAccumulationTextureDeltaReflectionMoment->getGPUSrvHandler());
        mpCmdList->SetGraphicsRootDescriptorTable(5, historyLengthRenderTexture->getGPUSrvHandler());
        mpCmdList->SetGraphicsRootDescriptorTable(6, depthDerivativeTexture->getGPUSrvHandler());

        this->uploadParams(0);
        mpCmdList->SetGraphicsRootConstantBufferView(0, mNRDDeltaReflectionParameterBuffers.at(0)->GetGPUVirtualAddress());

        mpCmdList->DrawInstanced(6, 1, 0, 0);
        mpCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(temporalAccumulationTextureDeltaReflectionVarianceFilter->mResource, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));
        std::swap(temporalAccumulationTextureDeltaReflectionVarianceFilter, temporalAccumulationTextureDeltaReflection);

        // specular
        mpCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(temporalAccumulationTextureDeltaTransmissionVarianceFilter->mResource, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

        mpCmdList->OMSetRenderTargets(1, &temporalAccumulationTextureDeltaTransmissionVarianceFilter->mRtvDescriptorHandle, FALSE, nullptr);

        mpCmdList->SetGraphicsRootDescriptorTable(1, temporalAccumulationTextureDeltaTransmission->getGPUSrvHandler());
        mpCmdList->SetGraphicsRootDescriptorTable(4, temporalAccumulationTextureDeltaTransmissionMoment->getGPUSrvHandler());

        mpCmdList->DrawInstanced(6, 1, 0, 0);
        mpCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(temporalAccumulationTextureDeltaTransmissionVarianceFilter->mResource, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));
        std::swap(temporalAccumulationTextureDeltaTransmissionVarianceFilter, temporalAccumulationTextureDeltaTransmission);
    }

    // (3) Wavelet
    mpCmdList->SetPipelineState(waveletShader->getPipelineStateObject());
    mpCmdList->SetGraphicsRootSignature(waveletShader->getRootSignature()); // set the root signature

    mpCmdList->SetGraphicsRootDescriptorTable(2, gpuHandles.at("gNormal"));
    mpCmdList->SetGraphicsRootDescriptorTable(3, gpuHandles.at("gPositionMeshID"));
    mpCmdList->SetGraphicsRootDescriptorTable(4, depthDerivativeTexture->getGPUSrvHandler());
    mpCmdList->SetGraphicsRootDescriptorTable(5, gpuHandles.at("gPathType"));


    // DeltaReflection
    for (int i = 0; i < waveletCount; i++) {
        mpCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(waveletDeltaReflectionPingPong1->mResource, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));
        mpCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(waveletDeltaReflectionPingPong2->mResource, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));

        mpCmdList->OMSetRenderTargets(1, &waveletDeltaReflectionPingPong1->mRtvDescriptorHandle, FALSE, nullptr);
        if (i == 0) {
            // mpCmdList->SetGraphicsRootDescriptorTable(1, gpuHandles.at("gOutputHDR"));
            mpCmdList->SetGraphicsRootDescriptorTable(1, temporalAccumulationTextureDeltaReflection->getGPUSrvHandler());
        }
        else {
            mpCmdList->SetGraphicsRootDescriptorTable(1, waveletDeltaReflectionPingPong2->getGPUSrvHandler());
        }
        param.stepSize = 1 << i;
        uploadParams(i);

        mpCmdList->SetGraphicsRootConstantBufferView(0, mNRDDeltaReflectionParameterBuffers.at(i)->GetGPUVirtualAddress());

        mpCmdList->DrawInstanced(6, 1, 0, 0);
        mpCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(waveletDeltaReflectionPingPong1->mResource, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));
        mpCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(waveletDeltaReflectionPingPong2->mResource, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_PRESENT));

        if (i + 1 == mFeedbackTap)
        {
            resourceBarrier(mpCmdList, waveletDeltaReflectionPingPong1->mResource, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_COPY_SOURCE);
            resourceBarrier(mpCmdList, temporalAccumulationTextureDeltaReflectionPrev->mResource, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_COPY_DEST);
            mpCmdList->CopyResource(temporalAccumulationTextureDeltaReflectionPrev->mResource, waveletDeltaReflectionPingPong1->mResource);
            resourceBarrier(mpCmdList, waveletDeltaReflectionPingPong1->mResource, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_PRESENT);
        }

        std::swap(waveletDeltaReflectionPingPong1, waveletDeltaReflectionPingPong2);
    }

    // DeltaTransmission
    for (int i = 0; i < waveletCount; i++) {
        mpCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(waveletDeltaTransmissionPingPong1->mResource, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

        mpCmdList->OMSetRenderTargets(1, &waveletDeltaTransmissionPingPong1->mRtvDescriptorHandle, FALSE, nullptr);
        if (i == 0) {
            mpCmdList->SetGraphicsRootDescriptorTable(1, temporalAccumulationTextureDeltaTransmission->getGPUSrvHandler());
        }
        else {
            mpCmdList->SetGraphicsRootDescriptorTable(1, waveletDeltaTransmissionPingPong2->getGPUSrvHandler());
        }
        param.stepSize = 1 << i;
        uploadParams(i);

        mpCmdList->SetGraphicsRootConstantBufferView(0, mNRDDeltaReflectionParameterBuffers.at(i)->GetGPUVirtualAddress());

        mpCmdList->DrawInstanced(6, 1, 0, 0);
        mpCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(waveletDeltaTransmissionPingPong1->mResource, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

        if (i + 1 == mFeedbackTap)
        {
            resourceBarrier(mpCmdList, waveletDeltaTransmissionPingPong1->mResource, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_COPY_SOURCE);
            resourceBarrier(mpCmdList, temporalAccumulationTextureDeltaTransmissionPrev->mResource, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_COPY_DEST);
            mpCmdList->CopyResource(temporalAccumulationTextureDeltaTransmissionPrev->mResource, waveletDeltaTransmissionPingPong1->mResource);
            resourceBarrier(mpCmdList, waveletDeltaTransmissionPingPong1->mResource, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_PRESENT);
        }

        std::swap(waveletDeltaTransmissionPingPong1, waveletDeltaTransmissionPingPong2);
    }

    RenderTexture* deltaReflectionRenderTexture;
    RenderTexture* deltaTransmissionRenderTexture;

    if (waveletCount > 0)
    {
        deltaReflectionRenderTexture = waveletDeltaReflectionPingPong2;
        deltaTransmissionRenderTexture = waveletDeltaTransmissionPingPong2;
    }
    else
    {
        deltaReflectionRenderTexture = temporalAccumulationTextureDeltaReflection;
        deltaTransmissionRenderTexture = temporalAccumulationTextureDeltaTransmission;
    }

    gpuHandles["gDeltaReflectionRadiance"] = deltaReflectionRenderTexture->getGPUSrvHandler();
    gpuHandles["gDeltaTransmissionRadiance"] = deltaTransmissionRenderTexture->getGPUSrvHandler();

    // update buffer
    std::swap(historyLengthRenderTexture, historyLengthRenderTexturePrev);
    std::swap(temporalAccumulationTextureDeltaReflectionMoment, temporalAccumulationTextureDeltaReflectionMomentPrev);
    std::swap(temporalAccumulationTextureDeltaTransmissionMoment, temporalAccumulationTextureDeltaTransmissionMomentPrev);

    if (mFeedbackTap == 0)
    {
        std::swap(temporalAccumulationTextureDeltaReflection, temporalAccumulationTextureDeltaReflectionPrev);
        std::swap(temporalAccumulationTextureDeltaTransmission, temporalAccumulationTextureDeltaTransmissionPrev);
    }
    return;
}