#include "ReSTIR.h"
#include <string>
#include "DX12Initializer.h"
#include "DX12Helper.h"

ReSTIR::ReSTIR(ID3D12Device5Ptr mpDevice)
{
    this->mpDevice = mpDevice;
    mParamBuffer = createBuffer(mpDevice, sizeof(ReSTIRParameters), D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_GENERIC_READ, kUploadHeapProps);

    param.lightCandidateCount = 4;
    param.maxHistoryLength = 20;
    param.normalThreshold = 0.5;
    param.depthThreshold = 0.1;
    param.resamplingMode = ReSTIR_MODE_NO_REUSE;
}

void ReSTIR::processGUI()
{
    if (ImGui::CollapsingHeader("ReSTIR"))
    {
        ImGui::Checkbox("enable", &mEnabled);
        
        const char* items[] = { "No Resampling", "Temporal reuse only", "Spatial reuse only", "Spatiotemporal reuse"};
        ImGui::Combo("ReSTIR Mode", &param.resamplingMode, items, IM_ARRAYSIZE(items), 4);
        
        ImGui::SliderInt("Light samples", &param.lightCandidateCount, 1, 5);
        ImGui::SliderInt("Max History Length", &param.maxHistoryLength, 1, 32);
        ImGui::SliderFloat("Normal Threshold", &param.normalThreshold, 0.1f, 1.0f);
        ImGui::SliderFloat("Depth Threshold", &param.depthThreshold, 0.1f, 1.0f);
    }
    param.enabled = mEnabled;

    this->uploadParams();
}


void ReSTIR::uploadParams()
{
    uint8_t* pData;
    d3d_call(mParamBuffer->Map(0, nullptr, (void**)&pData));
    memcpy(pData, &param, sizeof(ReSTIRParameters));
    mParamBuffer->Unmap(0, nullptr);
}