#include "RELAXPass.h"
#include <map>
#include "DX12Utils.h"
#include "DX12BufferUtils.h"

RELAXPass::RELAXPass(ID3D12Device5Ptr mpDevice, uvec2 size)
    : PostProcessPass(mpDevice, size)
{
    // Create Shaders
    std::vector<DXGI_FORMAT> rtvFormats = { DXGI_FORMAT_R32G32_FLOAT, DXGI_FORMAT_R32_FLOAT };
    this->motionVectorShader = new Shader(kQuadVertexShader, L"RenderPass/RELAX/RELAXMotionVector.hlsl", mpDevice, 6, rtvFormats);

    rtvFormats = { DXGI_FORMAT_R32G32B32A32_FLOAT, DXGI_FORMAT_R32G32_FLOAT };
    this->temporalAccumulationShader = new Shader(kQuadVertexShader, L"RenderPass/RELAX/RELAXTemporalAccumulation.hlsl", mpDevice, 5, rtvFormats);

    rtvFormats = { DXGI_FORMAT_R32G32B32A32_FLOAT };
    this->waveletShader = new Shader(kQuadVertexShader, L"RenderPass/RELAX/RELAXATrousWavelet.hlsl", mpDevice, 4, rtvFormats);

    //rtvFormats = { DXGI_FORMAT_R32G32B32A32_FLOAT, };
    //this->reconstructionShader = new Shader(kQuadVertexShader, L"RenderPass/RELAX/RELAXReconstruction.hlsl", mpDevice, 7, rtvFormats);

    rtvFormats = { DXGI_FORMAT_R32G32B32A32_FLOAT };
    this->varianceFilterShader = new Shader(kQuadVertexShader, L"RenderPass/RELAX/RELAXFilterVariance.hlsl", mpDevice, 6, rtvFormats);

    rtvFormats = { DXGI_FORMAT_R32G32_FLOAT };
    this->depthDerivativeShader = new Shader(kQuadVertexShader, L"RenderPass/RELAX/RELAXDepthDerivative.hlsl", mpDevice, 1, rtvFormats);

    for (int i = 0; i < maxWaveletCount; i++)
    {
        mRELAXParameterBuffers.push_back(createBuffer(mpDevice, sizeof(RELAXParameters), D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_GENERIC_READ, kUploadHeapProps));
    }

    param.diffuseMaxAccumulatedFrame = 32;
    param.specularMaxAccumulatedFrame = 32;
    param.sigmaP = 1.0f;
    param.sigmaN = 128.0f;
    param.sigmaL = 4.0f;
    param.screenSize = ivec2(size.x, size.y);
    defaultParam = param;
}

