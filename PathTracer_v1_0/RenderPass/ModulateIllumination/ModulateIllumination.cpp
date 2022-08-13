#include "ModulateIllumination.h"

ModulateIllumination::ModulateIllumination(ID3D12Device5Ptr mpDevice, uvec2 size)
    : PostProcessPass(mpDevice, size)
{
    // Create Shader
    std::vector<DXGI_FORMAT> rtvFormats = { DXGI_FORMAT_R32G32B32A32_FLOAT };
    this->mpShader = new Shader(kQuadVertexShader, L"RenderPass/ModulateIllumination/ModulateIllumination.hlsl", mpDevice, 12, rtvFormats);

    mpParameterBuffer = createBuffer(mpDevice, sizeof(ModulateIlluminationParameters), D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_GENERIC_READ, kUploadHeapProps);

    enableDiffuseRadiance = true;
    enableDiffuseReflectance = true;
    enableSpecularRadiance = true;
    enableSpecularReflectance = true;
    enableEmission = true;

    enableDeltaReflectionRadiance = true;
    enableDeltaReflectionReflectance = true;
    enableDeltaReflectionEmission = true;

    enableDeltaTransmissionRadiance = true;
    enableDeltaTransmissionReflectance = true;
    enableDeltaTransmissionEmission = true;

    enableResidualRadiance = true;
}

void ModulateIllumination::createRenderTextures(
    HeapData* rtvHeap,
    HeapData* srvHeap)
{
    blendRenderTexture = createRenderTexture(mpDevice, rtvHeap, srvHeap, size, DXGI_FORMAT_R32G32B32A32_FLOAT);
}

void ModulateIllumination::processGUI()
{
    if (ImGui::CollapsingHeader("ModulateIllumination"))
    {
        ImGui::Checkbox("DiffuseReflectance", &enableDiffuseReflectance);
        ImGui::Checkbox("DiffuseRadiance", &enableDiffuseRadiance);
        ImGui::Checkbox("SpecularReflectance", &enableSpecularReflectance);
        ImGui::Checkbox("SpecularRadiance", &enableSpecularRadiance);
        ImGui::Checkbox("Emission", &enableEmission);

        ImGui::Checkbox("DeltaReflectionRadiance", &enableDeltaReflectionRadiance);
        ImGui::Checkbox("DeltaReflectionReflectance", &enableDeltaReflectionReflectance);
        ImGui::Checkbox("DeltaReflectionEmission", &enableDeltaReflectionEmission);

        ImGui::Checkbox("DeltaTransmissionRadiance", &enableDeltaTransmissionRadiance);
        ImGui::Checkbox("DeltaTransmissionReflectance", &enableDeltaTransmissionReflectance);
        ImGui::Checkbox("DeltaTransmissionEmission", &enableDeltaTransmissionEmission);

        ImGui::Checkbox("ResidualRadiance", &enableResidualRadiance);
    }
}

void ModulateIllumination::forward(RenderContext* pRenderContext, RenderData& renderData)
{
    ID3D12GraphicsCommandList4Ptr mpCmdList = pRenderContext->pCmdList;
    HeapData* pSrvUavHeap = pRenderContext->pSrvUavHeap;
    map<string, D3D12_GPU_DESCRIPTOR_HANDLE>& gpuHandles = renderData.gpuHandleDictionary;
    // map<string, D3D12_GPU_DESCRIPTOR_HANDLE>& gpuHandles = pSrvUavHeap->getGPUHandleMap();
    this->setViewPort(mpCmdList);

    mpCmdList->SetPipelineState(mpShader->getPipelineStateObject());
    mpCmdList->SetGraphicsRootSignature(mpShader->getRootSignature()); // set the root signature

    mpCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(blendRenderTexture->mResource, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

    mpCmdList->OMSetRenderTargets(1, &blendRenderTexture->mRtvDescriptorHandle, FALSE, nullptr);

    uploadParams();
    mpCmdList->SetGraphicsRootConstantBufferView(0, this->mpParameterBuffer->GetGPUVirtualAddress());

    mpCmdList->SetGraphicsRootDescriptorTable(1, gpuHandles.at("gDiffuseRadiance"));
    mpCmdList->SetGraphicsRootDescriptorTable(2, gpuHandles.at("gSpecularRadiance"));
    mpCmdList->SetGraphicsRootDescriptorTable(3, gpuHandles.at("gDiffuseReflectance"));
    mpCmdList->SetGraphicsRootDescriptorTable(4, gpuHandles.at("gSpecularReflectance"));
    mpCmdList->SetGraphicsRootDescriptorTable(5, gpuHandles.at("gEmission"));

    mpCmdList->SetGraphicsRootDescriptorTable(6, gpuHandles.at("gDeltaReflectionRadiance"));
    mpCmdList->SetGraphicsRootDescriptorTable(7, gpuHandles.at("gDeltaTransmissionRadiance"));
    mpCmdList->SetGraphicsRootDescriptorTable(8, gpuHandles.at("gDeltaReflectionReflectance"));
    mpCmdList->SetGraphicsRootDescriptorTable(9, gpuHandles.at("gDeltaTransmissionReflectance"));
    mpCmdList->SetGraphicsRootDescriptorTable(10, gpuHandles.at("gDeltaReflectionEmission"));
    mpCmdList->SetGraphicsRootDescriptorTable(11, gpuHandles.at("gDeltaTransmissionEmission"));
    mpCmdList->SetGraphicsRootDescriptorTable(12, gpuHandles.at("gResidualRadiance"));

    mpCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(blendRenderTexture->mResource, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));
    mpCmdList->DrawInstanced(6, 1, 0, 0);
}

void ModulateIllumination::uploadParams()
{
    mParam.enableDiffuseReflectance = enableDiffuseReflectance;
    mParam.enableDiffuseRadiance = enableDiffuseRadiance;
    mParam.enableSpecularReflectance = enableSpecularReflectance;
    mParam.enableSpecularRadiance = enableSpecularRadiance;
    mParam.enableEmission = enableEmission;

    mParam.enableDeltaReflectionRadiance = enableDeltaReflectionRadiance;
    mParam.enableDeltaReflectionReflectance = enableDeltaReflectionReflectance;
    mParam.enableDeltaReflectionEmission = enableDeltaReflectionEmission;
    mParam.enableDeltaTransmissionRadiance = enableDeltaTransmissionRadiance;
    mParam.enableDeltaTransmissionReflectance = enableDeltaTransmissionReflectance;
    mParam.enableDeltaTransmissionEmission = enableDeltaTransmissionEmission;

    mParam.enableResidualRadiance = enableResidualRadiance;

    uint8_t* pData;
    d3d_call(mpParameterBuffer->Map(0, nullptr, (void**)&pData));
    memcpy(pData, &mParam, sizeof(ModulateIlluminationParameters));
    mpParameterBuffer->Unmap(0, nullptr);
}

