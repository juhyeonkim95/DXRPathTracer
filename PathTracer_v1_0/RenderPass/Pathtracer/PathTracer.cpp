#include "PathTracer.h"
#include <string>
#include "DX12Initializer.h"
#include "DX12Helper.h"

PathTracer::PathTracer(ID3D12Device5Ptr mpDevice)
{
    this->mpDevice = mpDevice;
    
    param.maxDepthDiffuse = 3;
    param.maxDepthSpecular = 3;
    param.maxDepthTransmittance = 10;
    param.accumulate = false;
    defaultParam = param;
}

void PathTracer::processGUI()
{
    mDirty = false;
    if (ImGui::CollapsingHeader("PathTracer"))
    {
        mDirty |= ImGui::SliderInt("Max Depth - diffuse", &param.maxDepthDiffuse, 1, 16);
        mDirty |= ImGui::SliderInt("Max Depth - specular", &param.maxDepthSpecular, 1, 16);
        mDirty |= ImGui::SliderInt("Max Depth - transmittance", &param.maxDepthTransmittance, 1, 16);
        mDirty |= ImGui::Checkbox("Accumulate", &param.accumulate);
    }
}