void RELAXPass::createRenderTextures(
    HeapData* rtvHeap,
    HeapData* srvHeap)
{
    depthDerivativeTexture = createRenderTexture(mpDevice, rtvHeap, srvHeap, size, DXGI_FORMAT_R32G32_FLOAT);
    motionVectorRenderTexture = createRenderTexture(mpDevice, rtvHeap, srvHeap, size, DXGI_FORMAT_R16G16_UNORM);
    historyLengthRenderTexture = createRenderTexture(mpDevice, rtvHeap, srvHeap, size, DXGI_FORMAT_R32_FLOAT);
    historyLengthRenderTexturePrev = createRenderTexture(mpDevice, rtvHeap, srvHeap, size, DXGI_FORMAT_R32_FLOAT);

    temporalAccumulationTextureDiffuse = createRenderTexture(mpDevice, rtvHeap, srvHeap, size, DXGI_FORMAT_R32G32B32A32_FLOAT);
    temporalAccumulationTextureSpecular = createRenderTexture(mpDevice, rtvHeap, srvHeap, size, DXGI_FORMAT_R32G32B32A32_FLOAT);
    temporalAccumulationTextureDiffusePrev = createRenderTexture(mpDevice, rtvHeap, srvHeap, size, DXGI_FORMAT_R32G32B32A32_FLOAT);
    temporalAccumulationTextureSpecularPrev = createRenderTexture(mpDevice, rtvHeap, srvHeap, size, DXGI_FORMAT_R32G32B32A32_FLOAT);
    temporalAccumulationTextureDiffuseVarianceFilter = createRenderTexture(mpDevice, rtvHeap, srvHeap, size, DXGI_FORMAT_R32G32B32A32_FLOAT);
    temporalAccumulationTextureSpecularVarianceFilter = createRenderTexture(mpDevice, rtvHeap, srvHeap, size, DXGI_FORMAT_R32G32B32A32_FLOAT);

    temporalAccumulationTextureDiffuseMoment = createRenderTexture(mpDevice, rtvHeap, srvHeap, size, DXGI_FORMAT_R32G32_FLOAT);
    temporalAccumulationTextureSpecularMoment = createRenderTexture(mpDevice, rtvHeap, srvHeap, size, DXGI_FORMAT_R32G32_FLOAT);
    temporalAccumulationTextureDiffuseMomentPrev = createRenderTexture(mpDevice, rtvHeap, srvHeap, size, DXGI_FORMAT_R32G32_FLOAT);
    temporalAccumulationTextureSpecularMomentPrev = createRenderTexture(mpDevice, rtvHeap, srvHeap, size, DXGI_FORMAT_R32G32_FLOAT);


    waveletDiffusePingPong1 = createRenderTexture(mpDevice, rtvHeap, srvHeap, size, DXGI_FORMAT_R32G32B32A32_FLOAT);
    waveletDiffusePingPong2 = createRenderTexture(mpDevice, rtvHeap, srvHeap, size, DXGI_FORMAT_R32G32B32A32_FLOAT);
    waveletSpecularPingPong1 = createRenderTexture(mpDevice, rtvHeap, srvHeap, size, DXGI_FORMAT_R32G32B32A32_FLOAT);
    waveletSpecularPingPong2 = createRenderTexture(mpDevice, rtvHeap, srvHeap, size, DXGI_FORMAT_R32G32B32A32_FLOAT);



    /*for (int i = 0; i < maxWaveletCount; i++) {
        RenderTexture* waveletDiffusei = createRenderTexture(mpDevice, rtvHeap, srvHeap, size, DXGI_FORMAT_R32G32B32A32_FLOAT);
        RenderTexture* waveletSpeculari = createRenderTexture(mpDevice, rtvHeap, srvHeap, size, DXGI_FORMAT_R32G32B32A32_FLOAT);
        this->waveletDiffuse.push_back(waveletDiffusei);
        this->waveletSpecular.push_back(waveletSpeculari);
    }*/

    // this->reconstructionRenderTexture = createRenderTexture(mpDevice, rtvHeap, srvHeap, size, DXGI_FORMAT_R32G32B32A32_FLOAT);
}

void RELAXPass::processGUI()
{

    mDirty = false;
    if (ImGui::CollapsingHeader("RELAX"))
    {
        mDirty |= ImGui::Checkbox("enable RELAX", &mEnabled);
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

void RELAXPass::uploadParams(uint32_t index)
{
    uint8_t* pData;
    d3d_call(mRELAXParameterBuffers.at(index)->Map(0, nullptr, (void**)&pData));
    memcpy(pData, &param, sizeof(RELAXParameters));
    mRELAXParameterBuffers.at(index)->Unmap(0, nullptr);
}

void RELAXPass::forward(RenderContext* pRenderContext, RenderData& renderData)
{
    ID3D12GraphicsCommandList4Ptr mpCmdList = pRenderContext->pCmdList;
    HeapData* pSrvUavHeap = pRenderContext->pSrvUavHeap;
    map<string, D3D12_GPU_DESCRIPTOR_HANDLE>& gpuHandles = renderData.gpuHandleDictionary;
    // map<string, D3D12_GPU_DESCRIPTOR_HANDLE>& gpuHandles = pSrvUavHeap->getGPUHandleMap();
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
    // Diffuse
    mpCmdList->SetPipelineState(temporalAccumulationShader->getPipelineStateObject());
    mpCmdList->SetGraphicsRootSignature(temporalAccumulationShader->getRootSignature()); // set the root signature

    mpCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(temporalAccumulationTextureDiffuse->mResource, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));
    mpCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(temporalAccumulationTextureDiffuseMoment->mResource, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

    D3D12_CPU_DESCRIPTOR_HANDLE handles[2] = { temporalAccumulationTextureDiffuse->mRtvDescriptorHandle, temporalAccumulationTextureDiffuseMoment->mRtvDescriptorHandle };
    mpCmdList->OMSetRenderTargets(2, handles, FALSE, nullptr);

    mpCmdList->SetGraphicsRootDescriptorTable(1, temporalAccumulationTextureDiffusePrev->getGPUSrvHandler());
    mpCmdList->SetGraphicsRootDescriptorTable(2, motionVectorRenderTexture->getGPUSrvHandler());
    mpCmdList->SetGraphicsRootDescriptorTable(3, gpuHandles.at("gDiffuseRadiance"));
    mpCmdList->SetGraphicsRootDescriptorTable(4, temporalAccumulationTextureDiffuseMomentPrev->getGPUSrvHandler());
    mpCmdList->SetGraphicsRootDescriptorTable(5, historyLengthRenderTexture->getGPUSrvHandler());
    this->uploadParams(0);
    mpCmdList->SetGraphicsRootConstantBufferView(0, mRELAXParameterBuffers[0]->GetGPUVirtualAddress());

    mpCmdList->DrawInstanced(6, 1, 0, 0);

    mpCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(temporalAccumulationTextureDiffuse->mResource, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));
    mpCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(temporalAccumulationTextureDiffuseMoment->mResource, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));


    // Specular
    mpCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(temporalAccumulationTextureSpecular->mResource, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));
    mpCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(temporalAccumulationTextureSpecularMoment->mResource, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

    D3D12_CPU_DESCRIPTOR_HANDLE handlesSpecular[2] = { temporalAccumulationTextureSpecular->mRtvDescriptorHandle, temporalAccumulationTextureSpecularMoment->mRtvDescriptorHandle };
    mpCmdList->OMSetRenderTargets(2, handlesSpecular, FALSE, nullptr);

    mpCmdList->SetGraphicsRootDescriptorTable(1, temporalAccumulationTextureSpecularPrev->getGPUSrvHandler());
    mpCmdList->SetGraphicsRootDescriptorTable(2, motionVectorRenderTexture->getGPUSrvHandler());
    mpCmdList->SetGraphicsRootDescriptorTable(3, gpuHandles.at("gSpecularRadiance"));
    mpCmdList->SetGraphicsRootDescriptorTable(4, temporalAccumulationTextureSpecularMomentPrev->getGPUSrvHandler());

    mpCmdList->DrawInstanced(6, 1, 0, 0);

    mpCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(temporalAccumulationTextureSpecular->mResource, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));
    mpCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(temporalAccumulationTextureSpecularMoment->mResource, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

    if (mEnableVarianceFilter) {
        // (2.5) Filter variance if length is <4
        // diffuse
        mpCmdList->SetPipelineState(varianceFilterShader->getPipelineStateObject());
        mpCmdList->SetGraphicsRootSignature(varianceFilterShader->getRootSignature()); // set the root signature

        mpCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(temporalAccumulationTextureDiffuseVarianceFilter->mResource, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

        mpCmdList->OMSetRenderTargets(1, &temporalAccumulationTextureDiffuseVarianceFilter->mRtvDescriptorHandle, FALSE, nullptr);

        mpCmdList->SetGraphicsRootDescriptorTable(1, temporalAccumulationTextureDiffuse->getGPUSrvHandler());
        // mpCmdList->SetGraphicsRootDescriptorTable(1, gpuHandles.at("gOutputHDR"));

        mpCmdList->SetGraphicsRootDescriptorTable(2, gpuHandles.at("gNormal"));
        mpCmdList->SetGraphicsRootDescriptorTable(3, gpuHandles.at("gPositionMeshID"));
        mpCmdList->SetGraphicsRootDescriptorTable(4, temporalAccumulationTextureDiffuseMoment->getGPUSrvHandler());
        mpCmdList->SetGraphicsRootDescriptorTable(5, historyLengthRenderTexture->getGPUSrvHandler());
        mpCmdList->SetGraphicsRootDescriptorTable(6, depthDerivativeTexture->getGPUSrvHandler());

        this->uploadParams(0);
        mpCmdList->SetGraphicsRootConstantBufferView(0, mRELAXParameterBuffers.at(0)->GetGPUVirtualAddress());

        mpCmdList->DrawInstanced(6, 1, 0, 0);
        mpCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(temporalAccumulationTextureDiffuseVarianceFilter->mResource, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));
        std::swap(temporalAccumulationTextureDiffuseVarianceFilter, temporalAccumulationTextureDiffuse);

        // specular
        mpCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(temporalAccumulationTextureSpecularVarianceFilter->mResource, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

        mpCmdList->OMSetRenderTargets(1, &temporalAccumulationTextureSpecularVarianceFilter->mRtvDescriptorHandle, FALSE, nullptr);

        mpCmdList->SetGraphicsRootDescriptorTable(1, temporalAccumulationTextureSpecular->getGPUSrvHandler());
        mpCmdList->SetGraphicsRootDescriptorTable(4, temporalAccumulationTextureSpecularMoment->getGPUSrvHandler());

        mpCmdList->DrawInstanced(6, 1, 0, 0);
        mpCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(temporalAccumulationTextureSpecularVarianceFilter->mResource, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));
        std::swap(temporalAccumulationTextureSpecularVarianceFilter, temporalAccumulationTextureSpecular);
    }

    // (3) Wavelet
    mpCmdList->SetPipelineState(waveletShader->getPipelineStateObject());
    mpCmdList->SetGraphicsRootSignature(waveletShader->getRootSignature()); // set the root signature

    mpCmdList->SetGraphicsRootDescriptorTable(2, gpuHandles.at("gNormal"));
    mpCmdList->SetGraphicsRootDescriptorTable(3, gpuHandles.at("gPositionMeshID"));
    mpCmdList->SetGraphicsRootDescriptorTable(4, depthDerivativeTexture->getGPUSrvHandler());


    // Diffuse
    for (int i = 0; i < waveletCount; i++) {
        mpCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(waveletDiffusePingPong1->mResource, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));
        mpCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(waveletDiffusePingPong2->mResource, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));

        mpCmdList->OMSetRenderTargets(1, &waveletDiffusePingPong1->mRtvDescriptorHandle, FALSE, nullptr);
        if (i == 0) {
            // mpCmdList->SetGraphicsRootDescriptorTable(1, gpuHandles.at("gOutputHDR"));
            mpCmdList->SetGraphicsRootDescriptorTable(1, temporalAccumulationTextureDiffuse->getGPUSrvHandler());
        }
        else {
            mpCmdList->SetGraphicsRootDescriptorTable(1, waveletDiffusePingPong2->getGPUSrvHandler());
        }
        param.stepSize = 1 << i;
        uploadParams(i);

        mpCmdList->SetGraphicsRootConstantBufferView(0, mRELAXParameterBuffers.at(i)->GetGPUVirtualAddress());

        mpCmdList->DrawInstanced(6, 1, 0, 0);
        mpCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(waveletDiffusePingPong1->mResource, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));
        mpCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(waveletDiffusePingPong2->mResource, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_PRESENT));

        if (i + 1 == mFeedbackTap)
        {
            resourceBarrier(mpCmdList, waveletDiffusePingPong1->mResource, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_COPY_SOURCE);
            resourceBarrier(mpCmdList, temporalAccumulationTextureDiffusePrev->mResource, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_COPY_DEST);
            mpCmdList->CopyResource(temporalAccumulationTextureDiffusePrev->mResource, waveletDiffusePingPong1->mResource);
            resourceBarrier(mpCmdList, waveletDiffusePingPong1->mResource, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_PRESENT);
        }

        std::swap(waveletDiffusePingPong1, waveletDiffusePingPong2);
    }

    // Specular
    for (int i = 0; i < waveletCount; i++) {
        mpCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(waveletSpecularPingPong1->mResource, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

        mpCmdList->OMSetRenderTargets(1, &waveletSpecularPingPong1->mRtvDescriptorHandle, FALSE, nullptr);
        if (i == 0) {
            mpCmdList->SetGraphicsRootDescriptorTable(1, temporalAccumulationTextureSpecular->getGPUSrvHandler());
        }
        else {
            mpCmdList->SetGraphicsRootDescriptorTable(1, waveletSpecularPingPong2->getGPUSrvHandler());
        }
        param.stepSize = 1 << i;
        uploadParams(i);

        mpCmdList->SetGraphicsRootConstantBufferView(0, mRELAXParameterBuffers.at(i)->GetGPUVirtualAddress());

        mpCmdList->DrawInstanced(6, 1, 0, 0);
        mpCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(waveletSpecularPingPong1->mResource, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

        if (i + 1 == mFeedbackTap)
        {
            resourceBarrier(mpCmdList, waveletSpecularPingPong1->mResource, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_COPY_SOURCE);
            resourceBarrier(mpCmdList, temporalAccumulationTextureSpecularPrev->mResource, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_COPY_DEST);
            mpCmdList->CopyResource(temporalAccumulationTextureSpecularPrev->mResource, waveletSpecularPingPong1->mResource);
            resourceBarrier(mpCmdList, waveletSpecularPingPong1->mResource, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_PRESENT);
        }

        std::swap(waveletSpecularPingPong1, waveletSpecularPingPong2);
    }

    RenderTexture* diffuseRenderTexture;
    RenderTexture* specularRenderTexture;

    if (waveletCount > 0)
    {
        diffuseRenderTexture = waveletDiffusePingPong2;
        specularRenderTexture = waveletSpecularPingPong2;
    }
    else
    {
        diffuseRenderTexture = temporalAccumulationTextureDiffuse;
        specularRenderTexture = temporalAccumulationTextureSpecular;
    }

    gpuHandles["gDiffuseRadiance"] = diffuseRenderTexture->getGPUSrvHandler();
    gpuHandles["gSpecularRadiance"] = specularRenderTexture->getGPUSrvHandler();


    // (4) Reconstruction!
    // mpCmdList->SetPipelineState(reconstructionShader->getPipelineStateObject());
    // mpCmdList->SetGraphicsRootSignature(reconstructionShader->getRootSignature()); // set the root signature

    

    /*mpCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(reconstructionRenderTexture->mResource, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

    mpCmdList->OMSetRenderTargets(1, &reconstructionRenderTexture->mRtvDescriptorHandle, FALSE, nullptr);

    mpCmdList->SetGraphicsRootDescriptorTable(1, diffuseRenderTexture->getGPUSrvHandler());
    mpCmdList->SetGraphicsRootDescriptorTable(2, specularRenderTexture->getGPUSrvHandler());
    mpCmdList->SetGraphicsRootDescriptorTable(3, gpuHandles.at("gDiffuseReflectance"));
    mpCmdList->SetGraphicsRootDescriptorTable(4, gpuHandles.at("gSpecularReflectance"));
    mpCmdList->SetGraphicsRootDescriptorTable(5, gpuHandles.at("gEmission"));
    mpCmdList->SetGraphicsRootDescriptorTable(6, gpuHandles.at("gDeltaReflectionRadiance"));
    mpCmdList->SetGraphicsRootDescriptorTable(7, gpuHandles.at("gDeltaTransmissionRadiance"));


    mpCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(reconstructionRenderTexture->mResource, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));
    mpCmdList->DrawInstanced(6, 1, 0, 0);*/


    // update buffer
    std::swap(historyLengthRenderTexture, historyLengthRenderTexturePrev);
    std::swap(temporalAccumulationTextureDiffuseMoment, temporalAccumulationTextureDiffuseMomentPrev);
    std::swap(temporalAccumulationTextureSpecularMoment, temporalAccumulationTextureSpecularMomentPrev);

    if (mFeedbackTap == 0)
    {
        std::swap(temporalAccumulationTextureDiffuse, temporalAccumulationTextureDiffusePrev);
        std::swap(temporalAccumulationTextureSpecular, temporalAccumulationTextureSpecularPrev);
    }
    return;
}